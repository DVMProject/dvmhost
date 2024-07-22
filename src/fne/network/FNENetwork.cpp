// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/edac/SHA256.h"
#include "common/network/json/json.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "network/FNENetwork.h"
#include "network/callhandler/TagDMRData.h"
#include "network/callhandler/TagP25Data.h"
#include "network/callhandler/TagNXDNData.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"

using namespace network;
using namespace network::callhandler;

#include <cassert>
#include <cerrno>
#include <chrono>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t MAX_HARD_CONN_CAP = 250U;
const uint8_t MAX_PEER_LIST_BEFORE_FLUSH = 10U;
const uint32_t MAX_RID_LIST_CHUNK = 50U;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex FNENetwork::m_peerMutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the FNENetwork class. */

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
    m_parrotDelayTimer(1000U, 0U, parrotDelay),
    m_parrotGrantDemand(parrotGrantDemand),
    m_parrotOnlyOriginating(false),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_peerListLookup(nullptr),
    m_status(NET_STAT_INVALID),
    m_peers(),
    m_peerAffiliations(),
    m_ccPeerMap(),
    m_maintainenceTimer(1000U, pingTime),
    m_updateLookupTime(updateLookupTime * 60U),
    m_softConnLimit(0U),
    m_callInProgress(false),
    m_disallowAdjStsBcast(false),
    m_disallowExtAdjStsBcast(true),
    m_allowConvSiteAffOverride(false),
    m_restrictGrantToAffOnly(false),
    m_filterHeaders(true),
    m_filterTerminators(true),
    m_dropU2UPeerTable(),
    m_enableInfluxDB(false),
    m_influxServerAddress("127.0.0.1"),
    m_influxServerPort(8086U),
    m_influxServerToken(),
    m_influxOrg("dvm"),
    m_influxBucket("dvm"),
    m_influxLogRawData(false),
    m_disablePacketData(false),
    m_dumpDataPacket(false),
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

/* Finalizes a instance of the FNENetwork class. */

FNENetwork::~FNENetwork()
{
    delete m_tagDMR;
    delete m_tagP25;
    delete m_tagNXDN;
}

/* Helper to set configuration options. */

void FNENetwork::setOptions(yaml::Node& conf, bool printOptions)
{
    m_disallowAdjStsBcast = conf["disallowAdjStsBcast"].as<bool>(false);
    m_disallowExtAdjStsBcast = conf["disallowExtAdjStsBcast"].as<bool>(true);
    m_allowConvSiteAffOverride = conf["allowConvSiteAffOverride"].as<bool>(true);
    m_softConnLimit = conf["connectionLimit"].as<uint32_t>(MAX_HARD_CONN_CAP);

    if (m_softConnLimit > MAX_HARD_CONN_CAP) {
        m_softConnLimit = MAX_HARD_CONN_CAP;
    }

    // always force disable ADJ_STS_BCAST to external peers if the all option
    // is enabled
    if (m_disallowAdjStsBcast) {
        m_disallowExtAdjStsBcast = true;
    }

    m_enableInfluxDB = conf["enableInflux"].as<bool>(false);
    m_influxServerAddress = conf["influxServerAddress"].as<std::string>("127.0.0.1");
    m_influxServerPort = conf["influxServerPort"].as<uint16_t>(8086U);
    m_influxServerToken = conf["influxServerToken"].as<std::string>();
    m_influxOrg = conf["influxOrg"].as<std::string>("dvm");
    m_influxBucket = conf["influxBucket"].as<std::string>("dvm");
    m_influxLogRawData = conf["influxLogRawData"].as<bool>(false);
    if (m_enableInfluxDB) {
        m_influxServer = influxdb::ServerInfo(m_influxServerAddress, m_influxServerPort, m_influxOrg, m_influxServerToken, m_influxBucket);
    }

    m_parrotOnlyOriginating = conf["parrotOnlyToOrginiatingPeer"].as<bool>(false);
    m_restrictGrantToAffOnly = conf["restrictGrantToAffiliatedOnly"].as<bool>(false);
    m_filterHeaders = conf["filterHeaders"].as<bool>(true);
    m_filterTerminators = conf["filterTerminators"].as<bool>(true);

    m_disablePacketData = conf["disablePacketData"].as<bool>(false);
    m_dumpDataPacket = conf["dumpDataPacket"].as<bool>(false);

    /*
    ** Drop Unit to Unit Peers
    */

    yaml::Node& dropUnitToUnit = conf["dropUnitToUnit"];
    if (dropUnitToUnit.size() > 0U) {
        for (size_t i = 0; i < dropUnitToUnit.size(); i++) {
            uint32_t peerId = (uint32_t)::strtoul(dropUnitToUnit[i].as<std::string>("0").c_str(), NULL, 10);
            if (peerId != 0U) {
                m_dropU2UPeerTable.push_back(peerId);
            }
        }
    }

    if (printOptions) {
        LogInfo("    Maximum Permitted Connections: %u", m_softConnLimit);
        LogInfo("    Disable adjacent site broadcasts to any peers: %s", m_disallowAdjStsBcast ? "yes" : "no");
        if (m_disallowAdjStsBcast) {
            LogWarning(LOG_NET, "NOTICE: All P25 ADJ_STS_BCAST messages will be blocked and dropped!");
        }
        LogInfo("    Disable Packet Data: %s", m_disablePacketData ? "yes" : "no");
        LogInfo("    Dump Packet Data: %s", m_dumpDataPacket ? "yes" : "no");
        LogInfo("    Disable P25 ADJ_STS_BCAST to external peers: %s", m_disallowExtAdjStsBcast ? "yes" : "no");
        LogInfo("    Allow conventional sites to override affiliation and receive all traffic: %s", m_allowConvSiteAffOverride ? "yes" : "no");
        LogInfo("    Restrict grant response by affiliation: %s", m_restrictGrantToAffOnly ? "yes" : "no");
        LogInfo("    Traffic Headers Filtered by Destination ID: %s", m_filterHeaders ? "yes" : "no");
        LogInfo("    Traffic Terminators Filtered by Destination ID: %s", m_filterTerminators ? "yes" : "no");
        LogInfo("    InfluxDB Reporting Enabled: %s", m_enableInfluxDB ? "yes" : "no");
        if (m_enableInfluxDB) {
            LogInfo("    InfluxDB Address: %s", m_influxServerAddress.c_str());
            LogInfo("    InfluxDB Port: %u", m_influxServerPort);
            LogInfo("    InfluxDB Organization: %s", m_influxOrg.c_str());
            LogInfo("    InfluxDB Bucket: %s", m_influxBucket.c_str());
            LogInfo("    InfluxDB Log Raw TSBK/CSBK/RCCH: %s", m_influxLogRawData ? "yes" : "no");
        }
        LogInfo("    Parrot Repeat to Only Originating Peer: %s", m_parrotOnlyOriginating ? "yes" : "no");
    }
}

/* Sets the instances of the Radio ID, Talkgroup Rules, and Peer List lookup tables. */

void FNENetwork::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup, lookups::PeerListLookup* peerListLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
    m_peerListLookup = peerListLookup;
}

/* Sets endpoint preshared encryption key. */

void FNENetwork::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/* Process a data frames from the network. */

void FNENetwork::processNetwork()
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
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

        NetPacketRequest* req = new NetPacketRequest();
        req->peerId = peerId;

        req->address = address;
        req->addrLen = addrLen;
        req->rtpHeader = rtpHeader;
        req->fneHeader = fneHeader;

        req->length = length;
        req->buffer = new uint8_t[length];
        ::memcpy(req->buffer, buffer.get(), length);

        if (!Thread::runAsThread(this, threadedNetworkRx, req)) {
            delete[] req->buffer;
            delete req;
            return;
        }
    }
}

/* Updates the timer by the passed number of milliseconds. */

void FNENetwork::clock(uint32_t ms)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (m_forceListUpdate) {
        for (auto peer : m_peers) {
            peerACLUpdate(peer.first);
        }
        m_forceListUpdate = false;
    }

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
                        LogInfoEx(LOG_NET, "PEER %u (%s) timed out, dt = %u, now = %u", id, connection->identity().c_str(),
                            dt, now);
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

    m_parrotDelayTimer.clock(ms);
    if (m_parrotDelayTimer.isRunning() && m_parrotDelayTimer.hasExpired()) {
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

    if (!m_tagDMR->hasParrotFrames() && !m_tagP25->hasParrotFrames() && !m_tagNXDN->hasParrotFrames() &&
        m_parrotDelayTimer.isRunning() && m_parrotDelayTimer.hasExpired()) {
        m_parrotDelayTimer.stop();
    }
}

/* Opens connection to the network. */

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

/* Closes connection to the network. */

void FNENetwork::close()
{
    if (m_debug)
        LogMessage(LOG_NET, "Closing Network");

    if (m_status == NET_STAT_MST_RUNNING) {
        uint8_t buffer[1U];
        ::memset(buffer, 0x00U, 1U);

        for (auto peer : m_peers) {
            writePeer(peer.first, { NET_FUNC::MST_CLOSING, NET_SUBFUNC::NOP }, buffer, 1U, (ushort)0U, 0U);
        }
    }

    m_socket->close();

    m_maintainenceTimer.stop();

    m_status = NET_STAT_INVALID;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Process a data frames from the network. */

void* FNENetwork::threadedNetworkRx(void* arg)
{
    NetPacketRequest* req = (NetPacketRequest*)arg;
    if (req != nullptr) {
        ::pthread_detach(req->thread);

        uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        FNENetwork* network = static_cast<FNENetwork*>(req->obj);
        if (network == nullptr) {
            delete req;
            return nullptr;
        }

        if (req->length > 0) {
            uint32_t peerId = req->fneHeader.getPeerId();
            uint32_t streamId = req->fneHeader.getStreamId();

            std::stringstream peerName;
            peerName << peerId << ":rx-pckt";
#ifdef _GNU_SOURCE
            ::pthread_setname_np(req->thread, peerName.str().c_str());
#endif // _GNU_SOURCE

            // update current peer packet sequence and stream ID
            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end()) && streamId != 0U) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                uint16_t pktSeq = req->rtpHeader.getSequence();

                if (connection != nullptr) {
                    if (pktSeq == RTP_END_OF_CALL_SEQ) {
                        if (req->fneHeader.getFunction() != NET_FUNC::PING) {
                            connection->pktLastSeq(pktSeq);
                            connection->pktNextSeq(0U);
                        }
                    } else {
                        if ((connection->currStreamId() == streamId) && (pktSeq != connection->pktNextSeq()) && (pktSeq != (RTP_END_OF_CALL_SEQ - 1U))) {
                            LogWarning(LOG_NET, "PEER %u (%s) stream %u out-of-sequence; %u != %u", peerId, connection->identity().c_str(),
                                streamId, pktSeq, connection->pktNextSeq());
                        }

                        connection->currStreamId(streamId);
                        connection->pktLastSeq(pktSeq);
                        connection->pktNextSeq(pktSeq + 1);
                        if (connection->pktNextSeq() > (RTP_END_OF_CALL_SEQ - 1U)) {
                            connection->pktNextSeq(0U);
                        }
                    }
                }

                network->m_peers[peerId] = connection;
            }

            // if we don't have a stream ID and are receiving call data -- throw an error and discard
            if (streamId == 0 && req->fneHeader.getFunction() == NET_FUNC::PROTOCOL) {
                std::string peerIdentity = network->resolvePeerIdentity(peerId);
                LogError(LOG_NET, "PEER %u (%s) malformed packet (no stream ID for a call?)", peerId, peerIdentity.c_str());

                if (req->buffer != nullptr)
                    delete req->buffer;
                delete req;

                return nullptr;
            }

            // process incoming message frame opcodes
            switch (req->fneHeader.getFunction()) {
            case NET_FUNC::PROTOCOL:
                {
                    if (req->fneHeader.getSubFunction() == NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR) {         // Encapsulated DMR data frame
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(req->address);
                                connection->lastPing(now);

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip) {
                                    if (network->m_dmrEnabled) {
                                        if (network->m_tagDMR != nullptr) {
                                            network->m_tagDMR->processFrame(req->buffer, req->length, peerId, req->rtpHeader.getSequence(), streamId);
                                        }
                                    } else {
                                        network->writePeerNAK(peerId, TAG_DMR_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                    }
                                }
                            }
                        }
                        else {
                            network->writePeerNAK(peerId, TAG_DMR_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED);
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::PROTOCOL_SUBFUNC_P25) {    // Encapsulated P25 data frame
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(req->address);
                                connection->lastPing(now);

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip) {
                                    if (network->m_p25Enabled) {
                                        if (network->m_tagP25 != nullptr) {
                                            network->m_tagP25->processFrame(req->buffer, req->length, peerId, req->rtpHeader.getSequence(), streamId);
                                        }
                                    } else {
                                        network->writePeerNAK(peerId, TAG_P25_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                    }
                                }
                            }
                        }
                        else {
                            network->writePeerNAK(peerId, TAG_P25_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED);
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN) {   // Encapsulated NXDN data frame
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(req->address);
                                connection->lastPing(now);

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip) {
                                    if (network->m_nxdnEnabled) {
                                        if (network->m_tagNXDN != nullptr) {
                                            network->m_tagNXDN->processFrame(req->buffer, req->length, peerId, req->rtpHeader.getSequence(), streamId);
                                        }
                                    } else {
                                        network->writePeerNAK(peerId, TAG_NXDN_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                    }
                                }
                            }
                        }
                        else {
                            network->writePeerNAK(peerId, TAG_NXDN_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED);
                        }
                    }
                    else {
                        Utils::dump("Unknown protocol opcode from peer", req->buffer, req->length);
                    }
                }
                break;

            case NET_FUNC::RPTL:                                                                        // Repeater Login
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) == network->m_peers.end())) {
                        if (network->m_peers.size() >= MAX_HARD_CONN_CAP) {
                            LogError(LOG_NET, "PEER %u attempted to connect with no more connections available, currConnections = %u", peerId, network->m_peers.size());
                            network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_MAX_CONN, req->address, req->addrLen);
                            break;
                        }

                        if (network->m_softConnLimit > 0U && network->m_peers.size() >= network->m_softConnLimit) {
                            LogError(LOG_NET, "PEER %u attempted to connect with no more connections available, maxConnections = %u, currConnections = %u", peerId, network->m_softConnLimit, network->m_peers.size());
                            network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_MAX_CONN, req->address, req->addrLen);
                            break;
                        }

                        FNEPeerConnection* connection = new FNEPeerConnection(peerId, req->address, req->addrLen);
                        connection->lastPing(now);
                        connection->currStreamId(streamId);

                        network->setupRepeaterLogin(peerId, connection);

                        // check if the peer is in the peer ACL list
                        if (network->m_peerListLookup->getACL()) {
                            if (!network->m_peerListLookup->isPeerAllowed(peerId)) {
                                if (network->m_peerListLookup->getMode() == lookups::PeerListLookup::BLACKLIST) {
                                    LogWarning(LOG_NET, "PEER %u RPTL, blacklisted from access", peerId);
                                } else {
                                    LogWarning(LOG_NET, "PEER %u RPTL, failed whitelist check", peerId);
                                }

                                network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);

                                delete connection;
                                network->erasePeer(peerId);
                            }
                        }
                    }
                    else {
                        // check if the peer is in our peer list -- if he is, and he isn't in a running state, reset
                        // the login sequence
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                if (connection->connectionState() == NET_STAT_RUNNING) {
                                    LogMessage(LOG_NET, "PEER %u (%s) resetting peer connection, connectionState = %u", peerId, connection->identity().c_str(),
                                        connection->connectionState());
                                    delete connection;

                                    connection = new FNEPeerConnection(peerId, req->address, req->addrLen);
                                    connection->lastPing(now);
                                    connection->currStreamId(streamId);

                                    network->erasePeerAffiliations(peerId);
                                    network->setupRepeaterLogin(peerId, connection);

                                    // check if the peer is in the peer ACL list
                                    if (network->m_peerListLookup->getACL()) {
                                        if (!network->m_peerListLookup->isPeerAllowed(peerId)) {
                                            if (network->m_peerListLookup->getMode() == lookups::PeerListLookup::BLACKLIST) {
                                                LogWarning(LOG_NET, "PEER %u RPTL, blacklisted from access", peerId);
                                            } else {
                                                LogWarning(LOG_NET, "PEER %u RPTL, failed whitelist check", peerId);
                                            }

                                            network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);

                                            delete connection;
                                            network->erasePeer(peerId);
                                        }
                                    }
                                } else {
                                    network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);

                                    LogWarning(LOG_NET, "PEER %u (%s) RPTL NAK, bad connection state, connectionState = %u", peerId, connection->identity().c_str(),
                                        connection->connectionState());

                                    delete connection;
                                    network->erasePeer(peerId);
                                }
                            } else {
                                network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);

                                network->erasePeer(peerId);
                                LogWarning(LOG_NET, "PEER %u RPTL NAK, having no connection", peerId);
                            }
                        }
                    }
                }
                break;
            case NET_FUNC::RPTK:                                                                        // Repeater Authentication
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            connection->lastPing(now);

                            if (connection->connectionState() == NET_STAT_WAITING_AUTHORISATION) {
                                // get the hash from the frame message
                                uint8_t hash[req->length - 8U];
                                ::memset(hash, 0x00U, req->length - 8U);
                                ::memcpy(hash, req->buffer + 8U, req->length - 8U);

                                // generate our own hash
                                uint8_t salt[4U];
                                ::memset(salt, 0x00U, 4U);
                                __SET_UINT32(connection->salt(), salt, 0U);

                                std::string passwordForPeer = network->m_password;

                                // check if the peer is in the peer ACL list
                                bool validAcl = true;
                                if (network->m_peerListLookup->getACL()) {
                                    if (!network->m_peerListLookup->isPeerAllowed(peerId)) {
                                        if (network->m_peerListLookup->getMode() == lookups::PeerListLookup::BLACKLIST) {
                                            LogWarning(LOG_NET, "PEER %u RPTK, blacklisted from access", peerId);
                                        } else {
                                            LogWarning(LOG_NET, "PEER %u RPTK, failed whitelist check", peerId);
                                        }

                                        validAcl = false;
                                    } else {
                                        lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                                        if (peerEntry.peerDefault()) {
                                            validAcl = false; // default peer IDs are a no-no as they have no data thus fail ACL check
                                        } else {
                                            passwordForPeer = peerEntry.peerPassword();
                                            if (passwordForPeer.length() == 0) {
                                                passwordForPeer = network->m_password;
                                            }
                                        }
                                    }
                                }

                                if (validAcl) {
                                    size_t size = passwordForPeer.size();
                                    uint8_t* in = new uint8_t[size + sizeof(uint32_t)];
                                    ::memcpy(in, salt, sizeof(uint32_t));
                                    for (size_t i = 0U; i < size; i++)
                                        in[i + sizeof(uint32_t)] = passwordForPeer.at(i);

                                    uint8_t out[32U];
                                    edac::SHA256 sha256;
                                    sha256.buffer(in, (uint32_t)(size + sizeof(uint32_t)), out);

                                    delete[] in;

                                    // validate hash
                                    bool validHash = false;
                                    if (req->length - 8U == 32U) {
                                        validHash = true;
                                        for (uint8_t i = 0; i < 32U; i++) {
                                            if (hash[i] != out[i]) {
                                                validHash = false;
                                                break;
                                            }
                                        }
                                    }

                                    if (validHash) {
                                        connection->connectionState(NET_STAT_WAITING_CONFIG);
                                        network->writePeerACK(peerId);
                                        LogInfoEx(LOG_NET, "PEER %u RPTK ACK, completed the login exchange", peerId);
                                        network->m_peers[peerId] = connection;
                                    }
                                    else {
                                        LogWarning(LOG_NET, "PEER %u RPTK NAK, failed the login exchange", peerId);
                                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                                        network->erasePeer(peerId);
                                    }
                                } else {
                                    network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);
                                    network->erasePeer(peerId);
                                }
                            }
                            else {
                                LogWarning(LOG_NET, "PEER %u RPTK NAK, login exchange while in an incorrect state, connectionState = %u", peerId, connection->connectionState());
                                network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                                network->erasePeer(peerId);
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                        LogWarning(LOG_NET, "PEER %u RPTK NAK, having no connection", peerId);
                    }
                }
                break;
            case NET_FUNC::RPTC:                                                                        // Repeater Configuration
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            connection->lastPing(now);

                            if (connection->connectionState() == NET_STAT_WAITING_CONFIG) {
                                uint8_t rawPayload[req->length - 8U];
                                ::memset(rawPayload, 0x00U, req->length - 8U);
                                ::memcpy(rawPayload, req->buffer + 8U, req->length - 8U);
                                std::string payload(rawPayload, rawPayload + (req->length - 8U));

                                // parse JSON body
                                json::value v;
                                std::string err = json::parse(v, payload);
                                if (!err.empty()) {
                                    LogWarning(LOG_NET, "PEER %u RPTC NAK, supplied invalid configuration data", peerId);
                                    network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_INVALID_CONFIG_DATA, req->address, req->addrLen);
                                    network->erasePeer(peerId);
                                }
                                else  {
                                    // ensure parsed JSON is an object
                                    if (!v.is<json::object>()) {
                                        LogWarning(LOG_NET, "PEER %u RPTC NAK, supplied invalid configuration data", peerId);
                                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_INVALID_CONFIG_DATA, req->address, req->addrLen);
                                        network->erasePeer(peerId);
                                    }
                                    else {
                                        connection->config(v.get<json::object>());
                                        connection->connectionState(NET_STAT_RUNNING);
                                        connection->connected(true);
                                        connection->pingsReceived(0U);
                                        connection->lastPing(now);
                                        connection->lastACLUpdate(now);
                                        network->m_peers[peerId] = connection;

                                        // attach extra notification data to the RPTC ACK to notify the peer of 
                                        // the use of the alternate diagnostic port
                                        uint8_t buffer[1U];
                                        buffer[0U] = 0x00U;
                                        if (network->m_host->m_useAlternatePortForDiagnostics) {
                                            buffer[0U] = 0x80U;
                                        }

                                        network->writePeerACK(peerId, buffer, 1U);
                                        LogInfoEx(LOG_NET, "PEER %u RPTC ACK, completed the configuration exchange", peerId);

                                        json::object peerConfig = connection->config();
                                        if (peerConfig["identity"].is<std::string>()) {
                                            std::string identity = peerConfig["identity"].get<std::string>();
                                            connection->identity(identity);
                                            LogInfoEx(LOG_NET, "PEER %u reports identity [%8s]", peerId, identity.c_str());
                                        }

                                        if (peerConfig["externalPeer"].is<bool>()) {
                                            bool external = peerConfig["externalPeer"].get<bool>();
                                            connection->isExternalPeer(external);
                                            if (external)
                                                LogInfoEx(LOG_NET, "PEER %u reports external peer", peerId);
                                        }

                                        if (peerConfig["conventionalPeer"].is<bool>()) {
                                            if (network->m_allowConvSiteAffOverride) {
                                                bool convPeer = peerConfig["conventionalPeer"].get<bool>();
                                                connection->isConventionalPeer(convPeer);
                                                if (convPeer)
                                                    LogInfoEx(LOG_NET, "PEER %u reports conventional peer", peerId);
                                            }
                                        }

                                        if (peerConfig["software"].is<std::string>()) {
                                            std::string software = peerConfig["software"].get<std::string>();
                                            LogInfoEx(LOG_NET, "PEER %u reports software %s", peerId, software.c_str());
                                        }

                                        // setup the affiliations list for this peer
                                        std::stringstream peerName;
                                        peerName << "PEER " << peerId;
                                        network->createPeerAffiliations(peerId, peerName.str());

                                        // spin up a thread and send ACL list over to peer
                                        network->peerACLUpdate(peerId);
                                    }
                                }
                            }
                            else {
                                LogWarning(LOG_NET, "PEER %u (%s) RPTC NAK, login exchange while in an incorrect state, connectionState = %u", peerId, connection->identity().c_str(),
                                    connection->connectionState());
                                network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                                network->erasePeer(peerId);
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                        LogWarning(LOG_NET, "PEER %u RPTC NAK, having no connection", peerId);
                    }
                }
                break;

            case NET_FUNC::RPT_CLOSING:                                                                 // Repeater Closing (Disconnect)
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                LogInfoEx(LOG_NET, "PEER %u (%s) is closing down", peerId, connection->identity().c_str());
                                if (network->erasePeer(peerId)) {
                                    network->erasePeerAffiliations(peerId);
                                    delete connection;
                                }
                            }
                        }
                    }
                }
                break;
            case NET_FUNC::PING:                                                                        // Repeater Ping
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                uint32_t pingsRx = connection->pingsReceived();
                                uint64_t lastPing = connection->lastPing();
                                pingsRx++;

                                connection->pingsReceived(pingsRx);
                                connection->lastPing(now);

                                // does this peer need an ACL update?
                                uint64_t dt = connection->lastACLUpdate() + network->m_updateLookupTime;
                                if (dt < now) {
                                    LogInfoEx(LOG_NET, "PEER %u (%s) updating ACL list, dt = %u, now = %u", peerId, connection->identity().c_str(),
                                        dt, now);
                                    network->peerACLUpdate(peerId);
                                    connection->lastACLUpdate(now);
                                }

                                uint8_t payload[8U];
                                ::memset(payload, 0x00U, 8U);

                                // split ulong64_t (8 byte) value into bytes
                                payload[0U] = (uint8_t)((now >> 56) & 0xFFU);
                                payload[1U] = (uint8_t)((now >> 48) & 0xFFU);
                                payload[2U] = (uint8_t)((now >> 40) & 0xFFU);
                                payload[3U] = (uint8_t)((now >> 32) & 0xFFU);
                                payload[4U] = (uint8_t)((now >> 24) & 0xFFU);
                                payload[5U] = (uint8_t)((now >> 16) & 0xFFU);
                                payload[6U] = (uint8_t)((now >> 8) & 0xFFU);
                                payload[7U] = (uint8_t)((now >> 0) & 0xFFU);

                                network->m_peers[peerId] = connection;
                                network->writePeerCommand(peerId, { NET_FUNC::PONG, NET_SUBFUNC::NOP }, payload, 8U);

                                if (network->m_reportPeerPing) {
                                    LogInfoEx(LOG_NET, "PEER %u (%s) ping, pingsReceived = %u, lastPing = %u", peerId, connection->identity().c_str(),
                                        connection->pingsReceived(), lastPing);
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, TAG_REPEATER_PING);
                            }
                        }
                    }
                }
                break;

            case NET_FUNC::GRANT_REQ:                                                                   // Repeater Grant Request
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                uint32_t srcId = __GET_UINT16(req->buffer, 11U);                // Source Address
                                uint32_t dstId = __GET_UINT16(req->buffer, 15U);                // Destination Address

                                uint8_t slot = req->buffer[19U];

                                bool unitToUnit = (req->buffer[19U] & 0x80U) == 0x80U;

                                DVM_STATE state = (DVM_STATE)req->buffer[20U];                  // DVM Mode State
                                switch (state) {
                                case STATE_DMR:
                                    if (network->m_dmrEnabled) {
                                        if (network->m_tagDMR != nullptr) {
                                            network->m_tagDMR->processGrantReq(srcId, dstId, slot, unitToUnit, peerId, req->rtpHeader.getSequence(), streamId);
                                        } else {
                                            network->writePeerNAK(peerId, TAG_DMR_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                    break;
                                case STATE_P25:
                                    if (network->m_p25Enabled) {
                                        if (network->m_tagP25 != nullptr) {
                                            network->m_tagP25->processGrantReq(srcId, dstId, unitToUnit, peerId, req->rtpHeader.getSequence(), streamId);
                                        } else {
                                            network->writePeerNAK(peerId, TAG_P25_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                    break;
                                case STATE_NXDN:
                                    if (network->m_nxdnEnabled) {
                                        if (network->m_tagNXDN != nullptr) {
                                            network->m_tagNXDN->processGrantReq(srcId, dstId, unitToUnit, peerId, req->rtpHeader.getSequence(), streamId);
                                        } else {
                                            network->writePeerNAK(peerId, TAG_NXDN_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                    break;
                                default:
                                    network->writePeerNAK(peerId, TAG_REPEATER_GRANT, NET_CONN_NAK_ILLEGAL_PACKET);
                                    Utils::dump("unknown state for grant request from the peer", req->buffer, req->length);
                                    break;
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, TAG_REPEATER_GRANT, NET_CONN_NAK_FNE_UNAUTHORIZED);
                            }
                        }
                    }
                }
                break;

            case NET_FUNC::TRANSFER:
                {
                    // are activity/diagnostic transfers occurring from the alternate port?
                    if (network->m_host->m_useAlternatePortForDiagnostics) {
                        break; // for performance and other reasons -- simply ignore the entire NET_FUNC::TRANSFER at this point
                               // since they should be coming from the alternate port anyway
                    }

                    if (req->fneHeader.getSubFunction() == NET_SUBFUNC::TRANSFER_SUBFUNC_ACTIVITY) {    // Peer Activity Log Transfer
                        if (network->m_allowActivityTransfer) {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip) {
                                        uint8_t rawPayload[req->length - 11U];
                                        ::memset(rawPayload, 0x00U, req->length - 11U);
                                        ::memcpy(rawPayload, req->buffer + 11U, req->length - 11U);
                                        std::string payload(rawPayload, rawPayload + (req->length - 11U));

                                        ::ActivityLog("%.9u (%8s) %s", peerId, connection->identity().c_str(), payload.c_str());

                                        // report activity log to InfluxDB
                                        if (network->m_enableInfluxDB) {
                                            influxdb::QueryBuilder()
                                                .meas("activity")
                                                    .tag("peerId", std::to_string(peerId))
                                                        .field("identity", connection->identity())
                                                        .field("msg", payload)
                                                    .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                                                .request(network->m_influxServer);
                                        }
                                    }
                                    else {
                                        network->writePeerNAK(peerId, TAG_TRANSFER_ACT_LOG, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::TRANSFER_SUBFUNC_DIAG) {   // Peer Diagnostic Log Transfer
                        if (network->m_allowDiagnosticTransfer) {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip) {
                                        uint8_t rawPayload[req->length - 11U];
                                        ::memset(rawPayload, 0x00U, req->length - 11U);
                                        ::memcpy(rawPayload, req->buffer + 11U, req->length - 11U);
                                        std::string payload(rawPayload, rawPayload + (req->length - 11U));

                                        bool currState = g_disableTimeDisplay;
                                        g_disableTimeDisplay = true;
                                        ::Log(9999U, nullptr, "%.9u (%8s) %s", peerId, connection->identity().c_str(), payload.c_str());
                                        g_disableTimeDisplay = currState;

                                        // report diagnostic log to InfluxDB
                                        if (network->m_enableInfluxDB) {
                                            influxdb::QueryBuilder()
                                                .meas("diag")
                                                    .tag("peerId", std::to_string(peerId))
                                                        .field("identity", connection->identity())
                                                        .field("msg", payload)
                                                    .timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
                                                .request(network->m_influxServer);
                                        }
                                    }
                                    else {
                                        network->writePeerNAK(peerId, TAG_TRANSFER_DIAG_LOG, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS) { // Peer Status Transfer
                        // main traffic port status transfers aren't supported for performance reasons
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_TRANSFER, NET_CONN_NAK_ILLEGAL_PACKET);
                        Utils::dump("unknown transfer opcode from the peer", req->buffer, req->length);
                    }
                }
                break;

            case NET_FUNC::ANNOUNCE:
                {
                    if (req->fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL) {       // Announce Group Affiliation
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(req->address);
                                lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                if (aff == nullptr) {
                                    LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", peerId, connection->identity().c_str());
                                }

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                    uint32_t srcId = __GET_UINT16(req->buffer, 0U);             // Source Address
                                    uint32_t dstId = __GET_UINT16(req->buffer, 3U);             // Destination Address
                                    aff->groupUnaff(srcId);
                                    aff->groupAff(srcId, dstId);
                                }
                                else {
                                    network->writePeerNAK(peerId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                }
                            }
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG) {   // Announce Unit Registration
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(req->address);
                                lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                if (aff == nullptr) {
                                    LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", peerId, connection->identity().c_str());
                                }

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                    uint32_t srcId = __GET_UINT16(req->buffer, 0U);             // Source Address
                                    aff->unitReg(srcId);
                                }
                                else {
                                    network->writePeerNAK(peerId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                }
                            }
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_UNIT_DEREG) { // Announce Unit Deregistration
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(req->address);
                                lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                if (aff == nullptr) {
                                    LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", peerId, connection->identity().c_str());
                                }

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                    uint32_t srcId = __GET_UINT16(req->buffer, 0U);             // Source Address
                                    aff->unitDereg(srcId);
                                }
                                else {
                                    network->writePeerNAK(peerId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                }
                            }
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_AFFILS) {     // Announce Update All Affiliations
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(req->address);

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip) {
                                    lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                    if (aff == nullptr) {
                                        LogError(LOG_NET, "PEER %u (%s) has an invalid affiliations lookup? This shouldn't happen BUGBUG.", peerId, connection->identity().c_str());
                                    }

                                    if (aff != nullptr) {
                                        aff->clearGroupAff(0U, true);

                                        // update TGID lists
                                        uint32_t len = __GET_UINT32(req->buffer, 0U);
                                        uint32_t offs = 4U;
                                        for (uint32_t i = 0; i < len; i++) {
                                            uint32_t srcId = __GET_UINT16(req->buffer, offs);
                                            uint32_t dstId = __GET_UINT16(req->buffer, offs + 4U);

                                            aff->groupAff(srcId, dstId);
                                            offs += 8U;
                                        }
                                        LogMessage(LOG_NET, "PEER %u (%s) announced %u affiliations", peerId, connection->identity().c_str(), len);
                                    }
                                }
                                else {
                                    network->writePeerNAK(peerId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                }
                            }
                        }
                    }
                    else if (req->fneHeader.getSubFunction() == NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC) {    // Announce Site VCs
                        if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                            FNEPeerConnection* connection = network->m_peers[peerId];
                            if (connection != nullptr) {
                                std::string ip = udp::Socket::address(req->address);

                                // validate peer (simple validation really)
                                if (connection->connected() && connection->address() == ip) {
                                    std::vector<uint32_t> vcPeers;

                                    // update peer association
                                    uint32_t len = __GET_UINT32(req->buffer, 0U);
                                    uint32_t offs = 4U;
                                    for (uint32_t i = 0; i < len; i++) {
                                        uint32_t vcPeerId = __GET_UINT32(req->buffer, offs);
                                        if (vcPeerId > 0 && (network->m_peers.find(vcPeerId) != network->m_peers.end())) {
                                            FNEPeerConnection* vcConnection = network->m_peers[vcPeerId];
                                            if (vcConnection != nullptr) {
                                                vcConnection->ccPeerId(peerId);
                                                vcPeers.push_back(vcPeerId);
                                            }
                                        }
                                        offs += 4U;
                                    }
                                    LogMessage(LOG_NET, "PEER %u (%s) announced %u VCs", peerId, connection->identity().c_str(), len);
                                    network->m_ccPeerMap[peerId] = vcPeers;
                                }
                                else {
                                    network->writePeerNAK(peerId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                }
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_ANNOUNCE, NET_CONN_NAK_ILLEGAL_PACKET);
                        Utils::dump("unknown announcement opcode from the peer", req->buffer, req->length);
                    }
                }
                break;

            default:
                Utils::dump("unknown opcode from the peer", req->buffer, req->length);
                break;
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }

    return nullptr;
}

/* Checks if the passed peer ID is blocked from unit-to-unit traffic. */

bool FNENetwork::checkU2UDroppedPeer(uint32_t peerId)
{
    if (m_dropU2UPeerTable.empty())
        return false;

    if (std::find(m_dropU2UPeerTable.begin(), m_dropU2UPeerTable.end(), peerId) != m_dropU2UPeerTable.end()) {
        return true;
    }

    return false;
}

/* Helper to create a peer on the peers affiliations list. */

void FNENetwork::createPeerAffiliations(uint32_t peerId, std::string peerName)
{
    erasePeerAffiliations(peerId);

    std::lock_guard<std::mutex> lock(m_peerMutex);
    lookups::ChannelLookup* chLookup = new lookups::ChannelLookup();
    m_peerAffiliations[peerId] = new lookups::AffiliationLookup(peerName, chLookup, m_verbose);
    m_peerAffiliations[peerId]->setDisableUnitRegTimeout(true); // FNE doesn't allow unit registration timeouts (notification must come from the peers)
}

/* Helper to erase the peer from the peers affiliations list. */

bool FNENetwork::erasePeerAffiliations(uint32_t peerId)
{
    std::lock_guard<std::mutex> lock(m_peerMutex);
    auto it = std::find_if(m_peerAffiliations.begin(), m_peerAffiliations.end(), [&](PeerAffiliationMapPair x) { return x.first == peerId; });
    if (it != m_peerAffiliations.end()) {
        lookups::AffiliationLookup* aff = m_peerAffiliations[peerId];
        if (aff != nullptr) {
            lookups::ChannelLookup* rfCh = aff->rfCh();
            if (rfCh != nullptr)
                delete rfCh;
            delete aff;
        }
        m_peerAffiliations.erase(peerId);

        return true;
    }

    return false;
}

/* Helper to erase the peer from the peers list. */

bool FNENetwork::erasePeer(uint32_t peerId)
{
    std::lock_guard<std::mutex> lock(m_peerMutex);
    {
        auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
        if (it != m_peers.end()) {
            m_peers.erase(peerId);
            return true;
        }
    }

    // erase any CC maps for this peer
    {
        auto it = std::find_if(m_ccPeerMap.begin(), m_ccPeerMap.end(), [&](auto x) { return x.first == peerId; });
        if (it != m_ccPeerMap.end()) {
            m_ccPeerMap.erase(peerId);
            return true;
        }
    }

    return false;
}

/* Helper to reset a peer connection. */

bool FNENetwork::resetPeer(uint32_t peerId)
{
    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            sockaddr_storage addr = connection->socketStorage();
            uint32_t addrLen = connection->sockStorageLen();

            LogInfoEx(LOG_NET, "PEER %u (%s) resetting peer connection", peerId, connection->identity().c_str());

            writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_PEER_RESET, addr, addrLen);

            delete connection;
            erasePeer(peerId);

            return true;
        }
    }

    LogWarning(LOG_NET, "PEER %u reset failed; peer not found.", peerId);
    return false;
}

/* Helper to resolve the peer ID to its identity string. */

std::string FNENetwork::resolvePeerIdentity(uint32_t peerId)
{
    std::lock_guard<std::mutex> lock(m_peerMutex);
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        if (it->second != nullptr) {
            FNEPeerConnection* peer = it->second;
            return peer->identity();
        }
    }

    return std::string();
}

/* Helper to complete setting up a repeater login request. */

void FNENetwork::setupRepeaterLogin(uint32_t peerId, FNEPeerConnection* connection)
{
    std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
    connection->salt(dist(m_random));

    LogInfoEx(LOG_NET, "PEER %u started login from, %s:%u", peerId, connection->address().c_str(), connection->port());

    connection->connectionState(NET_STAT_WAITING_AUTHORISATION);
    m_peers[peerId] = connection;

    // transmit salt to peer
    uint8_t salt[4U];
    ::memset(salt, 0x00U, 4U);
    __SET_UINT32(connection->salt(), salt, 0U);

    writePeerACK(peerId, salt, 4U);
    LogInfoEx(LOG_NET, "PEER %u RPTL ACK, challenge response sent for login", peerId);
}

/* Helper to send the ACL lists to the specified peer in a separate thread. */

void FNENetwork::peerACLUpdate(uint32_t peerId)
{
    ACLUpdateRequest* req = new ACLUpdateRequest();
    req->peerId = peerId;

    std::stringstream peerName;
    peerName << peerId << ":acl-update";

    if (!Thread::runAsThread(this, threadedACLUpdate, req)) {
        delete req;
        return;
    }

    // pthread magic to rename the thread properly
#ifdef _GNU_SOURCE
    ::pthread_setname_np(req->thread, peerName.str().c_str());
#endif // _GNU_SOURCE
}

/* Helper to send the ACL lists to the specified peer in a separate thread. */

void* FNENetwork::threadedACLUpdate(void* arg)
{
    ACLUpdateRequest* req = (ACLUpdateRequest*)arg;
    if (req != nullptr) {
        ::pthread_detach(req->thread);

        FNENetwork* network = static_cast<FNENetwork*>(req->obj);
        if (network == nullptr) {
            delete req;
            return nullptr;
        }

        std::string peerIdentity = network->resolvePeerIdentity(req->peerId);
        LogInfoEx(LOG_NET, "PEER %u (%s) sending ACL list updates", req->peerId, peerIdentity.c_str());

        network->writeWhitelistRIDs(req->peerId);
        network->writeBlacklistRIDs(req->peerId);
        network->writeTGIDs(req->peerId);
        network->writeDeactiveTGIDs(req->peerId);

        delete req;
    }

    return nullptr;
}

/* Helper to send the list of whitelisted RIDs to the specified peer. */

void FNENetwork::writeWhitelistRIDs(uint32_t peerId)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

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

    // send a chunk of RIDs to the peer
    FNEPeerConnection* connection = m_peers[peerId];
    if (connection != nullptr) {
        uint32_t chunkCnt = (ridWhitelist.size() / MAX_RID_LIST_CHUNK) + 1U;
        for (uint32_t i = 0U; i < chunkCnt; i++) {
            size_t listSize = ridWhitelist.size();
            if (chunkCnt > 1U) {
                listSize = MAX_RID_LIST_CHUNK;

                if (i == chunkCnt - 1U) {
                    // this is a disgusting dirty hack...
                    listSize = ::abs((long)((i * MAX_RID_LIST_CHUNK) - ridWhitelist.size()));
                }
            }

            if (listSize > ridWhitelist.size()) {
                listSize = ridWhitelist.size();
            }

            // Ignore lists of size 0 (happens on even multiples of 50, TODO: there's probably a better fix for this)
            if (listSize == 0) {
                continue;
            }

            // build dataset
            uint16_t bufSize = 4U + (listSize * 4U);
            uint8_t payload[bufSize];
            ::memset(payload, 0x00U, bufSize);

            __SET_UINT32(listSize, payload, 0U);

            // write whitelisted IDs to whitelist payload
            uint32_t offs = 4U;
            for (uint32_t j = 0; j < listSize; j++) {
                uint32_t id = ridWhitelist.at(j + (i * MAX_RID_LIST_CHUNK));

                if (m_debug)
                    LogDebug(LOG_NET, "PEER %u (%s) whitelisting RID %u (%d / %d)", peerId, connection->identity().c_str(),
                        id, i, j);

                __SET_UINT32(id, payload, offs);
                offs += 4U;
            }

            writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_SUBFUNC_WL_RID },
                payload, bufSize, true);
        }

        connection->lastPing(now);
    }
}

/* Helper to send the list of whitelisted RIDs to the specified peer. */

void FNENetwork::writeBlacklistRIDs(uint32_t peerId)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

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

    // send a chunk of RIDs to the peer
    FNEPeerConnection* connection = m_peers[peerId];
    if (connection != nullptr) {
        uint32_t chunkCnt = (ridBlacklist.size() / MAX_RID_LIST_CHUNK) + 1U;
        for (uint32_t i = 0U; i < chunkCnt; i++) {
            size_t listSize = ridBlacklist.size();
            if (chunkCnt > 1U) {
                listSize = MAX_RID_LIST_CHUNK;
                
                if (i == chunkCnt - 1U) {
                    // this is a disgusting dirty hack...
                    listSize = ::abs((long)((i * MAX_RID_LIST_CHUNK) - ridBlacklist.size()));
                }
            }

            if (listSize > ridBlacklist.size()) {
                listSize = ridBlacklist.size();
            }

            // Ignore lists of size 0 (happens on even multiples of 50, TODO: there's probably a better fix for this)
            if (listSize == 0) {
                continue;
            }

            // build dataset
            uint16_t bufSize = 4U + (listSize * 4U);
            uint8_t payload[bufSize];
            ::memset(payload, 0x00U, bufSize);

            __SET_UINT32(listSize, payload, 0U);

            // write blacklisted IDs to blacklist payload
            uint32_t offs = 4U;
            for (uint32_t j = 0; j < listSize; j++) {
                uint32_t id = ridBlacklist.at(j + (i * MAX_RID_LIST_CHUNK));

                if (m_debug)
                    LogDebug(LOG_NET, "PEER %u (%s) blacklisting RID %u (%d / %d)", peerId, connection->identity().c_str(),
                        id, i, j);

                __SET_UINT32(id, payload, offs);
                offs += 4U;
            }

            writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_SUBFUNC_BL_RID },
                payload, bufSize, true);
        }

        connection->lastPing(now);
    }
}

/* Helper to send the list of active TGIDs to the specified peer. */

void FNENetwork::writeTGIDs(uint32_t peerId)
{
    if (!m_tidLookup->sendTalkgroups()) {
        return;
    }

    std::vector<std::pair<uint32_t, uint8_t>> tgidList;
    auto groupVoice = m_tidLookup->groupVoice();
    for (auto entry : groupVoice) {
        std::vector<uint32_t> inclusion = entry.config().inclusion();
        std::vector<uint32_t> exclusion = entry.config().exclusion();
        std::vector<uint32_t> preferred = entry.config().preferred();

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

        // determine if the peer is non-preferred
        bool nonPreferred = false;
        if (preferred.size() > 0) {
            auto it = std::find(preferred.begin(), preferred.end(), peerId);
            if (it == preferred.end()) {
                nonPreferred = true;
            }
        }

        if (entry.config().active()) {
            uint8_t slotNo = entry.source().tgSlot();

            // set upper bit of the slot number to flag non-preferred
            if (nonPreferred) {
                slotNo = 0x80U + (slotNo & 0x03U);
            }

            tgidList.push_back({ entry.source().tgId(), slotNo });
        }
    }

    // build dataset
    uint8_t payload[4U + (tgidList.size() * 5U)];
    ::memset(payload, 0x00U, 4U + (tgidList.size() * 5U));

    __SET_UINT32(tgidList.size(), payload, 0U);

    // write talkgroup IDs to active TGID payload
    uint32_t offs = 4U;
    for (std::pair<uint32_t, uint8_t> tg : tgidList) {
        if (m_debug) {
            std::string peerIdentity = resolvePeerIdentity(peerId);
            LogDebug(LOG_NET, "PEER %u (%s) activating TGID %u TS %u", peerId, peerIdentity.c_str(),
                tg.first, tg.second);
        }
        __SET_UINT32(tg.first, payload, offs);
        payload[offs + 4U] = tg.second;
        offs += 5U;
    }

    writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_SUBFUNC_ACTIVE_TGS },
        payload, 4U + (tgidList.size() * 5U), true);
}

/* Helper to send the list of deactivated TGIDs to the specified peer. */

void FNENetwork::writeDeactiveTGIDs(uint32_t peerId)
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
        if (m_debug) {
            std::string peerIdentity = resolvePeerIdentity(peerId);
            LogDebug(LOG_NET, "PEER %u (%s) deactivating TGID %u TS %u", peerId, peerIdentity.c_str(),
                tg.first, tg.second);
        }
        __SET_UINT32(tg.first, payload, offs);
        payload[offs + 4U] = tg.second;
        offs += 5U;
    }

    writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_SUBFUNC_DEACTIVE_TGS }, 
        payload, 4U + (tgidList.size() * 5U), true);
}

/* Helper to send a data message to the specified peer. */

bool FNENetwork::writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data,
    uint32_t length, uint16_t pktSeq, uint32_t streamId, bool queueOnly, bool directWrite) const
{
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        FNEPeerConnection* connection = m_peers.at(peerId);
        if (connection != nullptr) {
            uint32_t peerStreamId = connection->currStreamId();
            if (streamId == 0U) {
                streamId = peerStreamId;
            }
            sockaddr_storage addr = connection->socketStorage();
            uint32_t addrLen = connection->sockStorageLen();

            if (directWrite)
                return m_frameQueue->write(data, length, streamId, peerId, m_peerId, opcode, pktSeq, addr, addrLen);
            else {
                m_frameQueue->enqueueMessage(data, length, streamId, peerId, m_peerId, opcode, pktSeq, addr, addrLen);
                if (queueOnly)
                    return true;
                return m_frameQueue->flushQueue();
            }
        }
    }

    return false;
}

/* Helper to send a data message to the specified peer. */

bool FNENetwork::writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data,
    uint32_t length, uint32_t streamId, bool queueOnly, bool incPktSeq, bool directWrite) const
{
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        FNEPeerConnection* connection = m_peers.at(peerId);
        if (connection != nullptr) {
            if (incPktSeq) {
                connection->pktLastSeq(connection->pktLastSeq() + 1);
            }
            uint16_t pktSeq = connection->pktLastSeq();

            return writePeer(peerId, opcode, data, length, pktSeq, streamId, queueOnly, directWrite);
        }
    }

    return false;
}

/* Helper to send a command message to the specified peer. */

bool FNENetwork::writePeerCommand(uint32_t peerId, FrameQueue::OpcodePair opcode,
    const uint8_t* data, uint32_t length, bool incPktSeq) const
{
    assert(peerId > 0);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + 6U, data, length);
    }

    uint32_t len = length + 6U;
    return writePeer(peerId, opcode, buffer, len, 0U, false, incPktSeq, true);
}

/* Helper to send a ACK response to the specified peer. */

bool FNENetwork::writePeerACK(uint32_t peerId, const uint8_t* data, uint32_t length)
{
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    __SET_UINT32(peerId, buffer, 0U);                                           // Peer ID

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + 6U, data, length);
    }

    return writePeer(peerId, { NET_FUNC::ACK, NET_SUBFUNC::NOP }, buffer, length + 10U, RTP_END_OF_CALL_SEQ, false, true);
}

/* Helper to log a warning specifying which NAK reason is being sent a peer. */

void FNENetwork::logPeerNAKReason(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason)
{
    switch (reason) {
    case NET_CONN_NAK_MODE_NOT_ENABLED:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; digital mode not enabled on FNE", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_ILLEGAL_PACKET:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; illegal/unknown packet", peerId ,tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_FNE_UNAUTHORIZED:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; unauthorized", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_BAD_CONN_STATE:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; bad connection state", peerId ,tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_INVALID_CONFIG_DATA:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; invalid configuration data", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_FNE_MAX_CONN:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; FNE has reached maximum permitted connections", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_PEER_RESET:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; FNE demanded connection reset", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_PEER_ACL:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; ACL rejection", peerId, tag, (uint16_t)reason);
        break;

    case NET_CONN_NAK_GENERAL_FAILURE:
    default:
        LogWarning(LOG_NET, "PEER %u NAK %s, reason = %u; general failure", peerId, tag, (uint16_t)reason);
        break;
    }
}

/* Helper to send a NAK response to the specified peer. */

bool FNENetwork::writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason)
{
    assert(peerId > 0);
    assert(tag != nullptr);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID
    __SET_UINT16B((uint16_t)reason, buffer, 10U);                               // Reason

    logPeerNAKReason(peerId, tag, reason);
    return writePeer(peerId, { NET_FUNC::NAK, NET_SUBFUNC::NOP }, buffer, 10U, RTP_END_OF_CALL_SEQ, false, true);
}

/* Helper to send a NAK response to the specified peer. */

bool FNENetwork::writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason, sockaddr_storage& addr, uint32_t addrLen)
{
    assert(peerId > 0);
    assert(tag != nullptr);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID
    __SET_UINT16B((uint16_t)reason, buffer, 10U);                               // Reason

    logPeerNAKReason(peerId, tag, reason);
    return m_frameQueue->write(buffer, 12U, createStreamId(), peerId, m_peerId,
        { NET_FUNC::NAK, NET_SUBFUNC::NOP }, 0U, addr, addrLen);
}
