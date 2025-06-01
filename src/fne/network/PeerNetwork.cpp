// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/network/json/json.h"
#include "common/zlib/Compression.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "fne/network/PeerNetwork.h"

using namespace network;
using namespace compress;

#include <cstdio>
#include <cassert>
#include <algorithm>
#include <fstream>
#include <streambuf>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define WORKER_CNT 8U

const uint64_t PACKET_LATE_TIME = 200U; // 200ms

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PeerNetwork class. */

PeerNetwork::PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    Network(address, port, localPort, peerId, password, duplex, debug, dmr, p25, nxdn, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, updateLookup, saveLookup),
    m_attachedKeyRSPHandler(false),
    m_blockTrafficToTable(),
    m_pidLookup(nullptr),
    m_peerLink(false),
    m_peerLinkSavesACL(false),
    m_tgidPkt(true, "Peer-Link, TGID List"),
    m_ridPkt(true, "Peer-Link, RID List"),
    m_pidPkt(true, "Peer-Link, PID List"),
    m_threadPool(WORKER_CNT, "peer")
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    // ignore the source peer ID for packets
    m_promiscuousPeer = true;

    // never disable peer network services on ACL NAK from master
    m_neverDisableOnACLNAK = true;

    // FNE peer network manually handle protocol packets
    m_userHandleProtocol = true;
}

/* Sets the instances of the Peer List lookup tables. */

void PeerNetwork::setPeerLookups(lookups::PeerListLookup* pidLookup)
{
    m_pidLookup = pidLookup;
}

/* Opens connection to the network. */

bool PeerNetwork::open()
{
    if (!m_enabled)
        return false;

    // start thread pool
    m_threadPool.start();

    return Network::open();
}

/* Closes connection to the network. */

void PeerNetwork::close()
{
    // stop thread pool
    m_threadPool.stop();
    m_threadPool.wait();

    Network::close();
}

/* Checks if the passed peer ID is blocked from sending to this peer. */

bool PeerNetwork::checkBlockedPeer(uint32_t peerId)
{
    if (!m_enabled)
        return false;

    if (m_blockTrafficToTable.empty())
        return false;

    if (std::find(m_blockTrafficToTable.begin(), m_blockTrafficToTable.end(), peerId) != m_blockTrafficToTable.end()) {
        if (m_debug) {
            ::LogDebugEx(LOG_HOST, "PeerNetwork::checkBlockedPeer()", "PEER %u peerId = %u, blocking traffic", m_peerId, peerId);
        }
        return true;
    }

    if (m_debug) {
        ::LogDebugEx(LOG_HOST, "PeerNetwork::checkBlockedPeer()", "PEER %u peerId = %u, passing traffic", m_peerId, peerId);
    }
    return false;
}

/* Writes a complete update of this CFNE's active peer list to the network. */

bool PeerNetwork::writePeerLinkPeers(json::array* peerList)
{
    if (peerList == nullptr)
        return false;
    if (peerList->size() == 0)
        return false;

    if (peerList->size() > 0 && m_peerLink) {
        json::value v = json::value(*peerList);
        std::string json = std::string(v.serialize());

        size_t len = json.length() + 9U;
        DECLARE_CHAR_ARRAY(buffer, len);

        ::memcpy(buffer + 0U, TAG_PEER_LINK, 4U);
        ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

        PacketBuffer pkt(true, "Peer-Link, Active Peer List");
        pkt.encode((uint8_t*)buffer, len);

        uint32_t streamId = createStreamId();
        LogInfoEx(LOG_NET, "PEER %u Peer-Link, Active Peer List, blocks %u, streamId = %u", m_peerId, pkt.fragments.size(), streamId);
        if (pkt.fragments.size() > 0U) {
            for (auto frag : pkt.fragments) {
                writeMaster({ NET_FUNC::PEER_LINK, NET_SUBFUNC::PL_ACT_PEER_LIST }, 
                    frag.second->data, FRAG_SIZE, RTP_END_OF_CALL_SEQ, streamId, false, true);
                Thread::sleep(60U); // pace block transmission
            }
        }

        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* User overrideable handler that allows user code to process network packets not handled by this class. */

void PeerNetwork::userPacketHandler(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint32_t streamId, 
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader)
{
    switch (opcode.first) {
    case NET_FUNC::PROTOCOL:                                        // Protocol
        {
            PeerPacketRequest* req = new PeerPacketRequest();
            req->obj = this;
            req->peerId = peerId;
            req->streamId = streamId;

            req->rtpHeader = rtpHeader;
            req->fneHeader = fneHeader;

            req->subFunc = opcode.second;

            req->pktRxTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            req->length = length;
            req->buffer = new uint8_t[length];
            ::memcpy(req->buffer, data, length);

            // enqueue the task
            if (!m_threadPool.enqueue(new_pooltask(taskNetworkRx, req))) {
                LogError(LOG_NET, "Failed to task enqueue network packet request, peerId = %u", peerId);
                if (req != nullptr) {
                    if (req->buffer != nullptr)
                        delete[] req->buffer;
                    delete req;
                }
            }
        }
        break;

    case NET_FUNC::PEER_LINK:                                       // Peer Link
    {
        switch (opcode.second) {
        case NET_SUBFUNC::PL_TALKGROUP_LIST:                        // Talkgroup List
        {
            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (m_tgidPkt.decode(data, &decompressed, &decompressedLen)) {
                if (m_tidLookup == nullptr) {
                    LogError(LOG_NET, "Talkgroup ID lookup not available yet.");
                    m_tgidPkt.clear();
                    delete[] decompressed;
                    break;
                }

                // store to file
                DECLARE_CHAR_ARRAY(str, decompressedLen + 1U);
                ::memcpy(str, decompressed, decompressedLen);
                str[decompressedLen] = 0; // null termination

                // randomize filename
                std::ostringstream s;
                if (!m_peerLinkSavesACL) {
                    std::random_device rd;
                    std::mt19937 mt(rd());
                    std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
                    s << "/tmp/talkgroup_rules.yml." << dist(mt);
                } else {
                    s << m_tidLookup->filename();
                }

                std::string filename = s.str();
                std::ofstream file(filename, std::ofstream::out);
                if (file.fail()) {
                    LogError(LOG_NET, "Cannot open the talkgroup ID lookup file - %s", filename.c_str());
                    m_tgidPkt.clear();
                    delete[] decompressed;
                    break;
                }

                file << str;
                file.close();

                m_tidLookup->stop(true);
                m_tidLookup->setReloadTime(0U);
                m_tidLookup->filename(filename);
                m_tidLookup->reload();

                // flag this peer as Peer-Link enabled
                m_peerLink = true;

                // cleanup temporary file
                ::remove(filename.c_str());
                m_tgidPkt.clear();
                delete[] decompressed;
            }
        }
        break;

        case NET_SUBFUNC::PL_RID_LIST:                              // Radio ID List
        {
            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (m_ridPkt.decode(data, &decompressed, &decompressedLen)) {
                if (m_ridLookup == nullptr) {
                    LogError(LOG_NET, "Radio ID lookup not available yet.");
                    m_ridPkt.clear();
                    delete[] decompressed;
                    break;
                }

                // store to file
                DECLARE_CHAR_ARRAY(str, decompressedLen + 1U);
                ::memcpy(str, decompressed, decompressedLen);
                str[decompressedLen] = 0; // null termination

                // randomize filename
                std::ostringstream s;
                if (!m_peerLinkSavesACL) {
                    std::random_device rd;
                    std::mt19937 mt(rd());
                    std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
                    s << "/tmp/rid_acl.dat." << dist(mt);
                } else {
                    s << m_ridLookup->filename();
                }

                std::string filename = s.str();
                std::ofstream file(filename, std::ofstream::out);
                if (file.fail()) {
                    LogError(LOG_NET, "Cannot open the radio ID lookup file - %s", filename.c_str());
                    m_ridPkt.clear();
                    delete[] decompressed;
                    break;
                }

                file << str;
                file.close();

                m_ridLookup->stop(true);
                m_ridLookup->setReloadTime(0U);
                m_ridLookup->filename(filename);
                m_ridLookup->reload();

                // flag this peer as Peer-Link enabled
                m_peerLink = true;

                // cleanup temporary file
                ::remove(filename.c_str());
                m_ridPkt.clear();
                delete[] decompressed;
            }
        }
        break;

        case NET_SUBFUNC::PL_PEER_LIST:                             // Peer List
        {
            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (m_pidPkt.decode(data, &decompressed, &decompressedLen)) {
                if (m_pidLookup == nullptr) {
                    LogError(LOG_NET, "Peer ID lookup not available yet.");
                    m_pidPkt.clear();
                    delete[] decompressed;
                    break;
                }

                // store to file
                DECLARE_CHAR_ARRAY(str, decompressedLen + 1U);
                ::memcpy(str, decompressed, decompressedLen);
                str[decompressedLen] = 0; // null termination

                // randomize filename
                std::ostringstream s;
                if (!m_peerLinkSavesACL) {
                    std::random_device rd;
                    std::mt19937 mt(rd());
                    std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
                    s << "/tmp/peer_list.dat." << dist(mt);
                } else {
                    s << m_pidLookup->filename();
                }

                std::string filename = s.str();
                std::ofstream file(filename, std::ofstream::out);
                if (file.fail()) {
                    LogError(LOG_NET, "Cannot open the peer ID lookup file - %s", filename.c_str());
                    m_pidPkt.clear();
                    delete[] decompressed;
                    break;
                }

                file << str;
                file.close();

                m_pidLookup->stop(true);
                m_pidLookup->setReloadTime(0U);
                m_pidLookup->filename(filename);
                m_pidLookup->reload();

                // flag this peer as Peer-Link enabled
                m_peerLink = true;

                // cleanup temporary file
                ::remove(filename.c_str());
                m_pidPkt.clear();
                delete[] decompressed;
            }
        }
        break;

        default:
            break;
        }
    }
    break;

    default:
        Utils::dump("unknown opcode from the master", data, length);
        break;
    }
}

/* Writes configuration to the network. */

bool PeerNetwork::writeConfig()
{
    if (m_loginStreamId == 0U) {
        LogWarning(LOG_NET, "BUGBUG: tried to write network authorisation with no stream ID?");
        return false;
    }

    const char* software = __NETVER__;

    json::object config = json::object();

    // identity and frequency
    config["identity"].set<std::string>(m_metadata->identity);                      // Identity
    config["rxFrequency"].set<uint32_t>(m_metadata->rxFrequency);                   // Rx Frequency
    config["txFrequency"].set<uint32_t>(m_metadata->txFrequency);                   // Tx Frequency

    // system info
    json::object sysInfo = json::object();
    sysInfo["latitude"].set<float>(m_metadata->latitude);                           // Latitude
    sysInfo["longitude"].set<float>(m_metadata->longitude);                         // Longitude

    sysInfo["height"].set<int>(m_metadata->height);                                 // Height
    sysInfo["location"].set<std::string>(m_metadata->location);                     // Location
    config["info"].set<json::object>(sysInfo);

    // channel data
    json::object channel = json::object();
    channel["txPower"].set<uint32_t>(m_metadata->power);                            // Tx Power
    channel["txOffsetMhz"].set<float>(m_metadata->txOffsetMhz);                     // Tx Offset (Mhz)
    channel["chBandwidthKhz"].set<float>(m_metadata->chBandwidthKhz);               // Ch. Bandwidth (khz)
    channel["channelId"].set<uint8_t>(m_metadata->channelId);                       // Channel ID
    channel["channelNo"].set<uint32_t>(m_metadata->channelNo);                      // Channel No
    config["channel"].set<json::object>(channel);

    // RCON
    json::object rcon = json::object();
    rcon["password"].set<std::string>(m_metadata->restApiPassword);                 // REST API Password
    rcon["port"].set<uint16_t>(m_metadata->restApiPort);                            // REST API Port
    config["rcon"].set<json::object>(rcon);

    // Flags
    bool external = true;
    config["externalPeer"].set<bool>(external);                                     // External Peer Marker

    config["software"].set<std::string>(std::string(software));                     // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    DECLARE_CHAR_ARRAY(buffer, json.length() + 9U);

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC::RPTC, NET_SUBFUNC::NOP }, (uint8_t*)buffer, json.length() + 8U, pktSeq(), m_loginStreamId);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Process a data frames from the network. */

void PeerNetwork::taskNetworkRx(PeerPacketRequest* req)
{
    if (req != nullptr) {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        PeerNetwork* network = static_cast<PeerNetwork*>(req->obj);
        if (network == nullptr) {
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }

            return;
        }

        if (req == nullptr)
            return;

        if (req->length > 0) {
            // determine if this packet is late (i.e. are we processing this packet more than 200ms after it was received?)
            uint64_t dt = req->pktRxTime + PACKET_LATE_TIME;
            if (dt < now) {
                LogWarning(LOG_NET, "PEER %u packet processing latency >200ms, dt = %u, now = %u", req->peerId, dt, now);
            }

            // process incomfing message subfunction opcodes
            switch (req->subFunc) {
            case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:                 // Encapsulated DMR data frame
                {
                    if (network->m_dmrCallback != nullptr)
                        network->m_dmrCallback(network, req->buffer, req->length, req->streamId, req->fneHeader, req->rtpHeader);
                }
                break;

            case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:                 // Encapsulated P25 data frame
                {
                    if (network->m_p25Callback != nullptr)
                        network->m_p25Callback(network, req->buffer, req->length, req->streamId, req->fneHeader, req->rtpHeader);
                }
                break;

            case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:                // Encapsulated NXDN data frame
                {
                    if (network->m_nxdnCallback != nullptr)
                        network->m_nxdnCallback(network, req->buffer, req->length, req->streamId, req->fneHeader, req->rtpHeader);
                }
                break;

            default:
                Utils::dump("unknown protocol opcode from the master", req->buffer, req->length);
                break;
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}
