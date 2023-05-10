/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#include "host/fne/HostFNE.h"
#include "network/FNENetwork.h"
#include "network/json/json.h"
#include "Log.h"
#include "StopWatch.h"
#include "Utils.h"

using namespace network;

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <chrono>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the FNENetwork class.
/// </summary>
/// <param name="host"></param>
/// <param name="address">Network Hostname/IP address to listen on.</param>
/// <param name="port">Network port number.</param>
/// <param name="password">Network authentication password.</param>
/// <param name="dmr">Flag indicating whether DMR is enabled.</param>
/// <param name="p25">Flag indicating whether P25 is enabled.</param>
/// <param name="nxdn">Flag indicating whether NXDN is enabled.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="pingTime"></param>
FNENetwork::FNENetwork(HostFNE* host, const std::string& address, uint16_t port, const std::string& password,
    bool debug, bool dmr, bool p25, bool nxdn, bool allowActivityTransfer, bool allowDiagnosticTransfer, uint32_t pingTime) :
    BaseNetwork(0U, 0U, true, debug, true, true, allowActivityTransfer, allowDiagnosticTransfer),
    m_host(host),
    m_address(address),
    m_port(port),
    m_password(password),
    m_enabled(false),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_nxdnEnabled(nxdn),
    m_ridLookup(nullptr),
    m_routingLookup(nullptr),
    m_peers(),
    m_maintainenceTimer(1000U, pingTime)
{
    assert(host != nullptr);
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());
}

/// <summary>
/// Finalizes a instance of the FNENetwork class.
/// </summary>
FNENetwork::~FNENetwork()
{
    /* stub */
}

/// <summary>
/// Sets the instances of the Radio ID and Routing Rules lookup tables.
/// </summary>
/// <param name="ridLookup">Radio ID Lookup Table Instance</param>
/// <param name="routingLookup">Routing Rules Lookup Table Instance</param>
void FNENetwork::setLookups(lookups::RadioIdLookup* ridLookup, lookups::RoutingRulesLookup* routingLookup)
{
    m_ridLookup = ridLookup;
    m_routingLookup = routingLookup;
}

/// <summary>
/// Gets the current status of the network.
/// </summary>
/// <returns></returns>
uint8_t FNENetwork::getStatus()
{
    return m_status;
}

/// <summary>
/// Updates the timer by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
void FNENetwork::clock(uint32_t ms)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    m_maintainenceTimer.clock(ms);
    if (m_maintainenceTimer.isRunning() && m_maintainenceTimer.hasExpired()) {
        // check to see if any peers have been quiet (no ping) longer than allowed
        std::vector<uint32_t> peersToRemove = std::vector<uint32_t>();
        for (auto peer : m_peers) {
            uint32_t id = peer.first;
            FNEPeerConnection peerConn = peer.second;

            uint64_t dt = peerConn.lastPing() + (m_host->m_pingTime * m_host->m_maxMissedPings);
            if (dt < now) {
                LogInfo(LOG_NET, "PEER %u timed out", id);
                peersToRemove.push_back(id);
            }
        }

        // remove any peers
        for (uint32_t peerId : peersToRemove) {
            m_peers.erase(peerId);
        }

        m_maintainenceTimer.start();
    }

    sockaddr_storage address;
    uint32_t addrLen;
    int length = m_socket.read(m_buffer, DATA_PACKET_LENGTH, address, addrLen);
    if (length > 0) {
        if (!m_enabled) {
            return;
        }

        if (m_debug)
            Utils::dump(1U, "Network Received", m_buffer, length);

        if (length < 4) {
            LogWarning(LOG_NET, "Malformed packet (from %s:%s)", UDPSocket::address(address).c_str(), UDPSocket::port(address));
            Utils::dump(1U, "Network Received", m_buffer, length);
            return;
        }

        if (::memcmp(m_buffer, TAG_DMR_DATA, 4U) == 0) {                        // Encapsulated DMR data frame
            // TODO TODO TODO
            // TODO: handle DMR data
        }
        else if (::memcmp(m_buffer, TAG_P25_DATA, 4U) == 0) {                   // Encapsulated P25 data frame
            // TODO TODO TODO
            // TODO: handle P25 data
        }
        else if (::memcmp(m_buffer, TAG_NXDN_DATA, 4U) == 0) {                  // Encapsulated NXDN data frame
            // TODO TODO TODO
            // TODO: handle NXDN data
        }
        else if (::memcmp(m_buffer, TAG_REPEATER_LOGIN, 4U) == 0) {             // Repeater Login
            uint32_t peerId = __GET_UINT32(m_buffer, 4U);
            if (peerId > 0 && (m_peers.find(peerId) == m_peers.end())) {
                FNEPeerConnection connection = FNEPeerConnection(peerId, address, addrLen);

                std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
                connection.salt(dist(m_random));

                LogInfo(LOG_NET, "Repeater logging in with PEER %u, %s:%u", peerId, connection.address().c_str(), connection.port());

                // transmit salt to peer
                uint8_t buffer[DATA_PACKET_LENGTH];
                ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

                ::memcpy(buffer + 0U, TAG_REPEATER_ACK, 6U);
                __SET_UINT32(connection.salt(), buffer, 6U);
                write(buffer, 10U, address, addrLen);

                connection.connectionState(NET_STAT_WAITING_AUTHORISATION);
                m_peers[peerId] = connection;

                LogInfo(LOG_NET, "Challenge response send to PEER %u for login", peerId);
            }
            else {
                writePeerNAK(peerId, TAG_REPEATER_LOGIN, address, addrLen);
            }
        }
        else if (::memcmp(m_buffer, TAG_REPEATER_AUTH, 4U) == 0) {              // Repeater Authentication
            uint32_t peerId = __GET_UINT32(m_buffer, 4U);
            if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                FNEPeerConnection connection = m_peers[peerId];
                connection.lastPing(now);

                if (connection.connectionState() == NET_STAT_WAITING_AUTHORISATION) {
                    // get the hash from the frame message
                    uint8_t hash[length - 8U];
                    ::memset(hash, 0x00U, length - 8U);
                    ::memcpy(hash, m_buffer + 8U, length - 8U);

                    // generate our own hash
                    size_t size = m_password.size();
                    uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
                    ::memcpy(in, m_salt, sizeof(uint32_t));
                    for (size_t i = 0U; i < size; i++)
                        in[i + sizeof(uint32_t)] = m_password.at(i);

                    uint8_t out[32U];
                    edac::SHA256 sha256;
                    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

                    delete[] in;

                    // validate hash
                    bool valid = false;
                    if (length - 8U == 32U) {
                        valid = true;
                        for (uint8_t i = 0; i < 32U; i++) {
                            if (hash[i] != out[i]) {
                                valid = false;
                                break;
                            }
                        }
                    }

                    if (valid) {
                        connection.connectionState(NET_STAT_WAITING_CONFIG);
                        LogInfo(LOG_NET, "PEER %u has completed the login exchange", peerId);
                    }
                    else {
                        LogWarning(LOG_NET, "PEER %u has failed the login exchange", peerId);
                        writePeerNAK(peerId, TAG_REPEATER_AUTH);
                        auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
                        if (it != m_peers.end()) {
                            m_peers.erase(peerId);
                        }
                    }

                    m_peers[peerId] = connection;
                }
                else {
                    LogWarning(LOG_NET, "PEER %u tried login exchange while in an incorrect state?", peerId);
                    writePeerNAK(peerId, TAG_REPEATER_AUTH);
                    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
                    if (it != m_peers.end()) {
                        m_peers.erase(peerId);
                    }
                }
            }
            else {
                writePeerNAK(peerId, TAG_REPEATER_AUTH, address, addrLen);
            }
        }
        else if (::memcmp(m_buffer, TAG_REPEATER_CONFIG, 4U) == 0) {            // Repeater Configuration
            uint32_t peerId = __GET_UINT32(m_buffer, 4U);
            if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                FNEPeerConnection connection = m_peers[peerId];
                connection.lastPing(now);

                if (connection.connectionState() == NET_STAT_WAITING_CONFIG) {
                    uint8_t rawPayload[length - 8U];
                    ::memset(rawPayload, 0x00U, length - 8U);
                    ::memcpy(rawPayload, m_buffer + 8U, length - 8U);
                    std::string payload(rawPayload, rawPayload + sizeof(rawPayload));

                    // parse JSON body
                    json::value v;
                    std::string err = json::parse(v, payload);
                    if (!err.empty()) {
                        LogWarning(LOG_NET, "PEER %u has supplied invalid configuration data", peerId);
                        writePeerNAK(peerId, TAG_REPEATER_AUTH);
                        auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
                        if (it != m_peers.end()) {
                            m_peers.erase(peerId);
                        }
                    }
                    else  {
                        // ensure parsed JSON is an object
                        if (!v.is<json::object>()) {
                            LogWarning(LOG_NET, "PEER %u has supplied invalid configuration data", peerId);
                            writePeerNAK(peerId, TAG_REPEATER_AUTH);
                            auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
                            if (it != m_peers.end()) {
                                m_peers.erase(peerId);
                            }
                        }
                        else {
                            connection.config(v.get<json::object>());
                            connection.connectionState(NET_STAT_RUNNING);
                            connection.connected(true);
                            connection.pingsReceived(0U);
                            connection.lastPing(now);
                            m_peers[peerId] = connection;

                            writePeerACK(peerId);
                            LogInfo(LOG_NET, "PEER %u has completed the configuration exchange", peerId);

                            // TODO TODO TODO
                            // TODO: handle peer connected
                        }
                    }
                }
                else {
                    LogWarning(LOG_NET, "PEER %u tried login exchange while in an incorrect state?", peerId);
                    writePeerNAK(peerId, TAG_REPEATER_CONFIG);
                    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
                    if (it != m_peers.end()) {
                        m_peers.erase(peerId);
                    }
                }
            }
            else {
                writePeerNAK(peerId, TAG_REPEATER_CONFIG, address, addrLen);
            }
        }
        else if (::memcmp(m_buffer, TAG_REPEATER_CLOSING, 5U) == 0) {           // Repeater Closing (Disconnect)
            uint32_t peerId = __GET_UINT32(m_buffer, 5U);
            if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                FNEPeerConnection connection = m_peers[peerId];
                std::string ip = UDPSocket::address(address);

                // validate peer (simple validation really)
                if (connection.connected() && connection.address() == ip) {
                    LogInfo(LOG_NET, "PEER %u is closing down", peerId);

                    // TODO TODO TODO
                    // TODO: handle peer disconnected

                    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
                    if (it != m_peers.end()) {
                        m_peers.erase(peerId);
                    }
                }
            }
        }
        else if (::memcmp(m_buffer, TAG_REPEATER_PING, 7U) == 0) {              // Repeater Ping
            uint32_t peerId = __GET_UINT32(m_buffer, 7U);
            if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                FNEPeerConnection connection = m_peers[peerId];
                std::string ip = UDPSocket::address(address);

                // validate peer (simple validation really)
                if (connection.connected() && connection.address() == ip) {
                    uint32_t pingsRx = connection.pingsReceived();
                    connection.pingsReceived(pingsRx++);
                    connection.lastPing(now);

                    m_peers[peerId] = connection;

                    if (m_debug) {
                        LogDebug(LOG_NET, "PEER %u ping received and answered", peerId);
                    }
                }
                else {
                    writePeerNAK(peerId, TAG_REPEATER_PING);
                }
            }
        }
        else if (::memcmp(m_buffer, TAG_REPEATER_GRANT, 7U) == 0) {             // Repeater Grant Request
            uint32_t peerId = __GET_UINT32(m_buffer, 7U);
            if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                FNEPeerConnection connection = m_peers[peerId];
                std::string ip = UDPSocket::address(address);

                // validate peer (simple validation really)
                if (connection.connected() && connection.address() == ip) {
                    // TODO TODO TODO
                    // TODO: handle repeater grant request
                }
                else {
                    writePeerNAK(peerId, TAG_REPEATER_GRANT);
                }
            }
        }
        else if (::memcmp(m_buffer, TAG_TRANSFER_ACT_LOG, 8U) == 0) {           // Peer Activity Log Transfer
            if (m_allowActivityTransfer) {
                uint32_t peerId = __GET_UINT32(m_buffer, 8U);
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection connection = m_peers[peerId];
                    std::string ip = UDPSocket::address(address);

                    // validate peer (simple validation really)
                    if (connection.connected() && connection.address() == ip) {
                        uint8_t rawPayload[length - 11U];
                        ::memset(rawPayload, 0x00U, length - 11U);
                        ::memcpy(rawPayload, m_buffer + 11U, length - 11U);
                        std::string payload(rawPayload, rawPayload + sizeof(rawPayload));

                        std::stringstream ss;
                        ss << peerId << " " << payload;

                        ::ActivityLog("", false, ss.str().c_str());
                    }
                    else {
                        writePeerNAK(peerId, TAG_TRANSFER_ACT_LOG);
                    }
                }
            }
        }
        else if (::memcmp(m_buffer, TAG_TRANSFER_DIAG_LOG, 8U) == 0) {          // Peer Diagnostic Log Transfer
            if (m_allowDiagnosticTransfer) {
                uint32_t peerId = __GET_UINT32(m_buffer, 8U);
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection connection = m_peers[peerId];
                    std::string ip = UDPSocket::address(address);

                    // validate peer (simple validation really)
                    if (connection.connected() && connection.address() == ip) {
                        uint8_t rawPayload[length - 11U];
                        ::memset(rawPayload, 0x00U, length - 11U);
                        ::memcpy(rawPayload, m_buffer + 11U, length - 11U);
                        std::string payload(rawPayload, rawPayload + sizeof(rawPayload));

                        // TODO TODO TODO
                        // TODO: handle diag log xfer
                    }
                    else {
                        writePeerNAK(peerId, TAG_TRANSFER_DIAG_LOG);
                    }
                }
            }
        }
        else {
            Utils::dump("Unknown packet from the peer", m_buffer, length);
        }
    }

    return;
}

/// <summary>
/// Opens connection to the network.
/// </summary>
/// <returns></returns>
bool FNENetwork::open()
{
    if (m_debug)
        LogMessage(LOG_NET, "Opening Network");

    m_status = NET_STAT_MST_RUNNING;
    m_maintainenceTimer.start();

    m_socket = UDPSocket(m_address, m_port);
    bool ret = m_socket.open();
    if (!ret) {
        m_status = NET_STAT_INVALID;
    }

    return ret;
}

/// <summary>
/// Sets flag enabling network communication.
/// </summary>
/// <param name="enabled"></param>
void FNENetwork::enable(bool enabled)
{
    m_enabled = enabled;
}

/// <summary>
/// Closes connection to the network.
/// </summary>
void FNENetwork::close()
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
/// Helper to send a raw message to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="buffer">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
bool FNENetwork::writePeer(uint32_t peerId, const uint8_t* data, uint32_t length)
{
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        sockaddr_storage address = m_peers[peerId].socketStorage();
        uint32_t addrLen = m_peers[peerId].sockStorageLen();
        return write(data, length, address, addrLen);
    }

    return false;
}

/// <summary>
/// Helper to send a tagged message to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="tag"></param>
/// <param name="buffer">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
bool FNENetwork::writePeerTagged(uint32_t peerId, const char* tag, const uint8_t* data, uint32_t length)
{
    assert(peerId > 0);
    assert(tag != nullptr);

    if (strlen(tag) + length > DATA_PACKET_LENGTH) {
        return false;
    }

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, tag, strlen(tag));

    ::memcpy(buffer + strlen(tag), data, length);

    uint32_t len = length + strlen(tag);
    return writePeer(peerId, buffer, len);
}

/// <summary>
/// Helper to send a ACK response to the specified peer.
/// </summary>
/// <param name="peerId"></param>
bool FNENetwork::writePeerACK(uint32_t peerId)
{
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_REPEATER_ACK, 6U);
    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID
    
    return writePeer(peerId, buffer, 10U);
}

/// <summary>
/// Helper to send a NAK response to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="tag"></param>
bool FNENetwork::writePeerNAK(uint32_t peerId, const char* tag)
{
    assert(peerId > 0);
    assert(tag != nullptr);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_MASTER_NAK, 6U);
    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID

    LogWarning(LOG_NET, "%s from unauth PEER %u", tag, peerId);
    return writePeer(peerId, buffer, 10U);
}

/// <summary>
/// Helper to send a NAK response to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="tag"></param>
/// <param name="addr"></param>
/// <param name="addrLen"></param>
bool FNENetwork::writePeerNAK(uint32_t peerId, const char* tag, sockaddr_storage& addr, uint32_t addrLen)
{
    assert(peerId > 0);
    assert(tag != nullptr);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_MASTER_NAK, 6U);
    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID

    LogWarning(LOG_NET, "%s from unauth PEER %u", tag, peerId);
    return write(buffer, 10U, addr, addrLen);
}

/// <summary>
/// Helper to send a raw message to the connected peers.
/// </summary>
/// <param name="buffer">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
void FNENetwork::writePeers(const uint8_t* data, uint32_t length)
{
    for (auto peer : m_peers) {
        uint32_t peerId = peer.first;
        bool ret = writePeer(peerId, data, length);
        if (!ret) {
            LogError(LOG_NET, "Failed to write data to PEER %u", peerId);
        }
    }
}

/// <summary>
/// Helper to send a tagged message to the connected peers.
/// </summary>
/// <param name="tag"></param>
/// <param name="buffer">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
void FNENetwork::writePeersTagged(const char* tag, const uint8_t* data, uint32_t length)
{
    assert(tag != nullptr);

    if (strlen(tag) + length > DATA_PACKET_LENGTH) {
        return;
    }

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, tag, strlen(tag));

    ::memcpy(buffer + strlen(tag), data, length);

    uint32_t len = length + strlen(tag);
    writePeers(buffer, len);
}