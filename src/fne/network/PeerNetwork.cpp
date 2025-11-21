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
#include "common/json/json.h"
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
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool analog, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    Network(address, port, localPort, peerId, password, duplex, debug, dmr, p25, nxdn, analog, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, updateLookup, saveLookup),
    m_attachedKeyRSPHandler(false),
    m_dmrCallback(nullptr),
    m_p25Callback(nullptr),
    m_nxdnCallback(nullptr),
    m_analogCallback(nullptr),
    m_netTreeDiscCallback(nullptr),
    m_peerReplicaCallback(nullptr),
    m_masterPeerId(0U),
    m_pidLookup(nullptr),
    m_peerReplica(false),
    m_peerReplicaSavesACL(false),
    m_tgidPkt(true, "Peer Replication, TGID List"),
    m_ridPkt(true, "Peer Replication, RID List"),
    m_pidPkt(true, "Peer Replication, PID List"),
    m_threadPool(WORKER_CNT, "peer"),
    m_prevSpanningTreeChildren(0U)
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

    // start thread pool
    m_threadPool.start();
}

/* Finalizes a instance of the PeerNetwork class. */

PeerNetwork::~PeerNetwork()
{
    // stop thread pool
    m_threadPool.stop();
    m_threadPool.wait();
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

    return Network::open();
}

/* Closes connection to the network. */

void PeerNetwork::close()
{
    Network::close();
}

/* Writes a complete update of this CFNE's active peer list to the network. */

bool PeerNetwork::writePeerLinkPeers(json::array* peerList)
{
    if (peerList == nullptr)
        return false;
    if (peerList->size() == 0)
        return false;

    if (peerList->size() > 0 && m_peerReplica) {
        json::value v = json::value(*peerList);
        std::string json = std::string(v.serialize());

        size_t len = json.length() + 9U;
        DECLARE_CHAR_ARRAY(buffer, len);

        ::memcpy(buffer + 0U, TAG_PEER_REPLICA, 4U);
        ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

        PacketBuffer pkt(true, "Peer Replication, Active Peer List");
        pkt.encode((uint8_t*)buffer, len);

        uint32_t streamId = createStreamId();
        LogInfoEx(LOG_REPL, "PEER %u Peer Replication, Active Peer List, blocks %u, streamId = %u", m_peerId, pkt.fragments.size(), streamId);
        if (pkt.fragments.size() > 0U) {
            for (auto frag : pkt.fragments) {
                writeMaster({ NET_FUNC::REPL, NET_SUBFUNC::REPL_ACT_PEER_LIST }, 
                    frag.second->data, FRAG_SIZE, RTP_END_OF_CALL_SEQ, streamId, true);
                Thread::sleep(60U); // pace block transmission
            }
        }

        pkt.clear();
        return true;
    }

    return false;
}

/* Writes a complete update of this CFNE's known spanning tree upstream to the network. */

bool PeerNetwork::writeSpanningTree(SpanningTree* treeRoot)
{
    if (treeRoot == nullptr)
        return false;
    if (treeRoot->m_children.size() == 0 && m_prevSpanningTreeChildren == 0U)
        return false;

    if ((treeRoot->m_children.size() > 0) || (m_prevSpanningTreeChildren > 0U)) {
        json::array jsonArray;
        SpanningTree::serializeTree(treeRoot, jsonArray);

        json::value v = json::value(jsonArray);
        std::string json = std::string(v.serialize());

        size_t len = json.length() + 9U;
        DECLARE_CHAR_ARRAY(buffer, len);

        ::memcpy(buffer + 0U, TAG_PEER_REPLICA, 4U);
        ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

        PacketBuffer pkt(true, "Network Tree, Tree List");
        pkt.encode((uint8_t*)buffer, len);

        uint32_t streamId = createStreamId();
        LogInfoEx(LOG_STP, "PEER %u Network Tree, Tree List, blocks %u, streamId = %u", m_peerId, pkt.fragments.size(), streamId);
        if (pkt.fragments.size() > 0U) {
            for (auto frag : pkt.fragments) {
                writeMaster({ NET_FUNC::NET_TREE, NET_SUBFUNC::NET_TREE_LIST }, 
                    frag.second->data, FRAG_SIZE, RTP_END_OF_CALL_SEQ, streamId, true);
                Thread::sleep(60U); // pace block transmission
            }
        }

        pkt.clear();

        m_prevSpanningTreeChildren = treeRoot->m_children.size();
        return true;
    }

    m_prevSpanningTreeChildren = treeRoot->m_children.size();
    return false;
}

/* Writes a complete update of this CFNE's HA parameters to the network. */

bool PeerNetwork::writeHAParams(std::vector<HAParameters>& haParams)
{
    if (haParams.size() == 0)
        return false;

    if (haParams.size() > 0 && m_peerReplica) {
        uint32_t len = 4U + (haParams.size() * HA_PARAMS_ENTRY_LEN);
        DECLARE_UINT8_ARRAY(buffer, len);

        SET_UINT32((len - 4U), buffer, 0U);

        uint32_t offs = 4U;
        for (uint8_t i = 0U; i < haParams.size(); i++) {
            uint32_t peerId = haParams[i].peerId;
            uint32_t ipAddr = haParams[i].masterIP;
            uint16_t port = haParams[i].masterPort;

            SET_UINT32(peerId, buffer, offs);
            SET_UINT32(ipAddr, buffer, offs + 4U);
            SET_UINT16(port, buffer, offs + 8U);

            offs += HA_PARAMS_ENTRY_LEN;
        }

        // bryanb: this should probably be packet buffered
        writeMaster({ NET_FUNC::REPL, NET_SUBFUNC::REPL_HA_PARAMS }, 
            buffer, len, RTP_END_OF_CALL_SEQ, createStreamId(), true);

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
                LogError(LOG_PEER, "Failed to task enqueue network packet request, peerId = %u", peerId);
                if (req != nullptr) {
                    if (req->buffer != nullptr)
                        delete[] req->buffer;
                    delete req;
                }
            }
        }
        break;

    case NET_FUNC::REPL:                                            // Peer Replication
    {
        switch (opcode.second) {
        case NET_SUBFUNC::REPL_TALKGROUP_LIST:                      // Talkgroup List
        {
            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (m_tgidPkt.decode(data, &decompressed, &decompressedLen)) {
                if (m_tidLookup == nullptr) {
                    LogError(LOG_PEER, "Talkgroup ID lookup not available yet.");
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
                if (!m_peerReplicaSavesACL) {
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
                    LogError(LOG_PEER, "Cannot open the talkgroup ID lookup file - %s", filename.c_str());
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

                // flag this peer as replica enabled
                m_peerReplica = true;
                if (m_peerReplicaCallback != nullptr)
                    m_peerReplicaCallback(this);

                // cleanup temporary file
                ::remove(filename.c_str());
                m_tgidPkt.clear();
                delete[] decompressed;
            }
        }
        break;

        case NET_SUBFUNC::REPL_RID_LIST:                            // Radio ID List
        {
            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (m_ridPkt.decode(data, &decompressed, &decompressedLen)) {
                if (m_ridLookup == nullptr) {
                    LogError(LOG_PEER, "Radio ID lookup not available yet.");
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
                if (!m_peerReplicaSavesACL) {
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
                    LogError(LOG_PEER, "Cannot open the radio ID lookup file - %s", filename.c_str());
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

                // flag this peer as replica enabled
                m_peerReplica = true;
                if (m_peerReplicaCallback != nullptr)
                    m_peerReplicaCallback(this);

                // cleanup temporary file
                ::remove(filename.c_str());
                m_ridPkt.clear();
                delete[] decompressed;
            }
        }
        break;

        case NET_SUBFUNC::REPL_PEER_LIST:                           // Peer List
        {
            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (m_pidPkt.decode(data, &decompressed, &decompressedLen)) {
                if (m_pidLookup == nullptr) {
                    LogError(LOG_PEER, "Peer ID lookup not available yet.");
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
                if (!m_peerReplicaSavesACL) {
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
                    LogError(LOG_PEER, "Cannot open the peer ID lookup file - %s", filename.c_str());
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

                // flag this peer as replica enabled
                m_peerReplica = true;
                if (m_peerReplicaCallback != nullptr)
                    m_peerReplicaCallback(this);

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

    case NET_FUNC::NET_TREE:                                      // Network Tree
    {
        switch (opcode.second) {
        case NET_SUBFUNC::NET_TREE_DISC:                          // Network Tree Disconnect
        {
            uint32_t offendingPeerId = GET_UINT32(data, 6U);
            LogWarning(LOG_PEER, "PEER %u Network Tree Disconnect, requested from upstream master, possible duplicate connection for PEER %u", m_peerId, offendingPeerId);

            if (m_netTreeDiscCallback != nullptr) {
                m_netTreeDiscCallback(this, offendingPeerId);
            }
        }
        break;

        default:
            break;
        }
    }
    break;

    default:
        Utils::dump("Unknown opcode from the master", data, length);
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
    /*
    ** bryanb: don't change externalPeer to neighborPeer -- this will break backward
    **  compat with older FNE versions (we're stuck with this naming :()
    */
    bool external = true;
    config["externalPeer"].set<bool>(external);                                     // External FNE Neighbor Peer Marker
    config["masterPeerId"].set<uint32_t>(m_masterPeerId);                           // Master Peer ID

    config["software"].set<std::string>(std::string(software));                     // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    DECLARE_CHAR_ARRAY(buffer, json.length() + 9U);

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "PeerNetwork::writeConfig(), Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
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
                LogWarning(LOG_PEER, "PEER %u packet processing latency >200ms, dt = %u, now = %u", req->peerId, dt, now);
            }

            uint16_t lastRxSeq = 0U;

            MULTIPLEX_RET_CODE ret = network->m_mux->verifyStream(req->streamId, req->rtpHeader.getSequence(), req->fneHeader.getFunction(), &lastRxSeq);
            if (ret == MUX_LOST_FRAMES) {
                LogError(LOG_PEER, "PEER %u stream %u possible lost frames; got %u, expected %u", req->fneHeader.getPeerId(),
                    req->streamId, req->rtpHeader.getSequence(), lastRxSeq);
            }
            else if (ret == MUX_OUT_OF_ORDER) {
                LogError(LOG_PEER, "PEER %u stream %u out-of-order; got %u, expected >%u", req->fneHeader.getPeerId(),
                    req->streamId, req->rtpHeader.getSequence(), lastRxSeq);
            }
#if DEBUG_RTP_MUX
            else {
                LogDebugEx(LOG_PEER, "PeerNetwork::taskNetworkRx()", "PEER %u valid mux, seq = %u, streamId = %u", req->fneHeader.getPeerId(), req->rtpHeader.getSequence(), req->streamId);
            }
#endif
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

            case NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG:              // Encapsulated analog data frame
                {
                    if (network->m_analogCallback != nullptr)
                        network->m_analogCallback(network, req->buffer, req->length, req->streamId, req->fneHeader, req->rtpHeader);
                }
                break;

            default:
                Utils::dump("Unknown protocol opcode from the master", req->buffer, req->length);
                break;
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}
