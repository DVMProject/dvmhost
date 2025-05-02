// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/edac/SHA256.h"
#include "common/network/json/json.h"
#include "common/p25/kmm/KMMFactory.h"
#include "common/zlib/Compression.h"
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
using namespace compress;

#include <cassert>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <streambuf>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t MAX_HARD_CONN_CAP = 250U;
const uint8_t MAX_PEER_LIST_BEFORE_FLUSH = 10U;
const uint32_t MAX_RID_LIST_CHUNK = 50U;

const uint64_t PACKET_LATE_TIME = 200U; // 200ms

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::timed_mutex FNENetwork::m_keyQueueMutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the FNENetwork class. */

FNENetwork::FNENetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
    bool debug, bool verbose, bool reportPeerPing, bool dmr, bool p25, bool nxdn, uint32_t parrotDelay, bool parrotGrantDemand,
    bool allowActivityTransfer, bool allowDiagnosticTransfer, uint32_t pingTime, uint32_t updateLookupTime, uint16_t workerCnt) :
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
    m_peerLinkPeers(),
    m_peerAffiliations(),
    m_ccPeerMap(),
    m_peerLinkKeyQueue(),
    m_peerLinkActPkt(),
    m_maintainenceTimer(1000U, pingTime),
    m_updateLookupTime(updateLookupTime * 60U),
    m_softConnLimit(0U),
    m_callInProgress(false),
    m_disallowAdjStsBcast(false),
    m_disallowExtAdjStsBcast(true),
    m_allowConvSiteAffOverride(false),
    m_disallowCallTerm(false),
    m_restrictGrantToAffOnly(false),
    m_enableInCallCtrl(true),
    m_rejectUnknownRID(false),
    m_filterHeaders(true),
    m_filterTerminators(true),
    m_disallowU2U(false),
    m_dropU2UPeerTable(),
    m_enableInfluxDB(false),
    m_influxServerAddress("127.0.0.1"),
    m_influxServerPort(8086U),
    m_influxServerToken(),
    m_influxOrg("dvm"),
    m_influxBucket("dvm"),
    m_influxLogRawData(false),
    m_threadPool(workerCnt, "fne"),
    m_disablePacketData(false),
    m_dumpPacketData(false),
    m_verbosePacketData(false),
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
    m_enableInCallCtrl = conf["enableInCallCtrl"].as<bool>(false);
    m_rejectUnknownRID = conf["rejectUnknownRID"].as<bool>(false);
    m_disallowCallTerm = conf["disallowCallTerm"].as<bool>(false);
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
    m_dumpPacketData = conf["dumpPacketData"].as<bool>(false);
    m_verbosePacketData = conf["verbosePacketData"].as<bool>(false);

    /*
    ** Drop Unit to Unit Peers
    */

    m_disallowU2U = conf["disallowAllUnitToUnit"].as<bool>(false);

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
        LogInfo("    Dump Packet Data: %s", m_dumpPacketData ? "yes" : "no");
        LogInfo("    Disable P25 ADJ_STS_BCAST to external peers: %s", m_disallowExtAdjStsBcast ? "yes" : "no");
        LogInfo("    Disable P25 TDULC call termination broadcasts to any peers: %s", m_disallowCallTerm ? "yes" : "no");
        LogInfo("    Allow conventional sites to override affiliation and receive all traffic: %s", m_allowConvSiteAffOverride ? "yes" : "no");
        LogInfo("    Enable In-Call Control: %s", m_enableInCallCtrl ? "yes" : "no");
        LogInfo("    Reject Unknown RIDs: %s", m_rejectUnknownRID ? "yes" : "no");
        LogInfo("    Restrict grant response by affiliation: %s", m_restrictGrantToAffOnly ? "yes" : "no");
        LogInfo("    Traffic Headers Filtered by Destination ID: %s", m_filterHeaders ? "yes" : "no");
        LogInfo("    Traffic Terminators Filtered by Destination ID: %s", m_filterTerminators ? "yes" : "no");
        LogInfo("    Disallow Unit-to-Unit: %s", m_disallowU2U ? "yes" : "no");
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

/* Sets the instances of the Radio ID, Talkgroup ID Peer List, and Crypto lookup tables. */

void FNENetwork::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup, lookups::PeerListLookup* peerListLookup,
    CryptoContainer* cryptoLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
    m_peerListLookup = peerListLookup;
    m_cryptoLookup = cryptoLookup;
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
        req->obj = this;
        req->peerId = peerId;

        req->address = address;
        req->addrLen = addrLen;
        req->rtpHeader = rtpHeader;
        req->fneHeader = fneHeader;

        req->pktRxTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        req->length = length;
        req->buffer = new uint8_t[length];
        ::memcpy(req->buffer, buffer.get(), length);

        // enqueue the task
        if (!m_threadPool.enqueue(new_pooltask(taskNetworkRx, req))) {
            LogError(LOG_NET, "Failed to task enqueue network packet request, peerId = %u, %s:%u", peerId, 
                udp::Socket::address(address).c_str(), udp::Socket::port(address));
            if (req != nullptr) {
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
            }
        }
    }
}

/* Updates the timer by the passed number of milliseconds. */

void FNENetwork::clock(uint32_t ms)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

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
                uint64_t dt = 0U;
                if (connection->isExternalPeer() || connection->isPeerLink())
                    dt = connection->lastPing() + ((m_host->m_pingTime * 1000) * (m_host->m_maxMissedPings * 2U));
                else
                    dt = connection->lastPing() + ((m_host->m_pingTime * 1000) * m_host->m_maxMissedPings);

                if (dt < now) {
                    LogInfoEx(LOG_NET, "PEER %u (%s) timed out, dt = %u, now = %u", id, connection->identity().c_str(),
                        dt, now);

                    // set connection states for this stale connection
                    connection->connected(false);
                    connection->connectionState(NET_STAT_INVALID);

                    // if the connection was an external peer or a peer link -- be noisy about a possible
                    // netsplit
                    if (connection->isExternalPeer() || connection->isPeerLink()) {
                        for (uint8_t i = 0U; i < 3U; i++)
                            LogWarning(LOG_NET, "PEER %u (%s) downstream netsplit, dt = %u, now = %u", id, connection->identity().c_str(),
                                dt, now);
                    }

                    peersToRemove.push_back(id);
                }
            }
        }

        // remove any peers
        for (uint32_t peerId : peersToRemove) {
            FNEPeerConnection* connection = m_peers[peerId];
            erasePeer(peerId);
            if (connection != nullptr) {
                delete connection;
            }
        }

        // roll the RTP timestamp if no call is in progress
        if (!m_callInProgress) {
            frame::RTPHeader::resetStartTime();
            m_frameQueue->clearTimestamps();
        }

        // send active peer list to Peer-Link masters
        if (m_host->m_peerNetworks.size() > 0) {
            for (auto peer : m_host->m_peerNetworks) {
                if (peer.second != nullptr) {
                    if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                        if (!peer.second->getAttachedKeyRSPHandler()) {
                            peer.second->setAttachedKeyRSPHandler(true); // this is the only place this should happen
                            peer.second->setKeyResponseCallback([=](p25::kmm::KeyItem ki, uint8_t algId, uint8_t keyLength) {
                                processTEKResponse(&ki, algId, keyLength);
                            });
                        }

                        if (m_peers.size() > 0) {
                            json::array peers = json::array();
                            for (auto entry : m_peers) {
                                uint32_t peerId = entry.first;
                                network::FNEPeerConnection* peerConn = entry.second;
                                if (peerConn != nullptr) {
                                    json::object peerObj = fneConnObject(peerId, peerConn);
                                    uint32_t peerNetPeerId = peer.second->getPeerId();
                                    peerObj["parentPeerId"].set<uint32_t>(peerNetPeerId);
                                    peers.push_back(json::value(peerObj));
                                }
                            }

                            peer.second->writePeerLinkPeers(&peers);
                        }
                    }
                }
            }
        }

        // send ACL updates forcibly to any Peer-Link peers
        for (auto peer : m_peers) {
            uint32_t id = peer.first;
            FNEPeerConnection* connection = peer.second;
            if (connection != nullptr) {
                if (connection->connected() && connection->isPeerLink()) {
                    // does this peer need an ACL update?
                    uint64_t dt = connection->lastACLUpdate() + (m_updateLookupTime * 1000);
                    if (dt < now) {
                        LogInfoEx(LOG_NET, "PEER %u (%s) updating ACL list, dt = %u, now = %u", id, connection->identity().c_str(),
                            dt, now);
                        peerACLUpdate(id);
                        connection->lastACLUpdate(now);
                    }
                }
            }
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

    // start thread pool
    m_threadPool.start();

    // start FluxQL thread pool
    if (m_enableInfluxDB) {
        influxdb::detail::TSCaller::start();
    }

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

        uint32_t streamId = createStreamId();
        for (auto peer : m_peers) {
            writePeer(peer.first, { NET_FUNC::MST_DISC, NET_SUBFUNC::NOP }, buffer, 1U, RTP_END_OF_CALL_SEQ, streamId, false);
        }
    }

    m_maintainenceTimer.stop();

    // stop thread pool
    m_threadPool.stop();
    m_threadPool.wait();

    // stop FluxQL thread pool
    if (m_enableInfluxDB) {
        influxdb::detail::TSCaller::stop();
        influxdb::detail::TSCaller::wait();
    }

    m_socket->close();

    m_status = NET_STAT_INVALID;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Process a data frames from the network. */

void FNENetwork::taskNetworkRx(NetPacketRequest* req)
{
    if (req != nullptr) {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        FNENetwork* network = static_cast<FNENetwork*>(req->obj);
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
            uint32_t peerId = req->fneHeader.getPeerId();
            uint32_t streamId = req->fneHeader.getStreamId();

            // determine if this packet is late (i.e. are we processing this packet more than 200ms after it was received?)
            uint64_t dt = req->pktRxTime + PACKET_LATE_TIME;
            if (dt < now) {
                std::string peerIdentity = network->resolvePeerIdentity(peerId);
                LogWarning(LOG_NET, "PEER %u (%s) packet processing latency >200ms, dt = %u, now = %u", peerId, peerIdentity.c_str(),
                    dt, now);
            }

            // update current peer packet sequence and stream ID
            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end()) && streamId != 0U) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                uint16_t pktSeq = req->rtpHeader.getSequence();

                if (connection != nullptr) {
                    if (pktSeq == RTP_END_OF_CALL_SEQ) {
                        // only reset packet sequences if we're a PROTOCOL or RPTC function
                        if ((req->fneHeader.getFunction() == NET_FUNC::PROTOCOL) ||
                            (req->fneHeader.getFunction() == NET_FUNC::RPTC)) {
                            connection->eraseStreamPktSeq(streamId); // attempt to erase packet sequence for the stream
                        }
                    } else {
                        if (connection->hasStreamPktSeq(streamId)) {
                            uint16_t currPkt = connection->getStreamPktSeq(streamId);
                            if ((pktSeq != currPkt) && (pktSeq != (RTP_END_OF_CALL_SEQ - 1U)) && pktSeq != 0U) {
                                LogWarning(LOG_NET, "PEER %u (%s) stream %u out-of-sequence; %u != %u", peerId, connection->identity().c_str(),
                                    streamId, pktSeq, currPkt);
                            }
                        }

                        connection->incStreamPktSeq(streamId, pktSeq + 1U);
                    }
                }

                network->m_peers[peerId] = connection;
            }

            // if we don't have a stream ID and are receiving call data -- throw an error and discard
            if (streamId == 0U && req->fneHeader.getFunction() == NET_FUNC::PROTOCOL) {
                std::string peerIdentity = network->resolvePeerIdentity(peerId);
                LogError(LOG_NET, "PEER %u (%s) malformed packet (no stream ID for a call?)", peerId, peerIdentity.c_str());

                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;

                return;
            }

            // process incoming message function opcodes
            switch (req->fneHeader.getFunction()) {
            case NET_FUNC::PROTOCOL:                                    // Protocol
                {
                    // process incoming message subfunction opcodes
                    switch (req->fneHeader.getSubFunction()) {
                    case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:             // Encapsulated DMR data frame
                        {
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
                                            network->writePeerNAK(peerId, streamId, TAG_DMR_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, TAG_DMR_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                            }
                        }
                        break;

                    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:             // Encapsulated P25 data frame
                        {
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
                                            network->writePeerNAK(peerId, streamId, TAG_P25_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, TAG_P25_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                            }
                        }
                        break;

                    case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:            // Encapsulated NXDN data frame
                        {
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
                                            network->writePeerNAK(peerId, streamId, TAG_NXDN_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, TAG_NXDN_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                            }
                        }
                        break;

                    default:
                        Utils::dump("unknown protocol opcode from peer", req->buffer, req->length);
                        break;
                    }
                }
                break;

            case NET_FUNC::RPTL:                                        // Repeater Login
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

                        network->setupRepeaterLogin(peerId, streamId, connection);

                        // check if the peer is in the peer ACL list
                        if (network->m_peerListLookup->getACL()) {
                            if (network->m_peerListLookup->isPeerListEmpty()) {
                                LogWarning(LOG_NET, "Peer List ACL enabled, but we have an empty peer list? Passing all peers.");
                            }

                            if (!network->m_peerListLookup->isPeerAllowed(peerId) && !network->m_peerListLookup->isPeerListEmpty()) {
                                if (network->m_peerListLookup->getMode() == lookups::PeerListLookup::BLACKLIST) {
                                    LogWarning(LOG_NET, "PEER %u RPTL, blacklisted from access", peerId);
                                } else {
                                    LogWarning(LOG_NET, "PEER %u RPTL, failed whitelist check", peerId);
                                }

                                network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);

                                network->erasePeer(peerId);
                                delete connection;
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

                                    network->erasePeerAffiliations(peerId);
                                    network->setupRepeaterLogin(peerId, streamId, connection);

                                    // check if the peer is in the peer ACL list
                                    if (network->m_peerListLookup->getACL()) {
                                        if (network->m_peerListLookup->isPeerListEmpty()) {
                                            LogWarning(LOG_NET, "Peer List ACL enabled, but we have an empty peer list? Passing all peers.");
                                        }

                                        if (!network->m_peerListLookup->isPeerAllowed(peerId) && !network->m_peerListLookup->isPeerListEmpty()) {
                                            if (network->m_peerListLookup->getMode() == lookups::PeerListLookup::BLACKLIST) {
                                                LogWarning(LOG_NET, "PEER %u RPTL, blacklisted from access", peerId);
                                            } else {
                                                LogWarning(LOG_NET, "PEER %u RPTL, failed whitelist check", peerId);
                                            }

                                            network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);

                                            network->erasePeer(peerId);
                                            delete connection;
                                        }
                                    }
                                } else {
                                    network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);

                                    LogWarning(LOG_NET, "PEER %u (%s) RPTL NAK, bad connection state, connectionState = %u", peerId, connection->identity().c_str(),
                                        connection->connectionState());

                                    network->erasePeer(peerId);
                                    delete connection;
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
            case NET_FUNC::RPTK:                                        // Repeater Authentication
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            connection->lastPing(now);

                            if (connection->connectionState() == NET_STAT_WAITING_AUTHORISATION) {
                                // get the hash from the frame message
                                UInt8Array __hash = std::make_unique<uint8_t[]>(req->length - 8U);
                                uint8_t* hash = __hash.get();
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
                                    if (!network->m_peerListLookup->isPeerAllowed(peerId) && !network->m_peerListLookup->isPeerListEmpty()) {
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

                                    if (network->m_peerListLookup->isPeerListEmpty()) {
                                        LogWarning(LOG_NET, "Peer List ACL enabled, but we have an empty peer list? Passing all peers.");
                                        validAcl = true;
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
                                        network->writePeerACK(peerId, streamId);
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
                                delete connection;
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);

                        network->erasePeer(peerId);
                        LogWarning(LOG_NET, "PEER %u RPTK NAK, having no connection", peerId);
                    }
                }
                break;
            case NET_FUNC::RPTC:                                        // Repeater Configuration
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            connection->lastPing(now);

                            if (connection->connectionState() == NET_STAT_WAITING_CONFIG) {
                                UInt8Array __rawPayload = std::make_unique<uint8_t[]>(req->length - 8U);
                                uint8_t* rawPayload = __rawPayload.get();
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
                                    delete connection;
                                }
                                else  {
                                    // ensure parsed JSON is an object
                                    if (!v.is<json::object>()) {
                                        LogWarning(LOG_NET, "PEER %u RPTC NAK, supplied invalid configuration data", peerId);
                                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_INVALID_CONFIG_DATA, req->address, req->addrLen);
                                        network->erasePeer(peerId);
                                        delete connection;
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

                                        network->writePeerACK(peerId, streamId, buffer, 1U);
                                        LogInfoEx(LOG_NET, "PEER %u RPTC ACK, completed the configuration exchange", peerId);

                                        json::object peerConfig = connection->config();
                                        if (peerConfig["identity"].is<std::string>()) {
                                            std::string identity = peerConfig["identity"].get<std::string>();
                                            connection->identity(identity);
                                            LogInfoEx(LOG_NET, "PEER %u reports identity [%8s]", peerId, identity.c_str());
                                        }

                                        // is the peer reporting it is an external peer?
                                        if (peerConfig["externalPeer"].is<bool>()) {
                                            bool external = peerConfig["externalPeer"].get<bool>();
                                            connection->isExternalPeer(external);
                                            if (external)
                                                LogInfoEx(LOG_NET, "PEER %u reports external peer", peerId);

                                            // check if the peer is participating in peer link
                                            lookups::PeerId peerEntry = network->m_peerListLookup->find(req->peerId);
                                            if (!peerEntry.peerDefault()) {
                                                if (peerEntry.peerLink()) {
                                                    if (network->m_host->m_useAlternatePortForDiagnostics) {
                                                        connection->isPeerLink(true);
                                                        if (external)
                                                            LogInfoEx(LOG_NET, "PEER %u configured for Peer-Link", peerId);
                                                    } else {
                                                        LogError(LOG_NET, "PEER %u, Peer-Link operations *require* the alternate diagnostics port option to be enabled.", peerId);
                                                        LogError(LOG_NET, "PEER %u, will not receive Peer-Link ACL updates.", peerId);
                                                    }
                                                }
                                            }
                                        }

                                        // is the peer reporting it is a conventional peer?
                                        if (peerConfig["conventionalPeer"].is<bool>()) {
                                            if (network->m_allowConvSiteAffOverride) {
                                                bool convPeer = peerConfig["conventionalPeer"].get<bool>();
                                                connection->isConventionalPeer(convPeer);
                                                if (convPeer)
                                                    LogInfoEx(LOG_NET, "PEER %u reports conventional peer", peerId);
                                            }
                                        }

                                        // is the peer reporting it is a SysView peer?
                                        if (peerConfig["sysView"].is<bool>()) {
                                            bool sysView = peerConfig["sysView"].get<bool>();
                                            connection->isSysView(sysView);
                                            if (sysView)
                                                LogInfoEx(LOG_NET, "PEER %u reports SysView peer", peerId);
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
                                delete connection;
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                        LogWarning(LOG_NET, "PEER %u RPTC NAK, having no connection", peerId);
                    }
                }
                break;

            case NET_FUNC::RPT_DISC:                                    // Repeater Disconnect
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                LogInfoEx(LOG_NET, "PEER %u (%s) disconnected", peerId, connection->identity().c_str());
                                network->erasePeer(peerId);
                                delete connection;
                            }
                        }
                    }
                }
                break;
            case NET_FUNC::PING:                                        // Repeater Ping
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
                                uint64_t dt = connection->lastACLUpdate() + (network->m_updateLookupTime * 1000);
                                if (dt < now) {
                                    if (connection->streamCount() <= 1 || ((dt * 2) < now)) {
                                        if ((dt * 2) < now)
                                            LogInfoEx(LOG_NET, "PEER %u (%s) late updating ACL list, dt = %u, ddt = %u, now = %u", peerId, connection->identity().c_str(),
                                                dt, dt * 2, now);
                                        else
                                            LogInfoEx(LOG_NET, "PEER %u (%s) updating ACL list, dt = %u,  now = %u", peerId, connection->identity().c_str(),
                                                dt, now);

                                        network->peerACLUpdate(peerId);
                                        connection->lastACLUpdate(now);
                                    }
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
                                network->writePeerCommand(peerId, { NET_FUNC::PONG, NET_SUBFUNC::NOP }, payload, 8U, streamId, false);

                                if (network->m_reportPeerPing) {
                                    LogInfoEx(LOG_NET, "PEER %u (%s) ping, pingsReceived = %u, lastPing = %u, now = %u", peerId, connection->identity().c_str(),
                                        connection->pingsReceived(), lastPing, now);
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, streamId, TAG_REPEATER_PING);
                            }
                        }
                    }
                }
                break;

            case NET_FUNC::GRANT_REQ:                                   // Repeater Grant Request
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
                                            network->writePeerNAK(peerId, streamId, TAG_DMR_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                    break;
                                case STATE_P25:
                                    if (network->m_p25Enabled) {
                                        if (network->m_tagP25 != nullptr) {
                                            network->m_tagP25->processGrantReq(srcId, dstId, unitToUnit, peerId, req->rtpHeader.getSequence(), streamId);
                                        } else {
                                            network->writePeerNAK(peerId, streamId, TAG_P25_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                    break;
                                case STATE_NXDN:
                                    if (network->m_nxdnEnabled) {
                                        if (network->m_tagNXDN != nullptr) {
                                            network->m_tagNXDN->processGrantReq(srcId, dstId, unitToUnit, peerId, req->rtpHeader.getSequence(), streamId);
                                        } else {
                                            network->writePeerNAK(peerId, streamId, TAG_NXDN_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                    break;
                                default:
                                    network->writePeerNAK(peerId, streamId, TAG_REPEATER_GRANT, NET_CONN_NAK_ILLEGAL_PACKET);
                                    Utils::dump("unknown state for grant request from the peer", req->buffer, req->length);
                                    break;
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, streamId, TAG_REPEATER_GRANT, NET_CONN_NAK_FNE_UNAUTHORIZED);
                            }
                        }
                    }
                }
                break;

            case NET_FUNC::INCALL_CTRL:                                 // In-Call Control
                {
                    // FNEs are god-like entities, and we don't recognize the authority of foreign FNEs telling us what
                    // to do...
                }
                break;

            case NET_FUNC::KEY_REQ:                                     // Enc. Key Request
                {
                    using namespace p25::defines;
                    using namespace p25::kmm;

                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                // is this peer allowed to request keys?
                                if (network->m_peerListLookup->getACL()) {
                                    if (network->m_peerListLookup->getMode() == lookups::PeerListLookup::WHITELIST) {
                                        lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                                        if (peerEntry.peerDefault()) {
                                            break;
                                        } else {
                                            if (!peerEntry.canRequestKeys()) {
                                                LogError(LOG_NET, "PEER %u (%s) requested enc. key but is not allowed, no response", peerId, connection->identity().c_str());
                                                break;
                                            }
                                        }
                                    }
                                }

                                std::unique_ptr<KMMFrame> frame = KMMFactory::create(req->buffer + 11U);
                                if (frame == nullptr) {
                                    LogWarning(LOG_NET, "PEER %u (%s), undecodable KMM frame from peer", peerId, connection->identity().c_str());
                                    break;
                                }

                                switch (frame->getMessageId()) {
                                case P25DEF::KMM_MessageType::MODIFY_KEY_CMD:
                                    {
                                        KMMModifyKey* modifyKey = static_cast<KMMModifyKey*>(frame.get());
                                        if (modifyKey->getAlgId() > 0U && modifyKey->getKId() > 0U) {
                                            LogMessage(LOG_NET, "PEER %u (%s) requested enc. key, algId = $%02X, kID = $%04X", peerId, connection->identity().c_str(),
                                                modifyKey->getAlgId(), modifyKey->getKId());
                                            ::KeyItem keyItem = network->m_cryptoLookup->find(modifyKey->getKId());
                                            if (!keyItem.isInvalid()) {
                                                uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
                                                ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                                uint8_t keyLength = keyItem.getKey(key);

                                                if (network->m_debug) {
                                                    LogDebugEx(LOG_HOST, "FNENetwork::threadedNetworkRx()", "keyLength = %u", keyLength);
                                                    Utils::dump(1U, "Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                                }

                                                LogMessage(LOG_NET, "PEER %u (%s) local enc. key, algId = $%02X, kID = $%04X", peerId, connection->identity().c_str(),
                                                    modifyKey->getAlgId(), modifyKey->getKId());

                                                // build response buffer
                                                uint8_t buffer[DATA_PACKET_LENGTH];
                                                ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

                                                KMMModifyKey modifyKeyRsp = KMMModifyKey();
                                                modifyKeyRsp.setDecryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE);
                                                modifyKeyRsp.setAlgId(modifyKey->getAlgId());
                                                modifyKeyRsp.setKId(0U);

                                                KeysetItem ks = KeysetItem();
                                                ks.keysetId(1U);
                                                ks.algId(modifyKey->getAlgId());
                                                ks.keyLength(keyLength);

                                                p25::kmm::KeyItem ki = p25::kmm::KeyItem();
                                                ki.keyFormat(KEY_FORMAT_TEK);
                                                ki.kId((uint16_t)keyItem.kId());
                                                ki.sln((uint16_t)keyItem.sln());
                                                ki.setKey(key, keyLength);

                                                ks.push_back(ki);
                                                modifyKeyRsp.setKeysetItem(ks);

                                                modifyKeyRsp.encode(buffer + 11U);

                                                network->writePeer(peerId, { NET_FUNC::KEY_RSP, NET_SUBFUNC::NOP }, buffer, modifyKeyRsp.length() + 11U, 
                                                    RTP_END_OF_CALL_SEQ, network->createStreamId(), false, false, true);
                                            } else {
                                                // attempt to forward KMM key request to Peer-Link masters
                                                if (network->m_host->m_peerNetworks.size() > 0) {
                                                    for (auto peer : network->m_host->m_peerNetworks) {
                                                        if (peer.second != nullptr) {
                                                            if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                                LogMessage(LOG_NET, "PEER %u (%s) no local key or container, requesting key from upstream master, algId = $%02X, kID = $%04X", peerId, connection->identity().c_str(),
                                                                    modifyKey->getAlgId(), modifyKey->getKId());

                                                                bool locked = network->m_keyQueueMutex.try_lock_for(std::chrono::milliseconds(60));
                                                                network->m_peerLinkKeyQueue[peerId] = modifyKey->getKId();

                                                                if (locked)
                                                                    network->m_keyQueueMutex.unlock();

                                                                peer.second->writeMaster({ NET_FUNC::KEY_REQ, NET_SUBFUNC::NOP }, 
                                                                    req->buffer, req->length, RTP_END_OF_CALL_SEQ, 0U, false, false);
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    break;

                                default:
                                    break;
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, streamId, TAG_REPEATER_KEY, NET_CONN_NAK_FNE_UNAUTHORIZED);
                            }
                        }
                    }
                }
                break;

            case NET_FUNC::TRANSFER:                                    // Transfer
                // transfer command is not supported for performance reasons on the main traffic port
                break;

            case NET_FUNC::ANNOUNCE:                                    // Announce
                {
                    // process incoming message subfunction opcodes
                    switch (req->fneHeader.getSubFunction()) {
                    case NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL:           // Announce Group Affiliation
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);
                                    lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                    if (aff == nullptr) {
                                        LogError(LOG_NET, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identity().c_str());
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                    }

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                        uint32_t srcId = __GET_UINT16(req->buffer, 0U);             // Source Address
                                        uint32_t dstId = __GET_UINT16(req->buffer, 3U);             // Destination Address
                                        aff->groupUnaff(srcId);
                                        aff->groupAff(srcId, dstId);

                                        // attempt to repeat traffic to Peer-Link masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false, false);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else {
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG:            // Announce Unit Registration
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);
                                    lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                    if (aff == nullptr) {
                                        LogError(LOG_NET, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identity().c_str());
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                    }

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                        uint32_t srcId = __GET_UINT16(req->buffer, 0U);             // Source Address
                                        aff->unitReg(srcId);

                                        // attempt to repeat traffic to Peer-Link masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false, false);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else {
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                        break;
                    case NET_SUBFUNC::ANNC_SUBFUNC_UNIT_DEREG:          // Announce Unit Deregistration
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);
                                    lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                    if (aff == nullptr) {
                                        LogError(LOG_NET, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identity().c_str());
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                    }

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                        uint32_t srcId = __GET_UINT16(req->buffer, 0U);             // Source Address
                                        aff->unitDereg(srcId);

                                        // attempt to repeat traffic to Peer-Link masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_DEREG }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false, false);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else {
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_GRP_UNAFFIL:         // Announce Group Affiliation Removal
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);
                                    lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                    if (aff == nullptr) {
                                        LogError(LOG_NET, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identity().c_str());
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                    }

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                        uint32_t srcId = __GET_UINT16(req->buffer, 0U);             // Source Address
                                        aff->groupUnaff(srcId);

                                        // attempt to repeat traffic to Peer-Link masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_GRP_UNAFFIL }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false, false);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else {
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_AFFILS:              // Announce Update All Affiliations
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip) {
                                        lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                        if (aff == nullptr) {
                                            LogError(LOG_NET, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identity().c_str());
                                            network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
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

                                            // attempt to repeat traffic to Peer-Link masters
                                            if (network->m_host->m_peerNetworks.size() > 0) {
                                                for (auto peer : network->m_host->m_peerNetworks) {
                                                    if (peer.second != nullptr) {
                                                        if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                            peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_AFFILS }, 
                                                                req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false, false);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else {
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                        break;

                    case NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC:             // Announce Site VCs
                        {
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

                                        // attempt to repeat traffic to Peer-Link masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isPeerLink()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false, false);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else {
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_FNE_UNAUTHORIZED);
                                    }
                                }
                            }
                        }
                        break;
                    default:
                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_ILLEGAL_PACKET);
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

/* Erases a stream ID from the given peer ID connection. */

void FNENetwork::eraseStreamPktSeq(uint32_t peerId, uint32_t streamId)
{
    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            connection->eraseStreamPktSeq(streamId);
        }
    }
}

/* Helper to create a peer on the peers affiliations list. */

void FNENetwork::createPeerAffiliations(uint32_t peerId, std::string peerName)
{
    erasePeerAffiliations(peerId);

    lookups::ChannelLookup* chLookup = new lookups::ChannelLookup();
    m_peerAffiliations[peerId] = new lookups::AffiliationLookup(peerName, chLookup, m_verbose);
    m_peerAffiliations[peerId]->setDisableUnitRegTimeout(true); // FNE doesn't allow unit registration timeouts (notification must come from the peers)
}

/* Helper to erase the peer from the peers affiliations list. */

bool FNENetwork::erasePeerAffiliations(uint32_t peerId)
{
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

void FNENetwork::erasePeer(uint32_t peerId)
{
    {
        auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
        if (it != m_peers.end()) {
            m_peers.erase(peerId);
        }
    }

    // erase any CC maps for this peer
    {
        auto it = std::find_if(m_ccPeerMap.begin(), m_ccPeerMap.end(), [&](auto x) { return x.first == peerId; });
        if (it != m_ccPeerMap.end()) {
            m_ccPeerMap.erase(peerId);
        }
    }

    // erase any Peer-Link entries for this peer
    {
        auto it = std::find_if(m_peerLinkPeers.begin(), m_peerLinkPeers.end(), [&](auto x) { return x.first == peerId; });
        if (it != m_peerLinkPeers.end()) {
            m_peerLinkPeers.erase(peerId);
        }
    }

    // cleanup peer affiliations
    erasePeerAffiliations(peerId);
}


/* Helper to create a JSON representation of a FNE peer connection. */

json::object FNENetwork::fneConnObject(uint32_t peerId, FNEPeerConnection *conn)
{
    json::object peerObj = json::object();
    peerObj["peerId"].set<uint32_t>(peerId);

    std::string address = conn->address();
    peerObj["address"].set<std::string>(address);
    uint16_t port = conn->port();
    peerObj["port"].set<uint16_t>(port);
    bool connected = conn->connected();
    peerObj["connected"].set<bool>(connected);
    uint32_t connectionState = (uint32_t)conn->connectionState();
    peerObj["connectionState"].set<uint32_t>(connectionState);
    uint32_t pingsReceived = conn->pingsReceived();
    peerObj["pingsReceived"].set<uint32_t>(pingsReceived);
    uint64_t lastPing = conn->lastPing();
    peerObj["lastPing"].set<uint64_t>(lastPing);
    uint32_t ccPeerId = conn->ccPeerId();
    peerObj["controlChannel"].set<uint32_t>(ccPeerId);

    json::object peerConfig = conn->config();
    if (peerConfig["rcon"].is<json::object>())
        peerConfig.erase("rcon");
    peerObj["config"].set<json::object>(peerConfig);

    json::array voiceChannels = json::array();
    auto it = std::find_if(m_ccPeerMap.begin(), m_ccPeerMap.end(), [&](auto x) { return x.first == peerId; });
    if (it != m_ccPeerMap.end()) {
        std::vector<uint32_t> vcPeers = m_ccPeerMap[peerId];
        for (uint32_t vcEntry : vcPeers) {
            voiceChannels.push_back(json::value((double)vcEntry));
        }
    }
    peerObj["voiceChannels"].set<json::array>(voiceChannels);

    return peerObj;
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

            erasePeer(peerId);
            delete connection;

            return true;
        }
    }

    LogWarning(LOG_NET, "PEER %u reset failed; peer not found.", peerId);
    return false;
}

/* Helper to resolve the peer ID to its identity string. */

std::string FNENetwork::resolvePeerIdentity(uint32_t peerId)
{
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

void FNENetwork::setupRepeaterLogin(uint32_t peerId, uint32_t streamId, FNEPeerConnection* connection)
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

    writePeerACK(peerId, streamId, salt, 4U);
    LogInfoEx(LOG_NET, "PEER %u RPTL ACK, challenge response sent for login", peerId);
}

/* Helper to send the ACL lists to the specified peer in a separate thread. */

void FNENetwork::peerACLUpdate(uint32_t peerId)
{
    ACLUpdateRequest* req = new ACLUpdateRequest();
    req->obj = this;
    req->peerId = peerId;

    // enqueue the task
    if (!m_threadPool.enqueue(new_pooltask(taskACLUpdate, req))) {
        LogError(LOG_NET, "Failed to task enqueue ACL update, peerId = %u", peerId);
        if (req != nullptr)
            delete req;
    }
}

/* Helper to send the ACL lists to the specified peer in a separate thread. */

void FNENetwork::taskACLUpdate(ACLUpdateRequest* req)
{
    if (req != nullptr) {
        FNENetwork* network = static_cast<FNENetwork*>(req->obj);
        if (network == nullptr) {
            if (req != nullptr)
                delete req;
            return;
        }

        if (req == nullptr)
            return;

        std::string peerIdentity = network->resolvePeerIdentity(req->peerId);

        FNEPeerConnection* connection = network->m_peers[req->peerId];
        if (connection != nullptr) {
            uint32_t aclStreamId = network->createStreamId();

            // if the connection is an external peer, and peer is participating in peer link,
            // send the peer proper configuration data
            if (connection->isExternalPeer() && connection->isPeerLink()) {
                LogInfoEx(LOG_NET, "PEER %u (%s) sending Peer-Link ACL list updates", req->peerId, peerIdentity.c_str());

                network->writeWhitelistRIDs(req->peerId, aclStreamId, true);
                network->writeTGIDs(req->peerId, aclStreamId, true);
                network->writePeerList(req->peerId, aclStreamId);
            }
            else {
                LogInfoEx(LOG_NET, "PEER %u (%s) sending ACL list updates", req->peerId, peerIdentity.c_str());

                network->writeWhitelistRIDs(req->peerId, aclStreamId, false);
                network->writeBlacklistRIDs(req->peerId, aclStreamId);
                network->writeTGIDs(req->peerId, aclStreamId, false);
                network->writeDeactiveTGIDs(req->peerId, aclStreamId);
            }
        }

        delete req;
    }
}

/* Helper to send the list of whitelisted RIDs to the specified peer. */

void FNENetwork::writeWhitelistRIDs(uint32_t peerId, uint32_t streamId, bool isExternalPeer)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // sending PEER_LINK style RID list to external peers
    if (isExternalPeer) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            std::string filename = m_ridLookup->filename();
            if (filename.empty()) {
                return;
            }

            // read entire file into string buffer
            std::stringstream b;
            std::ifstream stream(filename);
            if (stream.is_open()) {
                while (stream.peek() != EOF) {
                    b << (char)stream.get();
                }

                stream.close();
            }

            // convert to a byte array
            uint32_t len = b.str().size();
            UInt8Array __buffer = std::make_unique<uint8_t[]>(len);
            uint8_t* buffer = __buffer.get();
            ::memset(buffer, 0x00U, len);
            ::memcpy(buffer, b.str().data(), len);

            PacketBuffer pkt(true, "Peer-Link, RID List");
            pkt.encode((uint8_t*)buffer, len);

            LogInfoEx(LOG_NET, "PEER %u Peer-Link, RID List, blocks %u, streamId = %u", peerId, pkt.fragments.size(), streamId);
            if (pkt.fragments.size() > 0U) {
                for (auto frag : pkt.fragments) {
                    writePeer(peerId, { NET_FUNC::PEER_LINK, NET_SUBFUNC::PL_RID_LIST }, 
                        frag.second->data, FRAG_SIZE, 0U, streamId, false, true, true);
                    Thread::sleep(60U); // pace block transmission
                }
            }
        }

        return;
    }

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
            UInt8Array __payload = std::make_unique<uint8_t[]>(bufSize);
            uint8_t* payload = __payload.get();
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
                payload, bufSize, streamId, true);
        }

        connection->lastPing(now);
    }
}

/* Helper to send the list of whitelisted RIDs to the specified peer. */

void FNENetwork::writeBlacklistRIDs(uint32_t peerId, uint32_t streamId)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

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
            UInt8Array __payload = std::make_unique<uint8_t[]>(bufSize);
            uint8_t* payload = __payload.get();
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
                payload, bufSize, streamId, true);
        }

        connection->lastPing(now);
    }
}

/* Helper to send the list of active TGIDs to the specified peer. */

void FNENetwork::writeTGIDs(uint32_t peerId, uint32_t streamId, bool isExternalPeer)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (!m_tidLookup->sendTalkgroups()) {
        return;
    }

    // sending PEER_LINK style TGID list to external peers
    if (isExternalPeer) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            std::string filename = m_tidLookup->filename();
            if (filename.empty()) {
                return;
            }

            // read entire file into string buffer
            std::stringstream b;
            std::ifstream stream(filename);
            if (stream.is_open()) {
                while (stream.peek() != EOF) {
                    b << (char)stream.get();
                }

                stream.close();
            }

            // convert to a byte array
            uint32_t len = b.str().size();
            UInt8Array __buffer = std::make_unique<uint8_t[]>(len);
            uint8_t* buffer = __buffer.get();
            ::memset(buffer, 0x00U, len);
            ::memcpy(buffer, b.str().data(), len);

            PacketBuffer pkt(true, "Peer-Link, TGID List");
            pkt.encode((uint8_t*)buffer, len);

            LogInfoEx(LOG_NET, "PEER %u Peer-Link, TGID List, blocks %u, streamId = %u", peerId, pkt.fragments.size(), streamId);
            if (pkt.fragments.size() > 0U) {
                for (auto frag : pkt.fragments) {
                    writePeer(peerId, { NET_FUNC::PEER_LINK, NET_SUBFUNC::PL_TALKGROUP_LIST }, 
                        frag.second->data, FRAG_SIZE, 0U, streamId, false, true, true);
                    Thread::sleep(60U); // pace block transmission
                }
            }
        }

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

            // set the $80 bit of the slot number to flag non-preferred
            if (nonPreferred) {
                slotNo |= 0x80U;
            }

            // set the $40 bit of the slot number to identify if this TG is by affiliation or not
            if (entry.config().affiliated()) {
                slotNo |= 0x40U;
            }

            tgidList.push_back({ entry.source().tgId(), slotNo });
        }
    }

    // build dataset
    UInt8Array __payload = std::make_unique<uint8_t[]>(4U + (tgidList.size() * 5U));
    uint8_t* payload = __payload.get();
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
        payload, 4U + (tgidList.size() * 5U), streamId, true);
}

/* Helper to send the list of deactivated TGIDs to the specified peer. */

void FNENetwork::writeDeactiveTGIDs(uint32_t peerId, uint32_t streamId)
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
    UInt8Array __payload = std::make_unique<uint8_t[]>(4U + (tgidList.size() * 5U));
    uint8_t* payload = __payload.get();
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
        payload, 4U + (tgidList.size() * 5U), streamId, true);
}

/* Helper to send the list of peers to the specified peer. */

void FNENetwork::writePeerList(uint32_t peerId, uint32_t streamId)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // sending PEER_LINK style RID list to external peers
    FNEPeerConnection* connection = m_peers[peerId];
    if (connection != nullptr) {
        std::string filename = m_peerListLookup->filename();
        if (filename.empty()) {
            return;
        }

        // read entire file into string buffer
        std::stringstream b;
        std::ifstream stream(filename);
        if (stream.is_open()) {
            while (stream.peek() != EOF) {
                b << (char)stream.get();
            }

            stream.close();
        }

        // convert to a byte array
        uint32_t len = b.str().size();
        UInt8Array __buffer = std::make_unique<uint8_t[]>(len);
        uint8_t* buffer = __buffer.get();
        ::memset(buffer, 0x00U, len);
        ::memcpy(buffer, b.str().data(), len);

        PacketBuffer pkt(true, "Peer-Link, PID List");
        pkt.encode((uint8_t*)buffer, len);

        LogInfoEx(LOG_NET, "PEER %u Peer-Link, PID List, blocks %u, streamId = %u", peerId, pkt.fragments.size(), streamId);
        if (pkt.fragments.size() > 0U) {
            for (auto frag : pkt.fragments) {
                writePeer(peerId, { NET_FUNC::PEER_LINK, NET_SUBFUNC::PL_PEER_LIST }, 
                    frag.second->data, FRAG_SIZE, 0U, streamId, false, true, true);
                Thread::sleep(60U); // pace block transmission
            }
        }
    }

    return;
}

/* Helper to send a In-Call Control command to the specified peer. */

bool FNENetwork::writePeerICC(uint32_t peerId, uint32_t streamId, NET_SUBFUNC::ENUM subFunc, NET_ICC::ENUM command, uint32_t dstId, uint8_t slotNo)
{
    if (peerId == 0)
        return false;
    if (!m_enableInCallCtrl)
        return false;
    if (dstId == 0U)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID
    buffer[10U] = (uint8_t)command;                                             // In-Call Control Command
    __SET_UINT16(dstId, buffer, 11U);                                           // Destination ID
    buffer[14U] = slotNo;                                                       // DMR Slot No

    return writePeer(peerId, { NET_FUNC::INCALL_CTRL, subFunc }, buffer, 15U, RTP_END_OF_CALL_SEQ, streamId, false);
}

/* Helper to send a data message to the specified peer with a explicit packet sequence. */

bool FNENetwork::writePeer(uint32_t peerId, FrameQueue::OpcodePair opcode, const uint8_t* data,
    uint32_t length, uint16_t pktSeq, uint32_t streamId, bool queueOnly, bool incPktSeq, bool directWrite) const
{
    if (streamId == 0U) {
        LogError(LOG_NET, "BUGBUG: PEER %u, trying to send data with a streamId of 0?", peerId);
    }

    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        FNEPeerConnection* connection = m_peers.at(peerId);
        if (connection != nullptr) {
            sockaddr_storage addr = connection->socketStorage();
            uint32_t addrLen = connection->sockStorageLen();

            if (incPktSeq) {
                pktSeq = connection->incStreamPktSeq(streamId, pktSeq);
            }

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

/* Helper to send a command message to the specified peer. */

bool FNENetwork::writePeerCommand(uint32_t peerId, FrameQueue::OpcodePair opcode,
    const uint8_t* data, uint32_t length, uint32_t streamId, bool incPktSeq) const
{
    if (peerId == 0)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + 6U, data, length);
    }

    uint32_t len = length + 6U;
    return writePeer(peerId, opcode, buffer, len, RTP_END_OF_CALL_SEQ, streamId, false, incPktSeq, true);
}

/* Helper to send a ACK response to the specified peer. */

bool FNENetwork::writePeerACK(uint32_t peerId, uint32_t streamId, const uint8_t* data, uint32_t length)
{
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    __SET_UINT32(peerId, buffer, 0U);                                           // Peer ID

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + 6U, data, length);
    }

    return writePeer(peerId, { NET_FUNC::ACK, NET_SUBFUNC::NOP }, buffer, length + 10U, RTP_END_OF_CALL_SEQ, streamId, 
        false, false, true);
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

bool FNENetwork::writePeerNAK(uint32_t peerId, uint32_t streamId, const char* tag, NET_CONN_NAK_REASON reason)
{
    if (peerId == 0)
        return false;
    if (tag == nullptr)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID
    __SET_UINT16B((uint16_t)reason, buffer, 10U);                               // Reason

    logPeerNAKReason(peerId, tag, reason);
    return writePeer(peerId, { NET_FUNC::NAK, NET_SUBFUNC::NOP }, buffer, 10U, RTP_END_OF_CALL_SEQ, streamId, false);
}

/* Helper to send a NAK response to the specified peer. */

bool FNENetwork::writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason, sockaddr_storage& addr, uint32_t addrLen)
{
    if (peerId == 0)
        return false;
    if (tag == nullptr)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    __SET_UINT32(peerId, buffer, 6U);                                           // Peer ID
    __SET_UINT16B((uint16_t)reason, buffer, 10U);                               // Reason

    logPeerNAKReason(peerId, tag, reason);
    LogWarning(LOG_NET, "PEER %u NAK %s -> %s:%u", peerId, tag, udp::Socket::address(addr).c_str(), udp::Socket::port(addr));
    return m_frameQueue->write(buffer, 12U, createStreamId(), peerId, m_peerId,
        { NET_FUNC::NAK, NET_SUBFUNC::NOP }, 0U, addr, addrLen);
}

/* Helper to process a FNE KMM TEK response. */

void FNENetwork::processTEKResponse(p25::kmm::KeyItem* rspKi, uint8_t algId, uint8_t keyLength)
{
    using namespace p25::defines;
    using namespace p25::kmm;

    if (rspKi == nullptr)
        return;

    LogMessage(LOG_NET, "upstream master enc. key, algId = $%02X, kID = $%04X", algId, rspKi->kId());

    m_keyQueueMutex.lock();

    std::vector<uint32_t> peersToRemove;
    for (auto entry : m_peerLinkKeyQueue) {
        uint16_t keyId = entry.second;
        if (keyId == rspKi->kId() && algId > 0U) {
            uint32_t peerId = entry.first;

            uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
            ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
            rspKi->getKey(key);

            if (m_debug) {
                LogDebugEx(LOG_HOST, "FNENetwork::processTEKResponse()", "keyLength = %u", keyLength);
                Utils::dump(1U, "Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
            }

            // build response buffer
            uint8_t buffer[DATA_PACKET_LENGTH];
            ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

            KMMModifyKey modifyKeyRsp = KMMModifyKey();
            modifyKeyRsp.setDecryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE);
            modifyKeyRsp.setAlgId(algId);
            modifyKeyRsp.setKId(0U);

            KeysetItem ks = KeysetItem();
            ks.keysetId(1U);
            ks.algId(algId);
            ks.keyLength(keyLength);

            p25::kmm::KeyItem ki = p25::kmm::KeyItem();
            ki.keyFormat(KEY_FORMAT_TEK);
            ki.kId(rspKi->kId());
            ki.sln(rspKi->sln());
            ki.setKey(key, keyLength);

            ks.push_back(ki);
            modifyKeyRsp.setKeysetItem(ks);

            modifyKeyRsp.encode(buffer + 11U);

            writePeer(peerId, { NET_FUNC::KEY_RSP, NET_SUBFUNC::NOP }, buffer, modifyKeyRsp.length() + 11U, 
                RTP_END_OF_CALL_SEQ, createStreamId(), false, false, true);

            peersToRemove.push_back(peerId);
        }
    }

    // remove peers who were sent keys
    for (auto peerId : peersToRemove)
        m_peerLinkKeyQueue.erase(peerId);

    m_keyQueueMutex.unlock();
}
