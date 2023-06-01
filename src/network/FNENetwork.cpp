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
#include "network/fne/TagDMRData.h"
#include "network/fne/TagP25Data.h"
#include "network/fne/TagNXDNData.h"
#include "network/json/json.h"
#include "Log.h"
#include "StopWatch.h"
#include "Utils.h"

using namespace network;
using namespace network::fne;

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
/// <param name="peerId">Unique ID on the network.</param>
/// <param name="password">Network authentication password.</param>
/// <param name="debug">Flag indicating whether network debug is enabled.</param>
/// <param name="verbose">Flag indicating whether network verbose logging is enabled.</param>
/// <param name="dmr">Flag indicating whether DMR is enabled.</param>
/// <param name="p25">Flag indicating whether P25 is enabled.</param>
/// <param name="nxdn">Flag indicating whether NXDN is enabled.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="trafficRepeat">Flag indicating if traffic should be repeated from this master.</param>
/// <param name="pingTime"></param>
/// <param name="updateLookupTime"></param>
FNENetwork::FNENetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
    bool debug, bool verbose, bool dmr, bool p25, bool nxdn, bool allowActivityTransfer, bool allowDiagnosticTransfer,
    uint32_t pingTime, uint32_t updateLookupTime) :
    BaseNetwork(peerId, true, debug, true, true, allowActivityTransfer, allowDiagnosticTransfer),
    m_tagDMR(nullptr),
    m_tagP25(nullptr),
    m_tagNXDN(nullptr),
    m_host(host),
    m_address(address),
    m_port(port),
    m_password(password),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_nxdnEnabled(nxdn),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_status(NET_STAT_INVALID),
    m_peers(),
    m_maintainenceTimer(1000U, pingTime),
    m_updateLookupTimer(1000U, (updateLookupTime * 60U)),
    m_verbose(verbose)
{
    assert(host != nullptr);
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    m_tagDMR = new TagDMRData(this, debug);
    m_tagP25 = new TagP25Data(this, debug);
    m_tagNXDN = new TagNXDNData(this, debug);
}

/// <summary>
/// Finalizes a instance of the FNENetwork class.
/// </summary>
FNENetwork::~FNENetwork()
{
    delete m_tagDMR;
    delete m_tagP25;
    delete m_tagNXDN;
}

/// <summary>
/// Sets the instances of the Radio ID and Talkgroup Rules lookup tables.
/// </summary>
/// <param name="ridLookup">Radio ID Lookup Table Instance</param>
/// <param name="tidLookup">Talkgroup Rules Lookup Table Instance</param>
void FNENetwork::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
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
                LogInfoEx(LOG_NET, "PEER %u timed out", id);
                peersToRemove.push_back(id);
            }
        }

        // remove any peers
        for (uint32_t peerId : peersToRemove) {
            m_peers.erase(peerId);
        }

        m_maintainenceTimer.start();
    }

    m_updateLookupTimer.clock(ms);
    if (m_updateLookupTimer.isRunning() && m_updateLookupTimer.hasExpired()) {
        writeWhitelistRIDs();
        writeBlacklistRIDs();
        m_frameQueue->flushQueue();

        writeTGIDs();
        writeDeactiveTGIDs();
        m_frameQueue->flushQueue();

        m_updateLookupTimer.start();
    }

    sockaddr_storage address;
    uint32_t addrLen;
    frame::RTPHeader rtpHeader = frame::RTPHeader();
    frame::RTPFNEHeader fneHeader;
    int length = 0U;

    // read message
    UInt8Array buffer = m_frameQueue->read(length, address, addrLen, &rtpHeader, &fneHeader);
    if (length > 0) {
        if (m_debug)
            Utils::dump(1U, "Network Message", buffer.get(), length);

        uint32_t peerId = fneHeader.getPeerId();
        uint32_t streamId = fneHeader.getStreamId();

        // update current peer stream ID
        if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
            FNEPeerConnection connection = m_peers[peerId];

            uint16_t pktSeq = rtpHeader.getSequence();

            if ((connection.currStreamId() == streamId) && (pktSeq != connection.pktNextSeq())) {
                LogWarning(LOG_NET, "PEER %u Stream %u out-of-sequence; %u != %u", peerId, streamId, pktSeq, connection.pktNextSeq());
            }

            connection.currStreamId(streamId);
            connection.pktLastSeq(pktSeq);
            connection.pktNextSeq(pktSeq + 1);
            if (connection.pktNextSeq() > UINT16_MAX) {
                connection.pktNextSeq(0U);
            }

            m_peers[peerId] = connection;
        }

        // process incoming message frame opcodes
        switch (fneHeader.getFunction()) {
        case NET_FUNC_PROTOCOL:
            {
                if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_DMR) {           // Encapsulated DMR data frame
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection connection = m_peers[peerId];
                        std::string ip = UDPSocket::address(address);

                        // validate peer (simple validation really)
                        if (connection.connected() && connection.address() == ip) {
#if defined(ENABLE_DMR)
                            if (m_dmrEnabled) {
                                if (m_tagDMR != nullptr) {
                                    m_tagDMR->processFrame(buffer.get(), length, peerId, streamId);
                                }
                            }
#endif // defined(ENABLE_DMR)
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_P25) {      // Encapsulated P25 data frame
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection connection = m_peers[peerId];
                        std::string ip = UDPSocket::address(address);

                        // validate peer (simple validation really)
                        if (connection.connected() && connection.address() == ip) {
#if defined(ENABLE_P25)
                            if (m_p25Enabled) {
                                if (m_tagP25 != nullptr) {
                                    m_tagP25->processFrame(buffer.get(), length, peerId, streamId);
                                }
                            }
#endif // defined(ENABLE_P25)
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_NXDN) {     // Encapsulated NXDN data frame
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection connection = m_peers[peerId];
                        std::string ip = UDPSocket::address(address);

                        // validate peer (simple validation really)
                        if (connection.connected() && connection.address() == ip) {
#if defined(ENABLE_NXDN)        
                            if (m_nxdnEnabled) {
                                if (m_tagNXDN != nullptr) {
                                    m_tagNXDN->processFrame(buffer.get(), length, peerId, streamId);
                                }
                            }
#endif // defined(ENABLE_NXDN)
                        }
                    }
                }
                else {
                    Utils::dump("Unknown protocol opcode from peer", buffer.get(), length);
                }
            }
            break;

        case NET_FUNC_RPTL:                                                             // Repeater Login
            {
                if (peerId > 0 && (m_peers.find(peerId) == m_peers.end())) {
                    FNEPeerConnection connection = FNEPeerConnection(peerId, address, addrLen);

                    std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
                    connection.salt(dist(m_random));

                    LogInfoEx(LOG_NET, "Repeater logging in with PEER %u, %s:%u", peerId, connection.address().c_str(), connection.port());

                    connection.connectionState(NET_STAT_WAITING_AUTHORISATION);
                    m_peers[peerId] = connection;


                    // transmit salt to peer
                    uint8_t salt[4U];
                    ::memset(salt, 0x00U, 4U);
                    __SET_UINT32(connection.salt(), salt, 0U);

                    writePeerACK(peerId, salt, 4U);
                    LogInfoEx(LOG_NET, "Challenge response send to PEER %u for login", peerId);
                }
                else {
                    writePeerNAK(peerId, TAG_REPEATER_LOGIN, address, addrLen);
                }
            }
            break;
        case NET_FUNC_RPTK:                                                             // Repeater Authentication
            {
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection connection = m_peers[peerId];
                    connection.lastPing(now);

                    if (connection.connectionState() == NET_STAT_WAITING_AUTHORISATION) {
                        // get the hash from the frame message
                        uint8_t hash[length - 8U];
                        ::memset(hash, 0x00U, length - 8U);
                        ::memcpy(hash, buffer.get() + 8U, length - 8U);

                        // generate our own hash
                        uint8_t salt[4U];
                        ::memset(salt, 0x00U, 4U);
                        __SET_UINT32(connection.salt(), salt, 0U);

                        size_t size = m_password.size();
                        uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
                        ::memcpy(in, salt, sizeof(uint32_t));
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
                            writePeerACK(peerId);
                            LogInfoEx(LOG_NET, "PEER %u has completed the login exchange", peerId);
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
            break;
        case NET_FUNC_RPTC:                                                             // Repeater Configuration
            {
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection connection = m_peers[peerId];
                    connection.lastPing(now);

                    if (connection.connectionState() == NET_STAT_WAITING_CONFIG) {
                        uint8_t rawPayload[length - 8U];
                        ::memset(rawPayload, 0x00U, length - 8U);
                        ::memcpy(rawPayload, buffer.get() + 8U, length - 8U);
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
                                LogInfoEx(LOG_NET, "PEER %u has completed the configuration exchange", peerId);

                                // queue final update messages and flush
                                writeWhitelistRIDs(peerId, true);
                                writeBlacklistRIDs(peerId, true);
                                m_frameQueue->flushQueue();

                                writeTGIDs(peerId, true);
                                writeDeactiveTGIDs(peerId, true);
                                m_frameQueue->flushQueue();
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
            break;

        case NET_FUNC_RPT_CLOSING:                                                      // Repeater Closing (Disconnect)
            {
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection connection = m_peers[peerId];
                    std::string ip = UDPSocket::address(address);

                    // validate peer (simple validation really)
                    if (connection.connected() && connection.address() == ip) {
                        LogInfoEx(LOG_NET, "PEER %u is closing down", peerId);

                        auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
                        if (it != m_peers.end()) {
                            m_peers.erase(peerId);
                        }
                    }
                }
            }
            break;
        case NET_FUNC_PING:                                                             // Repeater Ping
            {
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection connection = m_peers[peerId];
                    std::string ip = UDPSocket::address(address);

                    // validate peer (simple validation really)
                    if (connection.connected() && connection.address() == ip) {
                        uint32_t pingsRx = connection.pingsReceived();
                        connection.pingsReceived(pingsRx++);
                        connection.lastPing(now);

                        m_peers[peerId] = connection;
                        writePeerTagged(peerId, { NET_FUNC_PONG, NET_SUBFUNC_NOP }, TAG_MASTER_PONG);

                        if (m_debug) {
                            LogDebug(LOG_NET, "PEER %u ping received and answered", peerId);
                        }
                    }
                    else {
                        writePeerNAK(peerId, TAG_REPEATER_PING);
                    }
                }
            }
            break;

        case NET_FUNC_GRANT:                                                            // Repeater Grant Request
            {
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
            break;

        case NET_FUNC_TRANSFER:
            {
                if (fneHeader.getSubFunction() == NET_TRANSFER_SUBFUNC_ACTIVITY) {      // Peer Activity Log Transfer
                    if (m_allowActivityTransfer) {
                        if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                            FNEPeerConnection connection = m_peers[peerId];
                            std::string ip = UDPSocket::address(address);

                            // validate peer (simple validation really)
                            if (connection.connected() && connection.address() == ip) {
                                uint8_t rawPayload[length - 11U];
                                ::memset(rawPayload, 0x00U, length - 11U);
                                ::memcpy(rawPayload, buffer.get() + 11U, length - 11U);
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
                else if (fneHeader.getSubFunction() == NET_TRANSFER_SUBFUNC_DIAG) {     // Peer Diagnostic Log Transfer
                    if (m_allowDiagnosticTransfer) {
                        if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                            FNEPeerConnection connection = m_peers[peerId];
                            std::string ip = UDPSocket::address(address);

                            // validate peer (simple validation really)
                            if (connection.connected() && connection.address() == ip) {
                                uint8_t rawPayload[length - 12U];
                                ::memset(rawPayload, 0x00U, length - 12U);
                                ::memcpy(rawPayload, buffer.get() + 12U, length - 12U);
                                std::string payload(rawPayload, rawPayload + sizeof(rawPayload));

                                bool currState = g_disableTimeDisplay;
                                g_disableTimeDisplay = true;
                                ::Log(9999U, nullptr, "%u %s", peerId, payload.c_str());
                                g_disableTimeDisplay = currState;
                            }
                            else {
                                writePeerNAK(peerId, TAG_TRANSFER_DIAG_LOG);
                            }
                        }
                    }
                }
                else {
                    Utils::dump("Unknown transfer opcode from the peer", buffer.get(), length);
                }
            }
            break;

        default:
            Utils::dump("Unknown opcode from the peer", buffer.get(), length);
            break;
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

    m_socket = new UDPSocket(m_address, m_port);

    // reinitialize the frame queue
    if (m_frameQueue != nullptr) {
        delete m_frameQueue;
        m_frameQueue = new FrameQueue(m_socket, m_peerId, m_debug);
    }

    bool ret = m_socket->open();
    if (!ret) {
        m_status = NET_STAT_INVALID;
    }

    return ret;
}

/// <summary>
/// Closes connection to the network.
/// </summary>
void FNENetwork::close()
{
    if (m_debug)
        LogMessage(LOG_NET, "Closing Network");

    if (m_status == NET_STAT_MST_RUNNING) {
        uint8_t buffer[9U];
        ::memcpy(buffer + 0U, TAG_MASTER_CLOSING, 5U);
        writePeers({ NET_FUNC_MST_CLOSING, NET_SUBFUNC_NOP }, buffer, 9U);
    }

    m_socket->close();

    m_maintainenceTimer.stop();
    m_updateLookupTimer.stop();

    m_status = NET_STAT_INVALID;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to send the list of whitelisted RIDs to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="queueOnly"></param>
void FNENetwork::writeWhitelistRIDs(uint32_t peerId, bool queueOnly)
{
    // send radio ID white/black lists
    std::vector<uint32_t> ridWhitelist;

    auto ridLookups = m_ridLookup->table();
    for (auto entry : ridLookups) {
        uint32_t id = entry.first;
        if (entry.second.radioEnabled()) {
            ridWhitelist.push_back(id);
        }
    }

    if (ridWhitelist.size() == 0U) {
        return;
    }

    // build dataset
    uint8_t payload[4U + (ridWhitelist.size() * 4U)];
    ::memset(payload, 0x00U, 4U + (ridWhitelist.size() * 4U));

    __SET_UINT32(ridWhitelist.size(), payload, 0U);

    // write whitelisted IDs to whitelist payload
    uint32_t offs = 4U;
    for (uint32_t id : ridWhitelist) {
        if (m_debug)
            LogDebug(LOG_NET, "PEER %u Whitelisting RID %u", peerId, id);

        __SET_UINT32(id, payload, offs);
        offs += 4U;
    }

    writePeerTagged(peerId, { NET_FUNC_MASTER, NET_MASTER_SUBFUNC_WL_RID }, TAG_MASTER_WL_RID, 
        payload, 4U + (ridWhitelist.size() * 4U), queueOnly);
}

/// <summary>
/// Helper to send the list of whitelisted RIDs to connected peers.
/// </summary>
void FNENetwork::writeWhitelistRIDs()
{
    if (m_ridLookup->table().size() == 0U) {
        return;
    }

    for (auto peer : m_peers) {
        writeWhitelistRIDs(peer.first, true);
    }
}

/// <summary>
/// Helper to send the list of whitelisted RIDs to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="queueOnly"></param>
void FNENetwork::writeBlacklistRIDs(uint32_t peerId, bool queueOnly)
{
    // send radio ID blacklist
    std::vector<uint32_t> ridBlacklist;

    auto ridLookups = m_ridLookup->table();
    for (auto entry : ridLookups) {
        uint32_t id = entry.first;
        if (!entry.second.radioEnabled()) {
            ridBlacklist.push_back(id);
        }
    }

    if (ridBlacklist.size() == 0U) {
        return;
    }

    // build dataset
    uint8_t payload[4U + (ridBlacklist.size() * 4U)];
    ::memset(payload, 0x00U, 4U + (ridBlacklist.size() * 4U));

    __SET_UINT32(ridBlacklist.size(), payload, 0U);

    // write blacklisted IDs to blacklist payload
    uint32_t offs = 4U;
    for (uint32_t id : ridBlacklist) {
        if (m_debug)
            LogDebug(LOG_NET, "PEER %u Blacklisting RID %u", peerId, id);

        __SET_UINT32(id, payload, offs);
        offs += 4U;
    }

    writePeerTagged(peerId, { NET_FUNC_MASTER, NET_MASTER_SUBFUNC_BL_RID }, TAG_MASTER_BL_RID, 
        payload, 4U + (ridBlacklist.size() * 4U), queueOnly);
}

/// <summary>
/// Helper to send the list of whitelisted RIDs to connected peers.
/// </summary>
void FNENetwork::writeBlacklistRIDs()
{
    if (m_ridLookup->table().size() == 0U) {
        return;
    }

    for (auto peer : m_peers) {
        writeBlacklistRIDs(peer.first, true);
    }
}

/// <summary>
/// Helper to send the list of active TGIDs to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="queueOnly"></param>
void FNENetwork::writeTGIDs(uint32_t peerId, bool queueOnly)
{
    if (!m_tidLookup->sendTalkgroups()) {
        return;
    }

    std::vector<std::pair<uint32_t, uint8_t>> tgidList;
    auto groupVoice = m_tidLookup->groupVoice();
    for (auto entry : groupVoice) {
        if (entry.config().active()) {
            tgidList.push_back({ entry.source().tgId(), entry.source().tgSlot() });
        }
    }

    // build dataset
    uint8_t payload[4U + (tgidList.size() * 5U)];
    ::memset(payload, 0x00U, 4U + (tgidList.size() * 5U));

    __SET_UINT32(tgidList.size(), payload, 0U);

    // write talkgroup IDs to active TGID payload
    uint32_t offs = 4U;
    for (std::pair<uint32_t, uint8_t> tg : tgidList) {
        if (m_debug)
            LogDebug(LOG_NET, "PEER %u Activating TGID %u TS %u", peerId, tg.first, tg.second);
        __SET_UINT32(tg.first, payload, offs);
        payload[offs + 4U] = tg.second;
        offs += 5U;
    }

    writePeerTagged(peerId, { NET_FUNC_MASTER, NET_MASTER_SUBFUNC_ACTIVE_TGS }, TAG_MASTER_ACTIVE_TGS, 
        payload, 4U + (tgidList.size() * 5U), queueOnly);
}

/// <summary>
/// Helper to send the list of active TGIDs to connected peers.
/// </summary>
void FNENetwork::writeTGIDs()
{
    for (auto peer : m_peers) {
        writeTGIDs(peer.first, true);
    }
}

/// <summary>
/// Helper to send the list of deactivated TGIDs to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="queueOnly"></param>
void FNENetwork::writeDeactiveTGIDs(uint32_t peerId, bool queueOnly)
{
    if (!m_tidLookup->sendTalkgroups()) {
        return;
    }

    std::vector<std::pair<uint32_t, uint8_t>> tgidList;
    auto groupVoice = m_tidLookup->groupVoice();
    for (auto entry : groupVoice) {
        if (!entry.config().active()) {
            tgidList.push_back({ entry.source().tgId(), entry.source().tgSlot() });
        }
    }

    // build dataset
    uint8_t payload[4U + (tgidList.size() * 5U)];
    ::memset(payload, 0x00U, 4U + (tgidList.size() * 5U));

    __SET_UINT32(tgidList.size(), payload, 0U);

    // write talkgroup IDs to deactive TGID payload
    uint32_t offs = 4U;
    for (std::pair<uint32_t, uint8_t> tg : tgidList) {
        if (m_debug)
            LogDebug(LOG_NET, "PEER %u Deactivating TGID %u TS %u", peerId, tg.first, tg.second);
        __SET_UINT32(tg.first, payload, offs);
        payload[offs + 4U] = tg.second;
        offs += 5U;
    }

    writePeerTagged(peerId, { NET_FUNC_MASTER, NET_MASTER_SUBFUNC_DEACTIVE_TGS }, TAG_MASTER_DEACTIVE_TGS, 
        payload, 4U + (tgidList.size() * 5U), queueOnly);
}

/// <summary>
/// Helper to send the list of deactivated TGIDs to connected peers.
/// </summary>
void FNENetwork::writeDeactiveTGIDs()
{
    for (auto peer : m_peers) {
        writeDeactiveTGIDs(peer.first, true);
    }
}

/// <summary>
/// Helper to send a raw message to the specified peer.
/// </summary>
/// <param name="peerId">Peer ID.</param>
/// <param name="opcode">Opcode.</param>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
/// <param name="queueOnly"></param>
bool FNENetwork::writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, bool queueOnly)
{
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        uint32_t streamId = m_peers[peerId].currStreamId();
        sockaddr_storage addr = m_peers[peerId].socketStorage();
        uint32_t addrLen = m_peers[peerId].sockStorageLen();
        uint16_t pktSeq = m_peers[peerId].pktLastSeq();

        m_frameQueue->enqueueMessage(data, length, streamId, peerId, opcode, pktSeq, addr, addrLen);
        if (queueOnly)
            return true;
        return m_frameQueue->flushQueue();
    }

    return false;
}

/// <summary>
/// Helper to send a tagged message to the specified peer.
/// </summary>
/// <param name="peerId">Peer ID.</param>
/// <param name="opcode">Opcode.</param>
/// <param name="tag">Message tag.</param>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
/// <param name="queueOnly"></param>
bool FNENetwork::writePeerTagged(uint32_t peerId, FrameQueue::OpcodePair opcode, const char* tag, 
    const uint8_t* data, uint32_t length, bool queueOnly)
{
    assert(peerId > 0);
    assert(tag != nullptr);

    if (strlen(tag) + length > DATA_PACKET_LENGTH) {
        return false;
    }

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, tag, strlen(tag));
    __SET_UINT32(peerId, buffer, strlen(tag));                                  // Peer ID

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + strlen(tag), data, length);
    }

    uint32_t len = length + (strlen(tag) + 4U);
    return writePeer(peerId, opcode, buffer, len, queueOnly);
}

/// <summary>
/// Helper to send a ACK response to the specified peer.
/// </summary>
/// <param name="peerId"></param>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
bool FNENetwork::writePeerACK(uint32_t peerId, const uint8_t* data, uint32_t length)
{
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    ::memcpy(buffer + 0U, TAG_REPEATER_ACK, 6U);
    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + 6U, data, length);
    }

    return writePeer(peerId, { NET_FUNC_ACK, NET_SUBFUNC_NOP }, buffer, 10U + length);
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
    return writePeer(peerId, { NET_FUNC_NAK, NET_SUBFUNC_NOP }, buffer, 10U);
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
    
    m_frameQueue->enqueueMessage(buffer, 10U, createStreamId(), peerId,
        { NET_FUNC_NAK, NET_SUBFUNC_NOP }, 0U, addr, addrLen);
    return m_frameQueue->flushQueue();
}

/// <summary>
/// Helper to send a raw message to the connected peers.
/// </summary>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
void FNENetwork::writePeers(FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length)
{
    for (auto peer : m_peers) {
        uint32_t peerId = peer.first;
        uint32_t streamId = peer.second.currStreamId();
        sockaddr_storage addr = peer.second.socketStorage();
        uint32_t addrLen = peer.second.sockStorageLen();
        uint16_t pktSeq = peer.second.pktLastSeq();

        m_frameQueue->enqueueMessage(data, length, streamId, peerId, opcode, pktSeq, addr, addrLen);
    }

    m_frameQueue->flushQueue();
}

/// <summary>
/// Helper to send a tagged message to the connected peers.
/// </summary>
/// <param name="tag"></param>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
void FNENetwork::writePeersTagged(FrameQueue::OpcodePair opcode, const char* tag, const uint8_t* data, uint32_t length)
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
    writePeers(opcode, buffer, len);
}
