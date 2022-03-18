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
*   Copyright (C) 2017-2020,2022 by Bryan Biedenkapp N2PLL
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
/// <param name="id">Unique ID of this modem on the network.</param>
/// <param name="password">Network authentication password.</param>
/// <param name="duplex">Flag indicating full-duplex operation.</param>
/// <param name="dmr">Flag indicating whether DMR is enabled.</param>
/// <param name="p25">Flag indicating whether P25 is enabled.</param>
/// <param name="slot1">Flag indicating whether DMR slot 1 is enabled for network traffic.</param>
/// <param name="slot2">Flag indicating whether DMR slot 2 is enabled for network traffic.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="updateLookup">Flag indicating that the system will accept radio ID and talkgroup ID lookups from the network.</param>
Network::Network(const std::string& address, uint16_t port, uint16_t local, uint32_t id, const std::string& password,
    bool duplex, bool debug, bool dmr, bool p25, bool slot1, bool slot2, bool allowActivityTransfer, bool allowDiagnosticTransfer, bool updateLookup) :
    BaseNetwork(local, id, duplex, debug, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer),
    m_address(address),
    m_port(port),
    m_password(password),
    m_enabled(false),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_updateLookup(updateLookup),
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
    m_rconPassword(),
    m_rconPort(0)
{
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());
}

/// <summary>
/// Finalizes a instance of the Network class.
/// </summary>
Network::~Network()
{
    /* stub */
}

/// <summary>
/// Sets the instances of the Radio ID and Talkgroup ID lookup tables.
/// </summary>
/// <param name="ridLookup">Radio ID Lookup Table Instance</param>
/// <param name="tidLookup">Talkgroup ID Lookup Table Instance</param>
void Network::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupIdLookup* tidLookup)
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
/// Sets RCON configuration settings from the modem.
/// </summary>
/// <param name="password"></param>
/// <param name="port"></param>
void Network::setRconData(const std::string& password, uint16_t port)
{
    m_rconPassword = password;
    m_rconPort = port;
}

/// <summary>
/// Gets the current status of the network.
/// </summary>
/// <returns></returns>
uint8_t Network::getStatus()
{
    return m_status;
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
            bool ret = m_socket.open(m_addr.ss_family);
            if (ret) {
                ret = writeLogin();
                if (!ret)
                    return;

                m_status = NET_STAT_WAITING_LOGIN;
                m_timeoutTimer.start();
            }

            m_retryTimer.start();
        }

        return;
    }

    sockaddr_storage address;
    uint32_t addrLen;
    int length = m_socket.read(m_buffer, DATA_PACKET_LENGTH, address, addrLen);
    if (length < 0) {
        LogError(LOG_NET, "Socket has failed, retrying connection to the master");
        close();
        open();
        return;
    }

    if (m_debug && length > 0)
        Utils::dump(1U, "Network Received", m_buffer, length);

    if (length > 0) {
        if (!UDPSocket::match(m_addr, address)) {
            LogError(LOG_NET, "Packet received from an invalid source");
            return;
        }

        if (::memcmp(m_buffer, TAG_DMR_DATA, 4U) == 0) {
            if (m_enabled && m_dmrEnabled) {
                if (m_debug)
                    Utils::dump(1U, "Network Received, DMR", m_buffer, length);

                uint8_t len = length;
                m_rxDMRData.addData(&len, 1U);
                m_rxDMRData.addData(m_buffer, len);
            }
        }
        else if (::memcmp(m_buffer, TAG_P25_DATA, 4U) == 0) {
            if (m_enabled && m_p25Enabled) {
                if (m_debug)
                    Utils::dump(1U, "Network Received, P25", m_buffer, length);

                uint8_t len = length;
                m_rxP25Data.addData(&len, 1U);
                m_rxP25Data.addData(m_buffer, len);
            }
        }
        else if (::memcmp(m_buffer, TAG_MASTER_WL_RID, 7U) == 0) {
            if (m_enabled && m_updateLookup) {
                if (m_debug)
                    Utils::dump(1U, "Network Received, WL RID", m_buffer, length);

                // update RID lists
                uint32_t len = (m_buffer[7U] << 16) | (m_buffer[8U] << 8) | (m_buffer[9U] << 0);
                uint32_t j = 0U;
                for (uint8_t i = 0; i < len; i++) {
                    uint32_t id = (m_buffer[11U + j] << 16) | (m_buffer[12U + j] << 8) | (m_buffer[13U + j] << 0);
                    m_ridLookup->toggleEntry(id, true);
                    j += 4U;
                }
            }
        }
        else if (::memcmp(m_buffer, TAG_MASTER_BL_RID, 7U) == 0) {
            if (m_enabled && m_updateLookup) {
                if (m_debug)
                    Utils::dump(1U, "Network Received, BL RID", m_buffer, length);

                // update RID lists
                uint32_t len = (m_buffer[7U] << 16) | (m_buffer[8U] << 8) | (m_buffer[9U] << 0);
                uint32_t j = 0U;
                for (uint8_t i = 0; i < len; i++) {
                    uint32_t id = (m_buffer[11U + j] << 16) | (m_buffer[12U + j] << 8) | (m_buffer[13U + j] << 0);
                    m_ridLookup->toggleEntry(id, false);
                    j += 4U;
                }
            }
        }
        else if (::memcmp(m_buffer, TAG_MASTER_ACTIVE_TGS, 6U) == 0) {
            if (m_enabled && m_updateLookup) {
                if (m_debug)
                    Utils::dump(1U, "Network Received, ACTIVE TGS", m_buffer, length);

                // update TGID lists
                uint32_t len = (m_buffer[7U] << 16) | (m_buffer[8U] << 8) | (m_buffer[9U] << 0);
                uint32_t j = 0U;
                for (uint8_t i = 0; i < len; i++) {
                    uint32_t id = (m_buffer[11U + j] << 16) | (m_buffer[12U + j] << 8) | (m_buffer[13U + j] << 0);
                    uint8_t slot = (m_buffer[14U + j]);
                    
                    lookups::TalkgroupId tid = m_tidLookup->find(id);
                    if (tid.tgEnabled() == false && tid.tgDefault() == true) {
                        LogMessage(LOG_NET, "Activated TG %u TS %u in TGID table", id, slot);
                    }

                    m_tidLookup->addEntry(id, slot, true);
                    j += 5U;
                }
            }
        }
        else if (::memcmp(m_buffer, TAG_MASTER_DEACTIVE_TGS, 7U) == 0) {
            if (m_enabled && m_updateLookup) {
                if (m_debug)
                    Utils::dump(1U, "Network Received, DEACTIVE TGS", m_buffer, length);

                // update TGID lists
                uint32_t len = (m_buffer[7U] << 16) | (m_buffer[8U] << 8) | (m_buffer[9U] << 0);
                uint32_t j = 0U;
                for (uint8_t i = 0; i < len; i++) {
                    uint32_t id = (m_buffer[11U + j] << 16) | (m_buffer[12U + j] << 8) | (m_buffer[13U + j] << 0);
                    uint8_t slot = (m_buffer[14U + j]);

                    lookups::TalkgroupId tid = m_tidLookup->find(id);
                    if (tid.tgEnabled() == true && tid.tgDefault() == false) {
                        LogMessage(LOG_NET, "Deactivated TG %u TS %u in TGID table", id, slot);
                        m_tidLookup->addEntry(id, slot, false);
                    }

                    j += 5U;
                }
            }
        }
        else if (::memcmp(m_buffer, TAG_MASTER_NAK, 6U) == 0) {
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
        else if (::memcmp(m_buffer, TAG_REPEATER_ACK, 6U) == 0) {
            switch (m_status) {
                case NET_STAT_WAITING_LOGIN:
                    LogDebug(LOG_NET, "Sending authorisation");
                    ::memcpy(m_salt, m_buffer + 6U, sizeof(uint32_t));
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
        else if (::memcmp(m_buffer, TAG_MASTER_CLOSING, 5U) == 0) {
            LogError(LOG_NET, "Master is closing down");
            close();
            open();
        }
        else if (::memcmp(m_buffer, TAG_MASTER_PONG, 7U) == 0) {
            m_timeoutTimer.start();
        }
        else {
            Utils::dump("Unknown packet from the master", m_buffer, length);
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
        __SET_UINT32(m_id, buffer, 5U);
        write(buffer, 9U);
    }

    m_socket.close();

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
    uint8_t buffer[8U];

    ::memcpy(buffer + 0U, TAG_REPEATER_LOGIN, 4U);
    __SET_UINT32(m_id, buffer, 4U);

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, Login", buffer, 8U);

    return write(buffer, 8U);
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
    __SET_UINT32(m_id, out, 4U);

    edac::SHA256 sha256;
    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out + 8U);

    delete[] in;

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, Authorisation", out, 40U);

    return write(out, 40U);
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
    rcon["password"].set<std::string>(m_rconPassword);                              // RCON Password
    rcon["port"].set<uint16_t>(m_rconPort);                                         // RCON Port
    config["rcon"].set<json::object>(rcon);

    config["software"].set<std::string>(std::string(software));                     // Software ID

    json::value v = json::value(config);
    std::string json = v.serialize();

    char* buffer = new char[json.length() + 8U];

    ::memcpy(buffer + 0U, TAG_REPEATER_CONFIG, 4U);
    __SET_UINT32(m_id, buffer, 4U);
    ::sprintf(buffer + 8U, "%s", json.c_str());

    if (m_debug) {
        Utils::dump(1U, "Network Transmitted, Configuration", (uint8_t*)buffer, json.length() + 8U);
    }

    bool ret = write((uint8_t*)buffer, json.length() + 8U);
    delete[] buffer;
    return ret;
}

/// <summary>
/// Writes a network stay-alive ping.
/// </summary>
bool Network::writePing()
{
    uint8_t buffer[11U];

    ::memcpy(buffer + 0U, TAG_REPEATER_PING, 7U);
    __SET_UINT32(m_id, buffer, 7U);

    if (m_debug)
        Utils::dump(1U, "Network Transmitted, Ping", buffer, 11U);

    return write(buffer, 11U);
}
