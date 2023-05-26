/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "edac/SHA256.h"
#include "network/Network.h"
#include "network/RTPHeader.h"
#include "network/RTPFNEHeader.h"
#include "network/json/json.h"
#include "Log.h"
#include "StopWatch.h"
#include "Utils.h"

using namespace network;

#include <cstdio>
#include <cassert>
#include <cstdlib>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Network class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="local"></param>
/// <param name="peerId">Unique ID on the network.</param>
/// <param name="password">Network authentication password.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="dmr">Flag indicating whether DMR is enabled.</param>
/// <param name="p25">Flag indicating whether P25 is enabled.</param>
/// <param name="nxdn">Flag indicating whether NXDN is enabled.</param>
/// <param name="slot1">Flag indicating whether DMR slot 1 is enabled for network traffic.</param>
/// <param name="slot2">Flag indicating whether DMR slot 2 is enabled for network traffic.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="updateLookup">Flag indicating that the system will accept radio ID and talkgroup ID lookups from the network.</param>
Network::Network(const std::string& address, uint16_t port, uint16_t localPort, uint32_t peerId, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool nxdn, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup) :
    BaseNetwork(peerId, duplex, debug, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, localPort),
    m_address(address),
    m_port(port),
    m_password(password),
    m_enabled(false),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_nxdnEnabled(nxdn),
    m_updateLookup(updateLookup),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_salt(nullptr),
    m_retryTimer(1000U, 10U),
    m_timeoutTimer(1000U, 60U),
    m_pktLastSeq(0U),
    m_pktNextSeq(0U),
    m_pktSeq(0U),
    m_identity(),
    m_rxFrequency(0U),
    m_txFrequency(0U),
    m_txOffsetMhz(0.0F),
    m_chBandwidthKhz(0.0F),
    m_channelId(0U),
    m_channelNo(0U),
    m_power(0U),
    m_latitude(0.0F),
    m_longitude(0.0F),
    m_height(0),
    m_location(),
    m_restApiPassword(),
    m_restApiPort(0)
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    m_salt = new uint8_t[sizeof(uint32_t)];
}

/// <summary>
/// Finalizes a instance of the Network class.
/// </summary>
Network::~Network()
{
    delete[] m_salt;
}

/// <summary>
/// Sets the instances of the Radio ID and Talkgroup ID lookup tables.
/// </summary>
/// <param name="ridLookup">Radio ID Lookup Table Instance</param>
/// <param name="tidLookup">Talkgroup Rules Lookup Table Instance</param>
void Network::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
}

/// <summary>
/// Sets metadata configuration settings from the modem.
/// </summary>
/// <param name="identity"></param>
/// <param name="rxFrequency"></param>
/// <param name="txFrequency"></param>
/// <param name="txOffsetMhz"></param>
/// <param name="chBandwidthKhz"></param>
/// <param name="channelId"></param>
/// <param name="channelNo"></param>
/// <param name="power"></param>
/// <param name="latitude"></param>
/// <param name="longitude"></param>
/// <param name="height"></param>
/// <param name="location"></param>
void Network::setMetadata(const std::string& identity, uint32_t rxFrequency, uint32_t txFrequency, float txOffsetMhz, float chBandwidthKhz,
    uint8_t channelId, uint32_t channelNo, uint32_t power, float latitude, float longitude, int height, const std::string& location)
{
    m_identity = identity;
    m_rxFrequency = rxFrequency;
    m_txFrequency = txFrequency;

    m_txOffsetMhz = txOffsetMhz;
    m_chBandwidthKhz = chBandwidthKhz;
    m_channelId = channelId;
    m_channelNo = channelNo;

    m_power = power;
    m_latitude = latitude;
    m_longitude = longitude;
    m_height = height;
    m_location = location;
}

/// <summary>
/// Sets REST API configuration settings from the modem.
/// </summary>
/// <param name="password"></param>
/// <param name="port"></param>
void Network::setRESTAPIData(const std::string& password, uint16_t port)
{
    m_restApiPassword = password;
    m_restApiPort = port;
}

/// <summary>
/// Updates the timer by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void Network::clock(uint32_t ms)
{
    if (m_status == NET_STAT_WAITING_CONNECT) {
        m_retryTimer.clock(ms);
        if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
            if (m_enabled) {
                bool ret = m_socket->open(m_addr.ss_family);
                if (ret) {
                    ret = writeLogin();
                    if (!ret) {
                        m_retryTimer.start();
                        return;
                    }

                    m_status = NET_STAT_WAITING_LOGIN;
                    m_timeoutTimer.start();
                }
            }

            m_retryTimer.start();
        }

        return;
    }

    // if we aren't enabled -- bail
    if (!m_enabled) {
        return;
    }

    sockaddr_storage address;
    uint32_t addrLen;

    frame::RTPHeader rtpHeader = frame::RTPHeader(true);
    frame::RTPFNEHeader fneHeader;
    int length = 0U;

    // read message
    UInt8Array buffer = m_frameQueue->read(length, address, addrLen, &rtpHeader, &fneHeader);
    if (length > 0) {
        if (!UDPSocket::match(m_addr, address)) {
            LogError(LOG_NET, "Packet received from an invalid source");
            return;
        }

        m_pktSeq = rtpHeader.getSequence();

        if (m_pktSeq != m_pktNextSeq) {
            LogWarning(LOG_NET, "Packet out-of-sequence; %u != %u", m_pktNextSeq, rtpHeader.getSequence());
        }

        m_pktLastSeq = m_pktSeq;
        m_pktNextSeq = rtpHeader.getSequence() + 1;
        if (m_pktNextSeq > UINT16_MAX) {
            m_pktNextSeq = 0U;
        }

        // process incoming message frame opcodes
        switch (fneHeader.getFunction()) {
        case NET_FUNC_PROTOCOL:
            {
                if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_DMR) {           // Encapsulated DMR data frame
#if defined(ENABLE_DMR)
                    if (m_enabled && m_dmrEnabled) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, DMR", buffer.get(), length);

                        uint8_t len = length;
                        m_rxDMRData.addData(&len, 1U);
                        m_rxDMRData.addData(buffer.get(), len);
                    }
#endif // defined(ENABLE_DMR)
                }
                else if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_P25) {      // Encapsulated P25 data frame
#if defined(ENABLE_P25)
                    if (m_enabled && m_p25Enabled) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, P25", buffer.get(), length);

                        uint8_t len = length;
                        m_rxP25Data.addData(&len, 1U);
                        m_rxP25Data.addData(buffer.get(), len);
                    }
#endif // defined(ENABLE_P25)
                }
                else if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_NXDN) {     // Encapsulated NXDN data frame
#if defined(ENABLE_NXDN)
                    if (m_enabled && m_nxdnEnabled) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, NXDN", buffer.get(), length);

                        uint8_t len = length;
                        m_rxNXDNData.addData(&len, 1U);
                        m_rxNXDNData.addData(buffer.get(), len);
                    }
#endif // defined(ENABLE_NXDN)
                }
                else {
                    Utils::dump("Unknown protocol opcode from the master", buffer.get(), length);
                }
            }
            break;

        case NET_FUNC_MASTER:
            {
                if (fneHeader.getSubFunction() == NET_MASTER_SUBFUNC_WL_RID) {          // Radio ID Whitelist
                    if (m_enabled && m_updateLookup) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, WL RID", buffer.get(), length);

                        if (m_ridLookup != nullptr) {
                            // update RID lists
                            uint32_t len = __GET_UINT16(buffer, 7U);
                            uint32_t j = 0U;
                            for (uint8_t i = 0; i < len; i++) {
                                uint32_t id = __GET_UINT16(buffer, 11U + j);
                                m_ridLookup->toggleEntry(id, true);
                                j += 4U;
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_MASTER_SUBFUNC_BL_RID) {     // Radio ID Blacklist
                    if (m_enabled && m_updateLookup) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, BL RID", buffer.get(), length);

                        if (m_ridLookup != nullptr) {
                            // update RID lists
                            uint32_t len = __GET_UINT16(buffer, 7U);
                            uint32_t j = 0U;
                            for (uint8_t i = 0; i < len; i++) {
                                uint32_t id = __GET_UINT16(buffer, 11U + j);
                                m_ridLookup->toggleEntry(id, false);
                                j += 4U;
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_MASTER_SUBFUNC_ACTIVE_TGS) { // Talkgroup Active IDs
                    if (m_enabled && m_updateLookup) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, ACTIVE TGS", buffer.get(), length);

                        if (m_tidLookup != nullptr) {
                            // update TGID lists
                            uint32_t len = __GET_UINT16(buffer, 7U);
                            uint32_t j = 0U;
                            for (uint8_t i = 0; i < len; i++) {
                                uint32_t id = __GET_UINT16(buffer, 11U + j);
                                uint8_t slot = (buffer[14U + j]);

                                lookups::TalkgroupRuleGroupVoice tid = m_tidLookup->find(id, slot);
                                if (tid.isInvalid()) {
                                    if (!tid.config().active()) {
                                        m_tidLookup->eraseEntry(id, slot);
                                    }
                                    
                                    LogMessage(LOG_NET, "Activated TG %u TS %u in TGID table", id, slot);
                                    m_tidLookup->addEntry(id, slot, true);
                                }

                                j += 5U;
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_MASTER_SUBFUNC_DEACTIVE_TGS) { // Talkgroup Deactivated IDs
                    if (m_enabled && m_updateLookup) {
                        if (m_debug)
                            Utils::dump(1U, "Network Received, DEACTIVE TGS", buffer.get(), length);

                        if (m_tidLookup != nullptr) {
                            // update TGID lists
                            uint32_t len = __GET_UINT16(buffer, 7U);
                            uint32_t j = 0U;
                            for (uint8_t i = 0; i < len; i++) {
                                uint32_t id = __GET_UINT16(buffer, 11U + j);
                                uint8_t slot = (buffer[14U + j]);

                                lookups::TalkgroupRuleGroupVoice tid = m_tidLookup->find(id, slot);
                                if (!tid.isInvalid()) {
                                    LogMessage(LOG_NET, "Deactivated TG %u TS %u in TGID table", id, slot);
                                    m_tidLookup->eraseEntry(id, slot);
                                }

                                j += 5U;
                            }
                        }
                    }
                }
                else {
                    Utils::dump("Unknown master control opcode from the master", buffer.get(), length);
                }
            }
            break;

        case NET_FUNC_NAK:                                                              // Master Negative Ack
            {
                if (m_status == NET_STAT_RUNNING) {
                    LogWarning(LOG_NET, "Master returned a NAK; attemping to relogin ...");
                    m_status = NET_STAT_WAITING_LOGIN;
                    m_timeoutTimer.start();
                    m_retryTimer.start();
                }
                else {
                    LogError(LOG_NET, "Master returned a NAK; network reconnect ...");
                    close();
                    open();
                    return;
                }
            }
            break;
        case NET_FUNC_ACK:                                                              // Repeater Ack
            {
                switch (m_status) {
                    case NET_STAT_WAITING_LOGIN:
                        LogDebug(LOG_NET, "Sending authorisation");
                        ::memcpy(m_salt, buffer.get() + 6U, sizeof(uint32_t));
                        writeAuthorisation();
                        m_status = NET_STAT_WAITING_AUTHORISATION;
                        m_timeoutTimer.start();
                        m_retryTimer.start();
                        break;
                    case NET_STAT_WAITING_AUTHORISATION:
                        LogDebug(LOG_NET, "Sending configuration");
                        writeConfig();
                        m_status = NET_STAT_WAITING_CONFIG;
                        m_timeoutTimer.start();
                        m_retryTimer.start();
                        break;
                    case NET_STAT_WAITING_CONFIG:
                        LogMessage(LOG_NET, "Logged into the master successfully");
                        m_status = NET_STAT_RUNNING;
                        m_timeoutTimer.start();
                        m_retryTimer.start();
                        break;
                    default:
                        break;
                }
            }
            break;
        case NET_FUNC_MST_CLOSING:                                                      // Master Shutdown
            {
                LogError(LOG_NET, "Master is closing down");
                close();
                open();
            }
            break;
        case NET_FUNC_PONG:                                                             // Master Ping Response
            m_timeoutTimer.start();
            break;
        default:
            Utils::dump("Unknown opcode from the master", buffer.get(), length);
        }
    }

    m_retryTimer.clock(ms);
    if (m_retryTimer.isRunning() && m_retryTimer.hasExpired()) {
        switch (m_status) {
            case NET_STAT_WAITING_LOGIN:
                writeLogin();
                break;
            case NET_STAT_WAITING_AUTHORISATION:
                writeAuthorisation();
                break;
            case NET_STAT_WAITING_CONFIG:
                writeConfig();
                break;
            case NET_STAT_RUNNING:
                writePing();
                break;
            default:
                break;
        }

        m_retryTimer.start();
    }

    m_timeoutTimer.clock(ms);
    if (m_timeoutTimer.isRunning() && m_timeoutTimer.hasExpired()) {
        LogError(LOG_NET, "Connection to the master has timed out, retrying connection");
        close();
        open();
    }
}

/// <summary>
/// Opens connection to the network.
/// </summary>
/// <returns></returns>
bool Network::open()
{
    if (!m_enabled)
        return false;
    if (m_debug)
        LogMessage(LOG_NET, "Opening Network");

    if (UDPSocket::lookup(m_address, m_port, m_addr, m_addrLen) != 0) {
        LogMessage(LOG_NET, "Could not lookup the address of the master");
        return false;
    }

    m_status = NET_STAT_WAITING_CONNECT;
    m_timeoutTimer.stop();
    m_retryTimer.start();

    return true;
}

/// <summary>
/// Sets flag enabling network communication.
/// </summary>
/// <param name="enabled"></param>
void Network::enable(bool enabled)
{
    m_enabled = enabled;
}

/// <summary>
/// Closes connection to the network.
/// </summary>
void Network::close()
{
    if (m_debug)
        LogMessage(LOG_NET, "Closing Network");

    if (m_status == NET_STAT_RUNNING) {
        uint8_t buffer[9U];
        ::memcpy(buffer + 0U, TAG_REPEATER_CLOSING, 5U);

        m_frameQueue->enqueueMessage(buffer, 9U, createStreamId(), m_peerId, 
            { NET_FUNC_RPT_CLOSING, NET_SUBFUNC_NOP }, m_addr, m_addrLen);
        m_frameQueue->flushQueue();
    }

    m_socket->close();

    m_retryTimer.stop();
    m_timeoutTimer.stop();

    m_status = NET_STAT_WAITING_CONNECT;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Writes login request to the network.
/// </summary>
/// <returns></returns>
bool Network::writeLogin()
{
    if (!m_enabled) {
        return false;
    }

    uint8_t buffer[8U];

    ::memcpy(buffer + 0U, TAG_REPEATER_LOGIN, 4U);

    if (m_debug)
        Utils::dump(1U, "Network Message, Login", buffer, 8U);

    m_frameQueue->enqueueMessage(buffer, 8U, createStreamId(), m_peerId, 
        { NET_FUNC_RPTL, NET_SUBFUNC_NOP }, m_addr, m_addrLen);
    return m_frameQueue->flushQueue();
}

/// <summary>
/// Writes network authentication challenge.
/// </summary>
/// <returns></returns>
bool Network::writeAuthorisation()
{
    size_t size = m_password.size();

    uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
    ::memcpy(in, m_salt, sizeof(uint32_t));
    for (size_t i = 0U; i < size; i++)
        in[i + sizeof(uint32_t)] = m_password.at(i);

    uint8_t out[40U];
    ::memcpy(out + 0U, TAG_REPEATER_AUTH, 4U);
    __SET_UINT32(m_peerId, out, 4U);                                                // Peer ID

    edac::SHA256 sha256;
    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out + 8U);

    delete[] in;

    if (m_debug)
        Utils::dump(1U, "Network Message, Authorisation", out, 40U);

    m_frameQueue->enqueueMessage(out, 40U, createStreamId(), m_peerId, 
        { NET_FUNC_RPTK, NET_SUBFUNC_NOP }, m_addr, m_addrLen);
    return m_frameQueue->flushQueue();
}

/// <summary>
/// Writes modem configuration to the network.
/// </summary>
/// <returns></returns>
bool Network::writeConfig()
{
    const char* software = __NET_NAME__;

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

    config["software"].set<std::string>(std::string(software));                     // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    char buffer[json.length() + 8U];

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    ::sprintf(buffer + 8U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Message, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    m_frameQueue->enqueueMessage((uint8_t*)buffer, json.length() + 8U, createStreamId(), m_peerId, 
        { NET_FUNC_RPTC, NET_SUBFUNC_NOP }, m_addr, m_addrLen);
    return m_frameQueue->flushQueue();
}

/// <summary>
/// Writes a network stay-alive ping.
/// </summary>
bool Network::writePing()
{
    uint8_t buffer[11U];

    ::memcpy(buffer + 0U, TAG_REPEATER_PING, 7U);

    if (m_debug)
        Utils::dump(1U, "Network Message, Ping", buffer, 11U);

    m_frameQueue->enqueueMessage(buffer, 11U, createStreamId(), m_peerId, 
        { NET_FUNC_PING, NET_SUBFUNC_NOP }, m_addr, m_addrLen);
    return m_frameQueue->flushQueue();
}
