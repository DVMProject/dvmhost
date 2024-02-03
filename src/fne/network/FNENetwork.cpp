// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "fne/Defines.h"
#include "common/edac/SHA256.h"
#include "common/network/json/json.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/fne/TagDMRData.h"
#include "network/fne/TagP25Data.h"
#include "network/fne/TagNXDNData.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;
using namespace network::fne;

#include <cassert>
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
/// <param name="reportPeerPing">Flag indicating whether peer pinging is reported.</param>
/// <param name="dmr">Flag indicating whether DMR is enabled.</param>
/// <param name="p25">Flag indicating whether P25 is enabled.</param>
/// <param name="nxdn">Flag indicating whether NXDN is enabled.</param>
/// <param name="parrotDelay">Delay for end of call to parrot TG playback.</param>
/// <param name="parrotGrantDemand">Flag indicating whether a parrot TG will generate a grant demand.</param>
/// <param name="allowActivityTransfer">Flag indicating that the system activity logs will be sent to the network.</param>
/// <param name="allowDiagnosticTransfer">Flag indicating that the system diagnostic logs will be sent to the network.</param>
/// <param name="pingTime"></param>
/// <param name="updateLookupTime"></param>
FNENetwork::FNENetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
    bool debug, bool verbose, bool reportPeerPing, bool dmr, bool p25, bool nxdn, uint32_t parrotDelay, bool parrotGrantDemand,
    bool allowActivityTransfer, bool allowDiagnosticTransfer, uint32_t pingTime, uint32_t updateLookupTime) :
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
    m_parrotDelay(parrotDelay),
    m_parrotGrantDemand(parrotGrantDemand),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_status(NET_STAT_INVALID),
    m_peers(),
    m_peerAffiliations(),
    m_maintainenceTimer(1000U, pingTime),
    m_updateLookupTimer(1000U, (updateLookupTime * 60U)),
    m_forceListUpdate(false),
    m_callInProgress(false),
    m_disallowP25AdjStsBcast(false),
    m_reportPeerPing(reportPeerPing),
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
/// Helper to set configuration options.
/// </summary>
/// <param name="conf">Instance of the yaml::Node class.</param>
/// <param name="printOptions"></param>
void FNENetwork::setOptions(yaml::Node& conf, bool printOptions)
{
    m_disallowP25AdjStsBcast = conf["disallowP25AdjStsBcast"].as<bool>(false);

    if (printOptions) {
        LogInfo("    Disable P25 ADJ_STS_BCAST to external peers: %s", m_disallowP25AdjStsBcast ? "yes" : "no");
    }
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
/// Sets endpoint preshared encryption key.
/// </summary>
void FNENetwork::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
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
            FNEPeerConnection* connection = peer.second;
            if (connection != nullptr) {
                if (connection->connected()) {
                    uint64_t dt = connection->lastPing() + (m_host->m_pingTime * m_host->m_maxMissedPings);
                    if (dt < now) {
                        LogInfoEx(LOG_NET, "PEER %u timed out, dt = %u, now = %u", id, dt, now);
                        peersToRemove.push_back(id);
                    }
                }
            }
        }

        // remove any peers
        for (uint32_t peerId : peersToRemove) {
            FNEPeerConnection* connection = m_peers[peerId];
            m_peers.erase(peerId);
            if (connection != nullptr) {
                delete connection;
            }

            erasePeerAffiliations(peerId);
        }

        // roll the RTP timestamp if no call is in progress
        if (!m_callInProgress) {
            frame::RTPHeader::resetStartTime();
            m_frameQueue->clearTimestamps();
        }

        m_maintainenceTimer.start();
    }

    m_updateLookupTimer.clock(ms);
    if ((m_updateLookupTimer.isRunning() && m_updateLookupTimer.hasExpired()) || m_forceListUpdate) {
        writeWhitelistRIDs();
        writeBlacklistRIDs();
        m_frameQueue->flushQueue();

        writeTGIDs();
        writeDeactiveTGIDs();
        m_frameQueue->flushQueue();

        m_updateLookupTimer.start();
        m_forceListUpdate = false;
    }

    sockaddr_storage address;
    uint32_t addrLen;
    frame::RTPHeader rtpHeader;
    frame::RTPFNEHeader fneHeader;
    int length = 0U;

    // read message
    UInt8Array buffer = m_frameQueue->read(length, address, addrLen, &rtpHeader, &fneHeader);
    if (length > 0) {
        if (m_debug)
            Utils::dump(1U, "Network Message", buffer.get(), length);

        uint32_t peerId = fneHeader.getPeerId();
        uint32_t streamId = fneHeader.getStreamId();

        // update current peer packet sequence and stream ID
        if (peerId > 0 && (m_peers.find(peerId) != m_peers.end()) && streamId != 0U) {
            FNEPeerConnection* connection = m_peers[peerId];
            uint16_t pktSeq = rtpHeader.getSequence();

            if (connection != nullptr) {
                if (pktSeq == RTP_END_OF_CALL_SEQ) {
                    connection->pktLastSeq(pktSeq);
                    connection->pktNextSeq(0U);
                } else {
                    if ((connection->currStreamId() == streamId) && (pktSeq != connection->pktNextSeq())) {
                        LogWarning(LOG_NET, "PEER %u Stream %u out-of-sequence; %u != %u", peerId, streamId, pktSeq, connection->pktNextSeq());
                    }

                    connection->currStreamId(streamId);
                    connection->pktLastSeq(pktSeq);
                    connection->pktNextSeq(pktSeq + 1);
                    if (connection->pktNextSeq() > UINT16_MAX) {
                        connection->pktNextSeq(0U);
                    }
                }
            }

            m_peers[peerId] = connection;
        }

        // if we don't have a stream ID and are receiving call data -- throw an error and discard
        if (streamId == 0 && fneHeader.getFunction() == NET_FUNC_PROTOCOL)
        {
            LogError(LOG_NET, "PEER %u Malformed packet (no stream ID for a call?)", peerId);
            return;
        }

        // process incoming message frame opcodes
        switch (fneHeader.getFunction()) {
        case NET_FUNC_PROTOCOL:
            {
                if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_DMR) {           // Encapsulated DMR data frame
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection* connection = m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                if (m_dmrEnabled) {
                                    if (m_tagDMR != nullptr) {
                                        m_tagDMR->processFrame(buffer.get(), length, peerId, rtpHeader.getSequence(), streamId);
                                    }
                                }
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_P25) {      // Encapsulated P25 data frame
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection* connection = m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                if (m_p25Enabled) {
                                    if (m_tagP25 != nullptr) {
                                        m_tagP25->processFrame(buffer.get(), length, peerId, rtpHeader.getSequence(), streamId);
                                    }
                                }
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_PROTOCOL_SUBFUNC_NXDN) {     // Encapsulated NXDN data frame
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection* connection = m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                if (m_nxdnEnabled) {
                                    if (m_tagNXDN != nullptr) {
                                        m_tagNXDN->processFrame(buffer.get(), length, peerId, rtpHeader.getSequence(), streamId);
                                    }
                                }
                            }
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
                    FNEPeerConnection* connection = new FNEPeerConnection(peerId, address, addrLen);
                    connection->lastPing(now);
                    connection->currStreamId(streamId);

                    std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
                    connection->salt(dist(m_random));

                    LogInfoEx(LOG_NET, "Repeater logging in with PEER %u, %s:%u", peerId, connection->address().c_str(), connection->port());

                    connection->connectionState(NET_STAT_WAITING_AUTHORISATION);
                    m_peers[peerId] = connection;

                    // transmit salt to peer
                    uint8_t salt[4U];
                    ::memset(salt, 0x00U, 4U);
                    __SET_UINT32(connection->salt(), salt, 0U);

                    writePeerACK(peerId, salt, 4U);
                    LogInfoEx(LOG_NET, "Challenge response send to PEER %u for login", peerId);
                }
                else {
                    writePeerNAK(peerId, TAG_REPEATER_LOGIN, address, addrLen);

                    // check if the peer is in our peer list -- if he is, and he isn't in a running state, reset
                    // the login sequence
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection* connection = m_peers[peerId];
                        LogMessage(LOG_NET, "PEER %u was RPTL NAKed, cleaning up peer connection, connectionState = %u", peerId, connection->connectionState());
                        if (connection != nullptr) {
                            if (erasePeer(peerId)) {
                                delete connection;
                            }
                        } else {
                            erasePeer(peerId);
                            if (m_verbose) {
                                LogWarning(LOG_NET, "PEER %u was RPTL NAKed while having no connection?, connectionState = %u", peerId, connection->connectionState());
                            }
                        }
                    }
                }
            }
            break;
        case NET_FUNC_RPTK:                                                             // Repeater Authentication
            {
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection* connection = m_peers[peerId];
                    if (connection != nullptr) {
                        connection->lastPing(now);

                        if (connection->connectionState() == NET_STAT_WAITING_AUTHORISATION) {
                            // get the hash from the frame message
                            uint8_t hash[length - 8U];
                            ::memset(hash, 0x00U, length - 8U);
                            ::memcpy(hash, buffer.get() + 8U, length - 8U);

                            // generate our own hash
                            uint8_t salt[4U];
                            ::memset(salt, 0x00U, 4U);
                            __SET_UINT32(connection->salt(), salt, 0U);

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
                                connection->connectionState(NET_STAT_WAITING_CONFIG);
                                writePeerACK(peerId);
                                LogInfoEx(LOG_NET, "PEER %u has completed the login exchange", peerId);
                            }
                            else {
                                LogWarning(LOG_NET, "PEER %u has failed the login exchange", peerId);
                                writePeerNAK(peerId, TAG_REPEATER_AUTH);
                                erasePeer(peerId);
                            }

                            m_peers[peerId] = connection;
                        }
                        else {
                            LogWarning(LOG_NET, "PEER %u tried login exchange while in an incorrect state?", peerId);
                            writePeerNAK(peerId, TAG_REPEATER_AUTH);
                            erasePeer(peerId);
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
                    FNEPeerConnection* connection = m_peers[peerId];
                    if (connection != nullptr) {
                        connection->lastPing(now);

                        if (connection->connectionState() == NET_STAT_WAITING_CONFIG) {
                            uint8_t rawPayload[length - 8U];
                            ::memset(rawPayload, 0x00U, length - 8U);
                            ::memcpy(rawPayload, buffer.get() + 8U, length - 8U);
                            std::string payload(rawPayload, rawPayload + (length - 8U));

                            // parse JSON body
                            json::value v;
                            std::string err = json::parse(v, payload);
                            if (!err.empty()) {
                                LogWarning(LOG_NET, "PEER %u has supplied invalid configuration data", peerId);
                                writePeerNAK(peerId, TAG_REPEATER_AUTH);
                                erasePeer(peerId);
                            }
                            else  {
                                // ensure parsed JSON is an object
                                if (!v.is<json::object>()) {
                                    LogWarning(LOG_NET, "PEER %u has supplied invalid configuration data", peerId);
                                    writePeerNAK(peerId, TAG_REPEATER_AUTH);
                                    erasePeer(peerId);
                                }
                                else {
                                    connection->config(v.get<json::object>());
                                    connection->connectionState(NET_STAT_RUNNING);
                                    connection->connected(true);
                                    connection->pingsReceived(0U);
                                    connection->lastPing(now);
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

                                    json::object peerConfig = connection->config();
                                    if (peerConfig["software"].is<std::string>()) {
                                        std::string software = peerConfig["software"].get<std::string>();
                                        LogInfoEx(LOG_NET, "PEER %u reports software %s", peerId, software.c_str());
                                    }

                                    // setup the affiliations list for this peer
                                    char *peerName = new char[16];
                                    ::sprintf(peerName, "PEER %u", peerId);
                                    m_peerAffiliations[peerId] = new lookups::AffiliationLookup(peerName, m_verbose);
                                }
                            }
                        }
                        else {
                            LogWarning(LOG_NET, "PEER %u tried login exchange while in an incorrect state?", peerId);
                            writePeerNAK(peerId, TAG_REPEATER_CONFIG);
                            erasePeer(peerId);
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
                    FNEPeerConnection* connection = m_peers[peerId];
                    if (connection != nullptr) {
                        std::string ip = udp::Socket::address(address);

                        // validate peer (simple validation really)
                        if (connection->connected() && connection->address() == ip) {
                            LogInfoEx(LOG_NET, "PEER %u is closing down", peerId);
                            if (erasePeer(peerId)) {
                                erasePeerAffiliations(peerId);
                                delete connection;
                            }
                        }
                    }
                }
            }
            break;
        case NET_FUNC_PING:                                                             // Repeater Ping
            {
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection* connection = m_peers[peerId];
                    if (connection != nullptr) {
                        std::string ip = udp::Socket::address(address);

                        // validate peer (simple validation really)
                        if (connection->connected() && connection->address() == ip) {
                            uint32_t pingsRx = connection->pingsReceived();
                            pingsRx++;

                            connection->pingsReceived(pingsRx);
                            connection->lastPing(now);
                            connection->pktLastSeq(connection->pktLastSeq() + 1);

                            m_peers[peerId] = connection;
                            writePeerCommand(peerId, { NET_FUNC_PONG, NET_SUBFUNC_NOP });

                            if (m_reportPeerPing) {
                                LogInfoEx(LOG_NET, "PEER %u ping received and answered, pingsReceived = %u", peerId, connection->pingsReceived());
                            }
                        }
                        else {
                            writePeerNAK(peerId, TAG_REPEATER_PING);
                        }
                    }
                }
            }
            break;

        case NET_FUNC_GRANT_REQ:                                                        // Repeater Grant Request
            {
                if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                    FNEPeerConnection* connection = m_peers[peerId];
                    if (connection != nullptr) {
                        std::string ip = udp::Socket::address(address);

                        // validate peer (simple validation really)
                        if (connection->connected() && connection->address() == ip) {
                            /* ignored */
                        }
                        else {
                            writePeerNAK(peerId, TAG_REPEATER_GRANT);
                        }
                    }
                }
            }
            break;

        case NET_FUNC_TRANSFER:
            {
                if (fneHeader.getSubFunction() == NET_TRANSFER_SUBFUNC_ACTIVITY) {      // Peer Activity Log Transfer
                    if (m_allowActivityTransfer) {
                        if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                            FNEPeerConnection* connection = m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(address);

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip) {
                                    uint8_t rawPayload[length - 11U];
                                    ::memset(rawPayload, 0x00U, length - 11U);
                                    ::memcpy(rawPayload, buffer.get() + 11U, length - 11U);
                                    std::string payload(rawPayload, rawPayload + (length - 11U));

                                    ::ActivityLog("%u %s", peerId, payload.c_str());
                                }
                                else {
                                    writePeerNAK(peerId, TAG_TRANSFER_ACT_LOG);
                                }
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_TRANSFER_SUBFUNC_DIAG) {     // Peer Diagnostic Log Transfer
                    if (m_allowDiagnosticTransfer) {
                        if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                            FNEPeerConnection* connection = m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(address);

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip) {
                                    uint8_t rawPayload[length - 11U];
                                    ::memset(rawPayload, 0x00U, length - 11U);
                                    ::memcpy(rawPayload, buffer.get() + 11U, length - 11U);
                                    std::string payload(rawPayload, rawPayload + (length - 11U));

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
                }
                else {
                    Utils::dump("Unknown transfer opcode from the peer", buffer.get(), length);
                }
            }
            break;

        case NET_FUNC_ANNOUNCE:
            {
                if (fneHeader.getSubFunction() == NET_ANNC_SUBFUNC_GRP_AFFIL) {         // Announce Group Affiliation
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection* connection = m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(address);
                            lookups::AffiliationLookup* aff = m_peerAffiliations[peerId];

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                uint32_t srcId = __GET_UINT16(buffer.get(), 0U);
                                uint32_t dstId = __GET_UINT16(buffer.get(), 3U);
                                aff->groupUnaff(srcId);
                                aff->groupAff(srcId, dstId);
                            }
                            else {
                                writePeerNAK(peerId, TAG_ANNOUNCE);
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_ANNC_SUBFUNC_UNIT_REG) {     // Announce Unit Registration
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection* connection = m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(address);
                            lookups::AffiliationLookup* aff = m_peerAffiliations[peerId];

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                uint32_t srcId = __GET_UINT16(buffer.get(), 0U);
                                aff->unitReg(srcId);
                            }
                            else {
                                writePeerNAK(peerId, TAG_ANNOUNCE);
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_ANNC_SUBFUNC_UNIT_DEREG) {   // Announce Unit Deregistration
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection* connection = m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(address);
                            lookups::AffiliationLookup* aff = m_peerAffiliations[peerId];

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                uint32_t srcId = __GET_UINT16(buffer.get(), 0U);
                                aff->unitDereg(srcId);
                            }
                            else {
                                writePeerNAK(peerId, TAG_ANNOUNCE);
                            }
                        }
                    }
                }
                else if (fneHeader.getSubFunction() == NET_ANNC_SUBFUNC_AFFILS) {       // Announce Update All Affiliations
                    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
                        FNEPeerConnection* connection = m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                lookups::AffiliationLookup* aff = m_peerAffiliations[peerId];
                                aff->clearGroupAff(0U, true);

                                // update TGID lists
                                uint32_t len = __GET_UINT32(buffer.get(), 0U);
                                uint32_t offs = 4U;
                                for (uint32_t i = 0; i < len; i++) {
                                    uint32_t srcId = __GET_UINT16(buffer.get(), offs);
                                    uint32_t dstId = __GET_UINT16(buffer.get(), offs + 4U);

                                    aff->groupAff(srcId, dstId);
                                    offs += 8U;
                                }
                                LogMessage(LOG_NET, "PEER %u announced %u affiliations", peerId, len);
                            }
                            else {
                                writePeerNAK(peerId, TAG_ANNOUNCE);
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
    else {
        // if the DMR handler has parrot frames to playback, playback a frame
        if (m_tagDMR->hasParrotFrames()) {
            m_tagDMR->playbackParrot();
        }

        // if the P25 handler has parrot frames to playback, playback a frame
        if (m_tagP25->hasParrotFrames()) {
            m_tagP25->playbackParrot();
        }

        // if the NXDN handler has parrot frames to playback, playback a frame
        if (m_tagNXDN->hasParrotFrames()) {
            m_tagNXDN->playbackParrot();
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

    m_socket = new udp::Socket(m_address, m_port);

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
        uint8_t buffer[1U];
        ::memset(buffer, 0x00U, 1U);

        for (auto peer : m_peers) {
            writePeer(peer.first, { NET_FUNC_MST_CLOSING, NET_SUBFUNC_NOP }, buffer, 1U, (ushort)0U, 0U);
        }
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
/// Helper to erase the peer from the peers affiliations list.
/// </summary>
/// <param name="peerId"></param>
/// <returns></returns>
bool FNENetwork::erasePeerAffiliations(uint32_t peerId)
{
    auto it = std::find_if(m_peerAffiliations.begin(), m_peerAffiliations.end(), [&](PeerAffiliationMapPair x) { return x.first == peerId; });
    if (it != m_peerAffiliations.end()) {
        lookups::AffiliationLookup* aff = m_peerAffiliations[peerId];
        m_peerAffiliations.erase(peerId);
        delete aff;

        return true;
    }

    return false;
}

/// <summary>
/// Helper to erase the peer from the peers list.
/// </summary>
/// <param name="peerId"></param>
/// <returns></returns>
bool FNENetwork::erasePeer(uint32_t peerId)
{
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        m_peers.erase(peerId);
        return true;
    }

    return false;
}

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

    writePeerCommand(peerId, { NET_FUNC_MASTER, NET_MASTER_SUBFUNC_WL_RID },
        payload, 4U + (ridWhitelist.size() * 4U), queueOnly, true);
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

    writePeerCommand(peerId, { NET_FUNC_MASTER, NET_MASTER_SUBFUNC_BL_RID },
        payload, 4U + (ridBlacklist.size() * 4U), queueOnly, true);
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
        std::vector<uint32_t> inclusion = entry.config().inclusion();
        std::vector<uint32_t> exclusion = entry.config().exclusion();

        // peer inclusion lists take priority over exclusion lists
        if (inclusion.size() > 0) {
            auto it = std::find(inclusion.begin(), inclusion.end(), peerId);
            if (it == inclusion.end()) {
                // LogDebug(LOG_NET, "PEER %u TGID %u TS %u -- not included peer", peerId, entry.source().tgId(), entry.source().tgSlot());
                continue;
            }
        }
        else {
            if (exclusion.size() > 0) {
                auto it = std::find(exclusion.begin(), exclusion.end(), peerId);
                if (it != exclusion.end()) {
                    // LogDebug(LOG_NET, "PEER %u TGID %u TS %u -- excluded peer", peerId, entry.source().tgId(), entry.source().tgSlot());
                    continue;
                }
            }
        }
        
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

    writePeerCommand(peerId, { NET_FUNC_MASTER, NET_MASTER_SUBFUNC_ACTIVE_TGS },
        payload, 4U + (tgidList.size() * 5U), queueOnly, true);
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
        std::vector<uint32_t> inclusion = entry.config().inclusion();
        std::vector<uint32_t> exclusion = entry.config().exclusion();

        // peer inclusion lists take priority over exclusion lists
        if (inclusion.size() > 0) {
            auto it = std::find(inclusion.begin(), inclusion.end(), peerId);
            if (it == inclusion.end()) {
                // LogDebug(LOG_NET, "PEER %u TGID %u TS %u -- not included peer", peerId, entry.source().tgId(), entry.source().tgSlot());
                continue;
            }
        }
        else {
            if (exclusion.size() > 0) {
                auto it = std::find(exclusion.begin(), exclusion.end(), peerId);
                if (it != exclusion.end()) {
                    // LogDebug(LOG_NET, "PEER %u TGID %u TS %u -- excluded peer", peerId, entry.source().tgId(), entry.source().tgSlot());
                    continue;
                }
            }
        }

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

    writePeerCommand(peerId, { NET_FUNC_MASTER, NET_MASTER_SUBFUNC_DEACTIVE_TGS }, 
        payload, 4U + (tgidList.size() * 5U), queueOnly, true);
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
/// Helper to send a data message to the specified peer.
/// </summary>
/// <param name="peerId">Peer ID.</param>
/// <param name="opcode">Opcode.</param>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
/// <param name="pktSeq"></param>
/// <param name="streamId"></param>
/// <param name="queueOnly"></param>
bool FNENetwork::writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint16_t pktSeq, uint32_t streamId, bool queueOnly)
{
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            uint32_t peerStreamId = connection->currStreamId();
            if (streamId == 0U) {
                streamId = peerStreamId;
            }
            sockaddr_storage addr = connection->socketStorage();
            uint32_t addrLen = connection->sockStorageLen();

            m_frameQueue->enqueueMessage(data, length, streamId, peerId, m_peerId, opcode, pktSeq, addr, addrLen);
            if (queueOnly)
                return true;
            return m_frameQueue->flushQueue();
        }
    }

    return false;
}

/// <summary>
/// Helper to send a data message to the specified peer.
/// </summary>
/// <param name="peerId">Peer ID.</param>
/// <param name="opcode">Opcode.</param>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
/// <param name="streamId"></param>
/// <param name="queueOnly"></param>
/// <param name="incPktSeq"></param>
bool FNENetwork::writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data, uint32_t length, uint32_t streamId, bool queueOnly, bool incPktSeq)
{
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            if (incPktSeq) {
                connection->pktLastSeq(connection->pktLastSeq() + 1);
            }
            uint16_t pktSeq = connection->pktLastSeq();

            return writePeer(peerId, opcode, data, length, pktSeq, streamId, queueOnly);
        }
    }

    return false;
}

/// <summary>
/// Helper to send a command message to the specified peer.
/// </summary>
/// <param name="peerId">Peer ID.</param>
/// <param name="opcode">Opcode.</param>
/// <param name="data">Buffer to write to the network.</param>
/// <param name="length">Length of buffer to write.</param>
/// <param name="queueOnly"></param>
/// <param name="incPktSeq"></param>
bool FNENetwork::writePeerCommand(uint32_t peerId, FrameQueue::OpcodePair opcode,
    const uint8_t* data, uint32_t length, bool queueOnly, bool incPktSeq)
{
    assert(peerId > 0);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + 6U, data, length);
    }

    uint32_t len = length + 6U;
    return writePeer(peerId, opcode, buffer, len, 0U, queueOnly, incPktSeq);
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

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + 6U, data, length);
    }

    return writePeer(peerId, { NET_FUNC_ACK, NET_SUBFUNC_NOP }, buffer, length + 10U, RTP_END_OF_CALL_SEQ, false, true);
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

    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID

    LogWarning(LOG_NET, "%s from unauth PEER %u", tag, peerId);
    return writePeer(peerId, { NET_FUNC_NAK, NET_SUBFUNC_NOP }, buffer, 10U, RTP_END_OF_CALL_SEQ, false, true);
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

    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID

    LogWarning(LOG_NET, "%s from unauth PEER %u", tag, peerId);
    m_frameQueue->enqueueMessage(buffer, 10U, createStreamId(), peerId, m_peerId,
        { NET_FUNC_NAK, NET_SUBFUNC_NOP }, 0U, addr, addrLen);
    return m_frameQueue->flushQueue();
}
