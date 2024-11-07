// SPDX-License-Identifier: PROPRIETARY
/*
 * Digital Voice Modem - FNE Affiliations View
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
#include "common/Utils.h"
#include "network/PeerNetwork.h"
#include "SysViewMain.h"

using namespace network;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex PeerNetwork::m_peerStatusMutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the PeerNetwork class. */

PeerNetwork::PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    Network(address, port, localPort, peerId, password, duplex, debug, dmr, p25, nxdn, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, updateLookup, saveLookup),
    peerStatus()
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

void PeerNetwork::userPacketHandler(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint32_t streamId)
{
    switch (opcode.first) {
    case NET_FUNC::TRANSFER:
    {
        switch (opcode.second) {
        case NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY:
        {
            UInt8Array __rawPayload = std::make_unique<uint8_t[]>(length - 11U);
            uint8_t* rawPayload = __rawPayload.get();
            ::memset(rawPayload, 0x00U, length - 11U);
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
            UInt8Array __rawPayload = std::make_unique<uint8_t[]>(length - 11U);
            uint8_t* rawPayload = __rawPayload.get();
            ::memset(rawPayload, 0x00U, length - 11U);
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
            std::lock_guard<std::mutex> lock(m_peerStatusMutex);
            peerStatus[peerId] = obj;
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
    config["identity"].set<std::string>(m_identity);                                // Identity
    config["rxFrequency"].set<uint32_t>(m_rxFrequency);                             // Rx Frequency
    config["txFrequency"].set<uint32_t>(m_txFrequency);                             // Tx Frequency

    // system info
    json::object sysInfo = json::object();
    sysInfo["latitude"].set<float>(m_latitude);                                     // Latitude
    sysInfo["longitude"].set<float>(m_longitude);                                   // Longitude

    sysInfo["height"].set<int>(m_height);                                           // Height
    sysInfo["location"].set<std::string>(m_location);                               // Location
    config["info"].set<json::object>(sysInfo);

    // channel data
    json::object channel = json::object();
    channel["txPower"].set<uint32_t>(m_power);                                      // Tx Power
    channel["txOffsetMhz"].set<float>(m_txOffsetMhz);                               // Tx Offset (Mhz)
    channel["chBandwidthKhz"].set<float>(m_chBandwidthKhz);                         // Ch. Bandwidth (khz)
    channel["channelId"].set<uint8_t>(m_channelId);                                 // Channel ID
    channel["channelNo"].set<uint32_t>(m_channelNo);                                // Channel No
    config["channel"].set<json::object>(channel);

    // RCON
    json::object rcon = json::object();
    rcon["password"].set<std::string>(m_restApiPassword);                           // REST API Password
    rcon["port"].set<uint16_t>(m_restApiPort);                                      // REST API Port
    config["rcon"].set<json::object>(rcon);

    bool convPeer = true;
    config["conventionalPeer"].set<bool>(convPeer);                                 // Conventional Peer Marker
    bool sysView = true;
    config["sysView"].set<bool>(sysView);                                           // SysView Peer Marker
    config["software"].set<std::string>(std::string(software));                     // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    CharArray __buffer = std::make_unique<char[]>(json.length() + 9U);
    char* buffer = __buffer.get();

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC::RPTC, NET_SUBFUNC::NOP }, (uint8_t*)buffer, json.length() + 8U, RTP_END_OF_CALL_SEQ, m_loginStreamId);
}
