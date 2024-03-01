// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#include "fne/Defines.h"
#include "common/network/json/json.h"
#include "common/Utils.h"
#include "fne/network/PeerNetwork.h"

using namespace network;

#include <cstdio>
#include <cassert>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the PeerNetwork class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="local"></param>
/// <param name="peerId">Unique ID on the network.</param>
/// <param name="password">Network authentication password.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="debug">Flag indicating whether network debug is enabled.</param>
/// <param name="dmr">Flag indicating whether DMR is enabled.</param>
/// <param name="p25">Flag indicating whether P25 is enabled.</param>
/// <param name="nxdn">Flag indicating whether NXDN is enabled.</param>
/// <param name="slot1">Flag indicating whether DMR slot 1 is enabled for network traffic.</param>
/// <param name="slot2">Flag indicating whether DMR slot 2 is enabled for network traffic.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="updateLookup">Flag indicating that the system will accept radio ID and talkgroup ID lookups from the network.</param>
PeerNetwork::PeerNetwork(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup, bool saveLookup) :
    Network(address, port, localPort, peerId, password, duplex, debug, dmr, p25, nxdn, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, updateLookup, saveLookup),
    m_blockTrafficToTable()
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());
}

/// <summary>
/// Checks if the passed peer ID is blocked from sending to this peer.
/// </summary>
/// <param name="peerId"></param>
bool PeerNetwork::checkBlockedPeer(uint32_t peerId)
{
    if (m_blockTrafficToTable.empty())
        return false;

    if (std::find(m_blockTrafficToTable.begin(), m_blockTrafficToTable.end(), peerId) != m_blockTrafficToTable.end()) {
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Writes configuration to the network.
/// </summary>
/// <returns></returns>
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

    bool external = true;
    config["externalPeer"].set<bool>(external);                                     // External Peer Marker
    config["software"].set<std::string>(std::string(software));                     // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    char buffer[json.length() + 8U];

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::sprintf(buffer + 8U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    return writeMaster({ NET_FUNC_RPTC, NET_SUBFUNC_NOP }, (uint8_t*)buffer, json.length() + 8U, pktSeq(), m_loginStreamId);
}
