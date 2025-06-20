// SPDX-License-Identifier: PROPRIETARY
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "sysview/Defines.h"
#include "common/network/json/json.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/p25/dfsi/LC.h"
#include "common/zlib/zlib.h"
#include "common/Utils.h"
#include "network/PeerNetwork.h"
#include "SysViewMain.h"

using namespace network;

#include <cassert>
#include <fstream>
#include <streambuf>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex PeerNetwork::m_peerStatusMutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PeerNetwork class. */

PeerNetwork::PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    Network(address, port, localPort, peerId, password, duplex, debug, true, true, true, true, true, true, allowActivityTransfer, allowDiagnosticTransfer, updateLookup, saveLookup),
    peerStatus(),
    m_peerLink(false),
    m_tgidPkt(true, "Peer-Link, TGID List"),
    m_ridPkt(true, "Peer-Link, RID List")
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    // ignore the source peer ID for packets destined to SysView
    m_promiscuousPeer = true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* User overrideable handler that allows user code to process network packets not handled by this class. */

void PeerNetwork::userPacketHandler(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint32_t streamId,
    const frame::RTPFNEHeader& fneHeader, const frame::RTPHeader& rtpHeader)
{
    switch (opcode.first) {
    case NET_FUNC::TRANSFER:
    {
        switch (opcode.second) {
        case NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY:
        {
            DECLARE_UINT8_ARRAY(rawPayload, length - 11U);
            ::memcpy(rawPayload, data + 11U, length - 11U);
            std::string payload(rawPayload, rawPayload + (length - 11U));

            bool currState = g_disableTimeDisplay;
            g_disableTimeDisplay = true;

            std::string identity = std::string();
            auto it = std::find_if(g_peerIdentityNameMap.begin(), g_peerIdentityNameMap.end(), [&](PeerIdentityMapPair x) { return x.first == peerId; });
            if (it != g_peerIdentityNameMap.end())
                identity = g_peerIdentityNameMap[peerId];

            ::Log(9999U, nullptr, "%.9u (%8s) %s", peerId, identity.c_str(), payload.c_str());
            g_disableTimeDisplay = currState;
        }
        break;

        case NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS:
        {
            DECLARE_UINT8_ARRAY(rawPayload, length - 11U);
            ::memcpy(rawPayload, data + 11U, length - 11U);
            std::string payload(rawPayload, rawPayload + (length - 11U));

            if (g_debug)
                LogMessage(LOG_NET, "Peer Status, peerId = %u", peerId);

            // parse JSON body
            json::value v;
            std::string err = json::parse(v, payload);
            if (!err.empty()) {
                break;
            }

            // ensure parsed JSON is an object
            if (!v.is<json::object>()) {
                break;
            }

            json::object obj = v.get<json::object>();
            uint32_t actualPeerId = obj["peerId"].getDefault<uint32_t>(peerId);
            std::lock_guard<std::mutex> lock(m_peerStatusMutex);
            peerStatus[actualPeerId] = obj;
        }
        break;

        default:
            break;
        }
    }
    break;
    
    case NET_FUNC::PEER_LINK:
    {
        switch (opcode.second) {
        case NET_SUBFUNC::PL_TALKGROUP_LIST:
        {
            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (m_tgidPkt.decode(data, &decompressed, &decompressedLen)) {
                if (m_tidLookup == nullptr) {
                    LogError(LOG_NET, "Talkgroup ID lookups not available yet.");
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
                std::random_device rd;
                std::mt19937 mt(rd());
                std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
                s << "/tmp/talkgroup_rules.yml." << dist(mt);

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

        case NET_SUBFUNC::PL_RID_LIST:
        {
            uint32_t decompressedLen = 0U;
            uint8_t* decompressed = nullptr;

            if (m_ridPkt.decode(data, &decompressed, &decompressedLen)) {
                if (m_ridLookup == nullptr) {
                    LogError(LOG_NET, "Radio ID lookups not available yet.");
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
                std::random_device rd;
                std::mt19937 mt(rd());
                std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
                s << "/tmp/rid_acl.dat." << dist(mt);

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
    bool convPeer = true;
    config["conventionalPeer"].set<bool>(convPeer);                                 // Conventional Peer Marker
    bool sysView = true;
    config["sysView"].set<bool>(sysView);                                           // SysView Peer Marker

    config["software"].set<std::string>(std::string(software));                     // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    DECLARE_CHAR_ARRAY(buffer, json.length() + 9U);

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC::RPTC, NET_SUBFUNC::NOP }, (uint8_t*)buffer, json.length() + 8U, RTP_END_OF_CALL_SEQ, m_loginStreamId);
}
