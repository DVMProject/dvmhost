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
#include "common/p25/kmm/KMMFactory.h"
#include "common/json/json.h"
#include "common/zlib/Compression.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Utils.h"
#include "network/TrafficNetwork.h"
#include "network/callhandler/TagDMRData.h"
#include "network/callhandler/TagP25Data.h"
#include "network/callhandler/TagNXDNData.h"
#include "network/callhandler/TagAnalogData.h"
#include "network/P25OTARService.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"
#include "FNEMain.h"

using namespace network;
using namespace network::callhandler;
using namespace compress;

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <streambuf>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t MAX_HARD_CONN_CAP = 250U;
const uint8_t MAX_PEER_LIST_BEFORE_FLUSH = 10U;
const uint32_t MAX_RID_LIST_CHUNK = 50U;

const uint32_t MAX_MISSED_ACL_UPDATES = 10U;

const uint64_t PACKET_LATE_TIME = 250U; // 250ms

const uint32_t FIXED_HA_UPDATE_INTERVAL = 30U; // 30s

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::timed_mutex TrafficNetwork::s_keyQueueMutex;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TrafficNetwork class. */

TrafficNetwork::TrafficNetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
    std::string identity, bool debug, bool kmfDebug, bool verbose, bool reportPeerPing,
    bool dmr, bool p25, bool nxdn, bool analog,
    uint32_t parrotDelay, bool parrotGrantDemand, bool allowActivityTransfer, bool allowDiagnosticTransfer, 
    uint32_t pingTime, uint32_t updateLookupTime, uint16_t workerCnt) :
    BaseNetwork(peerId, true, debug, true, true, allowActivityTransfer, allowDiagnosticTransfer),
    m_tagDMR(nullptr),
    m_tagP25(nullptr),
    m_tagNXDN(nullptr),
    m_tagAnalog(nullptr),
    m_p25OTARService(nullptr),
    m_host(host),
    m_address(address),
    m_port(port),
    m_password(password),
    m_isReplica(false),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_nxdnEnabled(nxdn),
    m_analogEnabled(analog),
    m_parrotDelay(parrotDelay),
    m_parrotDelayTimer(1000U, 0U, parrotDelay),
    m_parrotGrantDemand(parrotGrantDemand),
    m_parrotOnlyOriginating(false),
    m_parrotOverrideSrcId(0U),
    m_kmfServicesEnabled(false),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_peerListLookup(nullptr),
    m_adjSiteMapLookup(nullptr),
    m_cryptoLookup(nullptr),
    m_status(NET_STAT_INVALID),
    m_peers(),
    m_peerReplicaPeers(),
    m_peerAffiliations(),
    m_ccPeerMap(),
    m_peerReplicaKeyQueue(),
    m_treeRoot(nullptr),
    m_treeLock(),
    m_peerReplicaHAParams(),
    m_advertisedHAAddress(),
    m_advertisedHAPort(TRAFFIC_DEFAULT_PORT),
    m_haEnabled(false),
    m_maintainenceTimer(1000U, pingTime),
    m_updateLookupTimer(1000U, (updateLookupTime * 60U)),
    m_haUpdateTimer(1000U, FIXED_HA_UPDATE_INTERVAL),
    m_softConnLimit(0U),
    m_enableSpanningTree(true),
    m_logSpanningTreeChanges(false),
    m_spanningTreeFastReconnect(true),
    m_callCollisionTimeout(5U),
    m_disallowAdjStsBcast(false),
    m_disallowExtAdjStsBcast(true),
    m_allowConvSiteAffOverride(false),
    m_disallowCallTerm(false),
    m_restrictGrantToAffOnly(false),
    m_restrictPVCallToRegOnly(false),
    m_enableRIDInCallCtrl(true),
    m_disallowInCallCtrl(false),
    m_rejectUnknownRID(false),
    m_maskOutboundPeerID(false),
    m_maskOutboundPeerIDForNonPL(false),
    m_filterTerminators(true),
    m_forceListUpdate(false),
    m_disallowU2U(false),
    m_dropU2UPeerTable(),
    m_enableInfluxDB(false),
    m_influxServerAddress("127.0.0.1"),
    m_influxServerPort(8086U),
    m_influxServerToken(),
    m_influxOrg("dvm"),
    m_influxBucket("dvm"),
    m_influxLogRawData(false),
    m_jitterBufferEnabled(false),
    m_jitterMaxSize(4U),
    m_jitterMaxWait(40000U),
    m_threadPool(workerCnt, "fne"),
    m_disablePacketData(false),
    m_dumpPacketData(false),
    m_verbosePacketData(false),
    m_sndcpStartAddr(__IP_FROM_STR("10.10.1.10")),
    m_sndcpEndAddr(__IP_FROM_STR("10.10.1.254")),
    m_totalActiveCalls(0U),
    m_totalCallsProcessed(0U),
    m_totalCallCollisions(0U),
    m_logDenials(false),
    m_logUpstreamCallStartEnd(true),
    m_reportPeerPing(reportPeerPing),
    m_verbose(verbose)
{
    assert(host != nullptr);
    assert(!address.empty());
    assert(port > 0U);
    assert(!password.empty());

    m_peers.reserve(MAX_HARD_CONN_CAP);
    m_peerReplicaPeers.reserve(MAX_HARD_CONN_CAP);
    m_peerAffiliations.reserve(MAX_HARD_CONN_CAP);
    m_ccPeerMap.reserve(MAX_HARD_CONN_CAP);

    m_tagDMR = new TagDMRData(this, debug);
    m_tagP25 = new TagP25Data(this, debug);
    m_tagNXDN = new TagNXDNData(this, debug);
    m_tagAnalog = new TagAnalogData(this, debug);

    m_p25OTARService = new P25OTARService(this, m_tagP25->packetData(), kmfDebug, verbose);

    SpanningTree::s_maxUpdatesBeforeReparent = (uint8_t)host->m_maxMissedPings;
    m_treeRoot = new SpanningTree(peerId, peerId, nullptr);
    m_treeRoot->identity(identity);

    /*
    ** Initialize Threads
    */

    Thread::runAsThread(this, threadParrotHandler);
}

/* Finalizes a instance of the TrafficNetwork class. */

TrafficNetwork::~TrafficNetwork()
{
    if (m_kmfServicesEnabled) {
        m_p25OTARService->close();
    }

    delete m_p25OTARService;

    delete m_tagDMR;
    delete m_tagP25;
    delete m_tagNXDN;
    delete m_tagAnalog;
}

/* Helper to set configuration options. */

void TrafficNetwork::setOptions(yaml::Node& conf, bool printOptions)
{
    m_disallowAdjStsBcast = conf["disallowAdjStsBcast"].as<bool>(false);
    m_disallowExtAdjStsBcast = conf["disallowExtAdjStsBcast"].as<bool>(true);
    m_allowConvSiteAffOverride = conf["allowConvSiteAffOverride"].as<bool>(true);
    m_enableRIDInCallCtrl = conf["enableRIDInCallCtrl"].as<bool>(false);
    m_disallowInCallCtrl = conf["disallowInCallCtrl"].as<bool>(false);
    m_rejectUnknownRID = conf["rejectUnknownRID"].as<bool>(false);
    m_maskOutboundPeerID = conf["maskOutboundPeerID"].as<bool>(false);
    m_maskOutboundPeerIDForNonPL = conf["maskOutboundPeerIDForNonPeerLink"].as<bool>(false);
    m_disallowCallTerm = conf["disallowCallTerm"].as<bool>(false);
    m_softConnLimit = conf["connectionLimit"].as<uint32_t>(MAX_HARD_CONN_CAP);

    if (m_softConnLimit > MAX_HARD_CONN_CAP) {
        m_softConnLimit = MAX_HARD_CONN_CAP;
    }

    m_enableSpanningTree = conf["enableSpanningTree"].as<bool>(true);

    if (!m_enableSpanningTree) {
        LogWarning(LOG_MASTER, "WARNING: Disabling the peer spanning tree is not recommended! This can cause network loops and other issues in a multi-peer FNE network.");
    }

    m_logSpanningTreeChanges = conf["logSpanningTreeChanges"].as<bool>(false);
    m_spanningTreeFastReconnect = conf["spanningTreeFastReconnect"].as<bool>(true);

    // always force disable ADJ_STS_BCAST to neighbor FNE peers if the all option
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
    m_parrotOverrideSrcId = conf["parrotOverrideSrcId"].as<uint32_t>(0U);
    if (m_parrotOverrideSrcId > 0U && m_parrotOverrideSrcId > 16777200U) {
        LogWarning(LOG_MASTER, "Parrot Override Source ID %u is out of valid range (1 - 16777200), disabling override.", m_parrotOverrideSrcId);
        m_parrotOverrideSrcId = 0U;
    }

    // jitter buffer configuration
    yaml::Node jitterConf = conf["jitterBuffer"];
    m_jitterBufferEnabled = jitterConf["enabled"].as<bool>(false);
    m_jitterMaxSize = (uint16_t)jitterConf["defaultMaxSize"].as<uint32_t>(DEFAULT_JITTER_MAX_SIZE);
    m_jitterMaxWait = jitterConf["defaultMaxWait"].as<uint32_t>(DEFAULT_JITTER_MAX_WAIT);

    // clamp jitter buffer parameters
    if (m_jitterMaxSize < MIN_JITTER_MAX_SIZE)
        m_jitterMaxSize = MIN_JITTER_MAX_SIZE;
    if (m_jitterMaxSize > MAX_JITTER_MAX_SIZE)
        m_jitterMaxSize = MAX_JITTER_MAX_SIZE;
    if (m_jitterMaxWait < MIN_JITTER_MAX_WAIT)
        m_jitterMaxWait = MIN_JITTER_MAX_WAIT;
    if (m_jitterMaxWait > MAX_JITTER_MAX_WAIT)
        m_jitterMaxWait = MAX_JITTER_MAX_WAIT;

#if defined(ENABLE_SSL)
    m_kmfServicesEnabled = conf["kmfServicesEnabled"].as<bool>(false);
    uint16_t kmfOtarPort = conf["kmfOtarPort"].as<uint16_t>(64414U);
    if (m_kmfServicesEnabled) {
        if (!m_p25OTARService->open(m_address, kmfOtarPort)) {
            m_kmfServicesEnabled = false;
            LogError(LOG_MASTER, "FNE OTAR KMF services failed to start, OTAR service disabled.");
        }
    }
#else
    uint16_t kmfOtarPort = 64414U; // hardcoded
    m_kmfServicesEnabled = false;
    LogWarning(LOG_MASTER, "FNE is compiled without OpenSSL support, KMF services are unavailable.");
#endif // ENABLE_SSL

    m_callCollisionTimeout = conf["callCollisionTimeout"].as<uint32_t>(5U);

    m_restrictGrantToAffOnly = conf["restrictGrantToAffiliatedOnly"].as<bool>(false);
    m_restrictPVCallToRegOnly = conf["restrictPrivateCallToRegOnly"].as<bool>(false);
    m_filterTerminators = conf["filterTerminators"].as<bool>(true);

    m_disablePacketData = conf["disablePacketData"].as<bool>(false);
    m_dumpPacketData = conf["dumpPacketData"].as<bool>(false);
    m_verbosePacketData = conf["verbosePacketData"].as<bool>(false);

    // SNDCP IP allocation configuration
    m_sndcpStartAddr = __IP_FROM_STR("10.10.1.10");
    m_sndcpEndAddr = __IP_FROM_STR("10.10.1.254");
    yaml::Node& vtun = conf["vtun"];
    if (vtun.size() > 0U) {
        yaml::Node& sndcp = vtun["sndcp"];
        if (sndcp.size() > 0U) {
            std::string startAddrStr = sndcp["startAddress"].as<std::string>("10.10.1.10");
            std::string endAddrStr = sndcp["endAddress"].as<std::string>("10.10.1.254");
            m_sndcpStartAddr = __IP_FROM_STR(startAddrStr);
            m_sndcpEndAddr = __IP_FROM_STR(endAddrStr);

            if (m_sndcpStartAddr > m_sndcpEndAddr) {
                LogWarning(LOG_MASTER, "SNDCP start address (%s) is greater than end address (%s), using defaults", 
                    startAddrStr.c_str(), endAddrStr.c_str());
                m_sndcpStartAddr = __IP_FROM_STR("10.10.1.10");
                m_sndcpEndAddr = __IP_FROM_STR("10.10.1.254");
            }
        }
    }

    m_logDenials = conf["logDenials"].as<bool>(false);
    m_logUpstreamCallStartEnd = conf["logUpstreamCallStartEnd"].as<bool>(true);

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

    yaml::Node& haParams = conf["ha"];
    m_advertisedHAAddress = haParams["advertisedWANAddress"].as<std::string>();
    m_advertisedHAPort = (uint16_t)haParams["advertisedWANPort"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
    m_haEnabled = haParams["enable"].as<bool>(false);

    if (m_haEnabled) {
        uint32_t ipAddr = __IP_FROM_STR(m_advertisedHAAddress);
        HAParameters params = HAParameters(m_peerId, ipAddr, m_advertisedHAPort);
        m_peerReplicaHAParams.push_back(params);
    }

    if (printOptions) {
        LogInfo("    Maximum Permitted Connections: %u", m_softConnLimit);
        LogInfo("    Enable Peer Spanning Tree: %s", m_enableSpanningTree ? "yes" : "no");
        LogInfo("    Log Spanning Tree Changes: %s", m_logSpanningTreeChanges ? "yes" : "no");
        LogInfo("    Spanning Tree Allow Fast Reconnect: %s", m_spanningTreeFastReconnect ? "yes" : "no");
        LogInfo("    Disable adjacent site broadcasts to any peers: %s", m_disallowAdjStsBcast ? "yes" : "no");
        if (m_disallowAdjStsBcast) {
            LogWarning(LOG_MASTER, "NOTICE: All P25 ADJ_STS_BCAST messages will be blocked and dropped!");
        }
        LogInfo("    Disable Packet Data: %s", m_disablePacketData ? "yes" : "no");
        LogInfo("    Dump Packet Data: %s", m_dumpPacketData ? "yes" : "no");
        LogInfo("    Disable P25 ADJ_STS_BCAST to neighbor peers: %s", m_disallowExtAdjStsBcast ? "yes" : "no");
        LogInfo("    Disable P25 TDULC call termination broadcasts to any peers: %s", m_disallowCallTerm ? "yes" : "no");
        LogInfo("    Allow conventional sites to override affiliation and receive all traffic: %s", m_allowConvSiteAffOverride ? "yes" : "no");
        LogInfo("    Enable RID In-Call Control: %s", m_enableRIDInCallCtrl ? "yes" : "no");
        LogInfo("    Disallow In-Call Control Requests: %s", m_disallowInCallCtrl ? "yes" : "no");
        LogInfo("    Reject Unknown RIDs: %s", m_rejectUnknownRID ? "yes" : "no");
        LogInfo("    Log Traffic Denials: %s", m_logDenials ? "yes" : "no");
        LogInfo("    Log Upstream Call Start/End Events: %s", m_logUpstreamCallStartEnd ? "yes" : "no");
        LogInfo("    Mask Outbound Traffic Peer ID: %s", m_maskOutboundPeerID ? "yes" : "no");
        if (m_maskOutboundPeerIDForNonPL) {
            LogInfo("    Mask Outbound Traffic Peer ID for Non-Peer Link: yes");
        }
        LogInfo("    Call Collision Timeout: %us", m_callCollisionTimeout);
        if (m_callCollisionTimeout == 0U) {
            LogWarning(LOG_MASTER, "Call Collisions are disabled because the call collision timeout is set to 0 seconds. This is not recommended, and can cause undesired behavior.");
        }
        LogInfo("    Restrict grant response by affiliation: %s", m_restrictGrantToAffOnly ? "yes" : "no");
        LogInfo("    Restrict private call to registered units: %s", m_restrictPVCallToRegOnly ? "yes" : "no");
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
        LogInfo("    Global Jitter Buffer Enabled: %s", m_jitterBufferEnabled ? "yes" : "no");
        if (m_jitterBufferEnabled) {
            LogInfo("    Global Jitter Buffer Default Max Size: %u frames", m_jitterMaxSize);
            LogInfo("    Global Jitter Buffer Default Max Wait: %u microseconds", m_jitterMaxWait);
        }
        LogInfo("    Parrot Repeat to Only Originating Peer: %s", m_parrotOnlyOriginating ? "yes" : "no");
        if (m_parrotOverrideSrcId != 0U) {
            LogInfo("    Parrot Repeat Source ID Override: %u", m_parrotOverrideSrcId);
        }
        LogInfo("    P25 OTAR KMF Services Enabled: %s", m_kmfServicesEnabled ? "yes" : "no");
        LogInfo("    P25 OTAR KMF Listening Address: %s", m_address.c_str());
        LogInfo("    P25 OTAR KMF Listening Port: %u", kmfOtarPort);
        LogInfo("    High Availability Enabled: %s", m_haEnabled ? "yes" : "no");
        if (m_haEnabled) {
            LogInfo("    Advertised HA WAN IP: %s", m_advertisedHAAddress.c_str());
            LogInfo("    Advertised HA WAN Port: %u", m_advertisedHAPort);
        }
    }
}

/* Sets the instances of the Radio ID, Talkgroup ID Peer List, and Crypto lookup tables. */

void TrafficNetwork::setLookups(lookups::RadioIdLookup* ridLookup, lookups::TalkgroupRulesLookup* tidLookup, lookups::PeerListLookup* peerListLookup,
    CryptoContainer* cryptoLookup, lookups::AdjSiteMapLookup* adjSiteMapLookup)
{
    m_ridLookup = ridLookup;
    m_tidLookup = tidLookup;
    m_peerListLookup = peerListLookup;
    m_cryptoLookup = cryptoLookup;
    m_adjSiteMapLookup = adjSiteMapLookup;
}

/* Sets endpoint preshared encryption key. */

void TrafficNetwork::setPresharedKey(const uint8_t* presharedKey)
{
    m_socket->setPresharedKey(presharedKey);
}

/* Process a data frames from the network. */

void TrafficNetwork::processNetwork()
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
            Utils::dump(1U, "TrafficNetwork::processNetwork(), Network Message", buffer.get(), length);

        uint32_t peerId = fneHeader.getPeerId();

        NetPacketRequest* req = new NetPacketRequest();
        req->obj = this;
        req->metadataObj = m_host->m_mdNetwork;
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

/* Process network tree disconnect notification. */

void TrafficNetwork::processNetworkTreeDisconnect(uint32_t peerId, uint32_t offendingPeerId)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }

    if (!m_enableSpanningTree) {
        LogWarning(LOG_STP, "TrafficNetwork::processNetworkTreeDisconnect(), ignoring disconnect request for PEER %u, spanning tree is disabled", offendingPeerId);
        return;
    }

    if (offendingPeerId > 0 && (m_peers.find(offendingPeerId) != m_peers.end())) {
        FNEPeerConnection* connection = m_peers[offendingPeerId];
        if (connection != nullptr) {
            LogWarning(LOG_STP, "PEER %u (%s) NAK, server already connected via upstream master, duplicate connection dropped, connectionState = %u", offendingPeerId, connection->identWithQualifier().c_str(),
                connection->connectionState());
            writePeerNAK(offendingPeerId, createStreamId(), TAG_REPEATER_CONFIG, NET_CONN_NAK_FNE_DUPLICATE_CONN);
            disconnectPeer(offendingPeerId, connection);
            logSpanningTree();
        } else {
            LogError(LOG_STP, "Network Tree Disconnect, upstream master requested disconnect for PEER %u, but connection is null", offendingPeerId);
        }
    } else {
        // is this perhaps a peer connection of ours?
        if (m_host->m_peerNetworks.size() > 0) {
            for (auto& peer : m_host->m_peerNetworks) {
                if (peer.second != nullptr) {
                    if (peer.second->getPeerId() == peerId) {
                        LogWarning(LOG_STP, "PEER %u, upstream master requested disconnect for our peer connection, duplicate connection dropped", peerId);
                        peer.second->close();
                        return;
                    }
                }
            }
        }

        LogError(LOG_STP, "Network Tree Disconnect, upstream master requested disconnect for unknown PEER %u", offendingPeerId);
    }
}

/* Helper to process an downstream peer In-Call Control message. */

void TrafficNetwork::processDownstreamInCallCtrl(network::NET_ICC::ENUM command, network::NET_SUBFUNC::ENUM subFunc, uint32_t dstId, 
    uint8_t slotNo, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    if (m_disallowInCallCtrl)
        return;

    processInCallCtrl(command, subFunc, dstId, slotNo, peerId, ssrc, streamId);
}

/* Updates the timer by the passed number of milliseconds. */

void TrafficNetwork::clock(uint32_t ms)
{
    if (m_status != NET_STAT_MST_RUNNING) {
        return;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // check jitter buffer timeouts for all peers
    m_peers.shared_lock();
    for (auto& peer : m_peers) {
        FNEPeerConnection* connection = peer.second;
        if (connection != nullptr && connection->jitterBufferEnabled()) {
            connection->checkJitterTimeouts();
        }
    }
    m_peers.unlock();

    if (m_forceListUpdate) {
        for (auto& peer : m_peers) {
            peerMetadataUpdate(peer.first);
        }
        m_forceListUpdate = false;
    }

    m_maintainenceTimer.clock(ms);
    if (m_maintainenceTimer.isRunning() && m_maintainenceTimer.hasExpired()) {
        // check to see if any peers have been quiet (no ping) longer than allowed
        std::vector<uint32_t> peersToRemove = std::vector<uint32_t>();
        m_peers.shared_lock();
        for (auto& peer : m_peers) {
            uint32_t id = peer.first;
            FNEPeerConnection* connection = peer.second;
            if (connection != nullptr) {
                uint64_t dt = 0U;
                if (connection->isNeighborFNEPeer() || connection->isReplica())
                    dt = connection->lastPing() + ((m_host->m_pingTime * 1000) * (m_host->m_maxMissedPings * 2U));
                else
                    dt = connection->lastPing() + ((m_host->m_pingTime * 1000) * m_host->m_maxMissedPings);

                if (dt < now) {
                    LogInfoEx(LOG_MASTER, "PEER %u (%s) timed out, dt = %u, now = %u", id, connection->identWithQualifier().c_str(),
                        dt, now);

                    // set connection states for this stale connection
                    connection->connected(false);
                    connection->connectionState(NET_STAT_INVALID);

                    peersToRemove.push_back(id);
                }
            }
        }
        m_peers.shared_unlock();

        // remove any peers
        for (uint32_t peerId : peersToRemove) {
            FNEPeerConnection* connection = m_peers[peerId];
            disconnectPeer(peerId, connection);
        }

        // send peer updates to neighbor FNE peers
        if (m_host->m_peerNetworks.size() > 0) {
            for (auto& peer : m_host->m_peerNetworks) {
                if (peer.second != nullptr) {
                    // perform master tree maintainence tasks
                    if (peer.second->isEnabled() && peer.second->getRemotePeerId() > 0U &&
                        m_enableSpanningTree) {
                        std::lock_guard<std::mutex> guard(m_treeLock);
                        peer.second->writeSpanningTree(m_treeRoot);
                    }

                    // perform peer replica maintainence tasks
                    if (peer.second->isEnabled() && peer.second->getRemotePeerId() > 0U &&
                        peer.second->isReplica()) {
                        if (!peer.second->getAttachedKeyRSPHandler()) {
                            peer.second->setAttachedKeyRSPHandler(true); // this is the only place this should happen
                            peer.second->setKeyResponseCallback([=](p25::kmm::KeyItem ki, uint8_t algId, uint8_t keyLength) {
                                processTEKResponse(&ki, algId, keyLength);
                            });
                        }

                        if (m_peers.size() > 0) {
                            json::array peers = json::array();
                            m_peers.shared_lock();
                            for (auto entry : m_peers) {
                                uint32_t peerId = entry.first;
                                network::FNEPeerConnection* connection = entry.second;
                                if (connection != nullptr) {
                                    json::object peerObj = fneConnObject(peerId, connection);
                                    uint32_t peerNetPeerId = peer.second->getPeerId();
                                    peerObj["parentPeerId"].set<uint32_t>(peerNetPeerId);
                                    peers.push_back(json::value(peerObj));
                                }
                            }
                            m_peers.shared_unlock();

                            peer.second->writePeerLinkPeers(&peers);
                        }
                    }
                }
            }
        }

        // cleanup possibly stale data calls
        m_tagDMR->packetData()->cleanupStale();
        m_tagP25->packetData()->cleanupStale();

        m_totalActiveCalls = 0U; // bryanb: this is techincally incorrect and should be better implemented
                                 // but for now it will suffice to reset the active call count on maintainence cycle

        m_maintainenceTimer.start();
    }

    m_updateLookupTimer.clock(ms);
    if (m_updateLookupTimer.isRunning() && m_updateLookupTimer.hasExpired()) {
        // send network metadata updates to peers
        m_peers.shared_lock();
        for (auto& peer : m_peers) {
            uint32_t id = peer.first;
            FNEPeerConnection* connection = peer.second;
            if (connection != nullptr) {
                // if this connection is a peer replica *always* send the update -- no stream checking
                if (connection->connected() && connection->isReplica()) {
                    LogInfoEx(LOG_MASTER, "PEER %u (%s), Peer Replication, updating network metadata", id, connection->identWithQualifier().c_str());

                    peerMetadataUpdate(id);
                    connection->missedMetadataUpdates(0U);
                    continue;
                }

                if (connection->connected()) {
                    if ((connection->streamCount() <= 1) || (connection->missedMetadataUpdates() > MAX_MISSED_ACL_UPDATES)) {
                        LogInfoEx(LOG_MASTER, "PEER %u (%s) updating ACL list", id, connection->identWithQualifier().c_str());
                        peerMetadataUpdate(id);
                        connection->missedMetadataUpdates(0U);
                    } else {
                        uint32_t missed = connection->missedMetadataUpdates();
                        missed++;

                        LogInfoEx(LOG_MASTER, "PEER %u (%s) skipped for metadata update, traffic in progress", id, connection->identWithQualifier().c_str());
                        connection->missedMetadataUpdates(missed);
                    }
                }
            }
        }
        m_peers.shared_unlock();

        m_updateLookupTimer.start();
    }

    // if HA is enabled perform HA parameter updates
    if (m_haEnabled) {
        m_haUpdateTimer.clock(ms);
        if (m_haUpdateTimer.isRunning() && m_haUpdateTimer.hasExpired()) {
            // send peer updates to replica peers
            if (m_host->m_peerNetworks.size() > 0) {
                for (auto& peer : m_host->m_peerNetworks) {
                    if (peer.second != nullptr) {
                        if (peer.second->isEnabled() && peer.second->isReplica()) {
                            std::vector<HAParameters> haParams;
                            m_peerReplicaHAParams.lock(false);
                            for (auto entry : m_peerReplicaHAParams) {
                                haParams.push_back(entry);
                            }
                            m_peerReplicaHAParams.unlock();

                            peer.second->writeHAParams(haParams);
                        }
                    }
                }
            }

            m_haUpdateTimer.start();
        }
    }

    if (m_kmfServicesEnabled)
        m_p25OTARService->clock(ms);
}

/* Opens connection to the network. */

bool TrafficNetwork::open()
{
    if (m_debug)
        LogInfoEx(LOG_MASTER, "Opening Network");

    // start thread pool
    m_threadPool.start();

    // start FluxQL thread pool
    if (m_enableInfluxDB) {
        influxdb::detail::TSCaller::start();
    }

    m_status = NET_STAT_MST_RUNNING;
    m_maintainenceTimer.start();
    m_updateLookupTimer.start();
    
    if (m_haEnabled) {
        m_haUpdateTimer.start();
    }

    m_socket = new udp::Socket(m_address, m_port);

    // reinitialize the frame queue
    if (m_frameQueue != nullptr) {
        delete m_frameQueue;
        m_frameQueue = new FrameQueue(m_socket, m_peerId, m_debug);
    }

    bool ret = m_socket->open();
    if (!ret) {
        m_socket->recvBufSize(524288U); // 512K recv buffer
        m_socket->sendBufSize(524288U); // 512K send buffer
        m_status = NET_STAT_INVALID;
    }

    return ret;
}

/* Closes connection to the network. */

void TrafficNetwork::close()
{
    if (m_debug)
        LogInfoEx(LOG_MASTER, "Closing Network");

    if (m_status == NET_STAT_MST_RUNNING) {
        uint8_t buffer[1U];
        ::memset(buffer, 0x00U, 1U);

        uint32_t streamId = createStreamId();
        for (auto& peer : m_peers) {
            writePeer(peer.first, m_peerId, { NET_FUNC::MST_DISC, NET_SUBFUNC::NOP }, buffer, 1U, RTP_END_OF_CALL_SEQ, 
                streamId);
        }
    }

    m_maintainenceTimer.stop();
    m_updateLookupTimer.stop();

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

/* Entry point to parrot handler thread. */

void* TrafficNetwork::threadParrotHandler(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("fne:parrot");
        TrafficNetwork* fne = static_cast<TrafficNetwork*>(th->obj);
        if (fne == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogInfoEx(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        if (fne != nullptr) {
            while (!g_killed) {
                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();

                fne->m_parrotDelayTimer.clock(ms);
                if (fne->m_parrotDelayTimer.isRunning() && fne->m_parrotDelayTimer.hasExpired()) {
                    // if the DMR handler has parrot frames to playback, playback a frame
                    if (fne->m_tagDMR->hasParrotFrames()) {
                        fne->m_tagDMR->playbackParrot();
                    }

                    // if the P25 handler has parrot frames to playback, playback a frame
                    if (fne->m_tagP25->hasParrotFrames()) {
                        fne->m_tagP25->playbackParrot();
                    }

                    // if the NXDN handler has parrot frames to playback, playback a frame
                    if (fne->m_tagNXDN->hasParrotFrames()) {
                        fne->m_tagNXDN->playbackParrot();
                    }

                    // if the analog handler has parrot frames to playback, playback a frame
                    if (fne->m_tagAnalog->hasParrotFrames()) {
                        fne->m_tagAnalog->playbackParrot();
                    }
                }

                if (!fne->m_tagDMR->hasParrotFrames() && !fne->m_tagP25->hasParrotFrames() && !fne->m_tagNXDN->hasParrotFrames() && !fne->m_tagAnalog->hasParrotFrames() &&
                    fne->m_parrotDelayTimer.isRunning() && fne->m_parrotDelayTimer.hasExpired()) {
                    fne->m_parrotDelayTimer.stop();
                }

                if (!fne->m_parrotDelayTimer.isRunning()) {
                    // if the DMR handle is marked as playing back parrot frames, but has no more frames in the queue
                    // clear the playback flag
                    if (fne->m_tagDMR->isParrotPlayback() && !fne->m_tagDMR->hasParrotFrames()) {
                        LogInfoEx(LOG_MASTER, "DMR, Parrot Call End, peer = %u, srcId = %u, dstId = %u",
                                   fne->m_tagDMR->lastParrotPeerId(), fne->m_tagDMR->lastParrotSrcId(), fne->m_tagDMR->lastParrotDstId());
                        fne->m_tagDMR->clearParrotPlayback();
                    }

                    // if the P25 handle is marked as playing back parrot frames, but has no more frames in the queue
                    // clear the playback flag
                    if (fne->m_tagP25->isParrotPlayback() && !fne->m_tagP25->hasParrotFrames()) {
                        LogInfoEx(LOG_MASTER, "P25, Parrot Call End, peer = %u, srcId = %u, dstId = %u",
                                   fne->m_tagP25->lastParrotPeerId(), fne->m_tagP25->lastParrotSrcId(), fne->m_tagP25->lastParrotDstId());
                        fne->m_tagP25->clearParrotPlayback();
                    }

                    // if the NXDN handle is marked as playing back parrot frames, but has no more frames in the queue
                    // clear the playback flag
                    if (fne->m_tagNXDN->isParrotPlayback() && !fne->m_tagNXDN->hasParrotFrames()) {
                        LogInfoEx(LOG_MASTER, "NXDN, Parrot Call End, peer = %u, srcId = %u, dstId = %u",
                                   fne->m_tagNXDN->lastParrotPeerId(), fne->m_tagNXDN->lastParrotSrcId(), fne->m_tagNXDN->lastParrotDstId());
                        fne->m_tagNXDN->clearParrotPlayback();
                    }

                    // if the analog handle is marked as playing back parrot frames, but has no more frames in the queue
                    // clear the playback flag
                    if (fne->m_tagAnalog->isParrotPlayback() && !fne->m_tagAnalog->hasParrotFrames()) {
                        LogInfoEx(LOG_MASTER, "Analog, Parrot Call End, peer = %u, srcId = %u, dstId = %u",
                                   fne->m_tagAnalog->lastParrotPeerId(), fne->m_tagAnalog->lastParrotSrcId(), fne->m_tagAnalog->lastParrotDstId());
                        fne->m_tagAnalog->clearParrotPlayback();
                    }
                }

                Thread::sleep(1U);
            }
        }

        LogInfoEx(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Process a data frames from the network. */

void TrafficNetwork::taskNetworkRx(NetPacketRequest* req)
{
    if (req != nullptr) {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        TrafficNetwork* network = static_cast<TrafficNetwork*>(req->obj);
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
            uint32_t ssrc = req->rtpHeader.getSSRC();
            uint32_t streamId = req->fneHeader.getStreamId();

            // determine if this packet is late (i.e. are we processing this packet more than 250ms after it was received?)
            uint64_t dt = req->pktRxTime + PACKET_LATE_TIME;
            if (dt < now) {
                std::string peerIdentity = network->resolvePeerIdentity(peerId);
                LogWarning(LOG_MASTER, "PEER %u (%s) packet processing latency >250ms, ssrc = %u, dt = %u, now = %u", peerId, peerIdentity.c_str(),
                    ssrc, dt, now);
            }

            // update current peer packet sequence and stream ID
            if (peerId > 0U && (network->m_peers.find(peerId) != network->m_peers.end()) && streamId != 0U) {
                FNEPeerConnection* connection = network->m_peers[peerId];
                uint16_t pktSeq = req->rtpHeader.getSequence();

                if (connection != nullptr) {
                    uint16_t lastRxSeq = 0U;

                    MULTIPLEX_RET_CODE ret = connection->verifyStream(streamId, pktSeq, req->fneHeader.getFunction(), &lastRxSeq);
                    if (ret == MUX_LOST_FRAMES) {
                        LogError(LOG_MASTER, "PEER %u (%s) stream %u possible lost frames; got %u, expected %u", peerId, connection->identWithQualifier().c_str(),
                            streamId, pktSeq, lastRxSeq);
                    }
                    else if (ret == MUX_OUT_OF_ORDER) {
                        LogError(LOG_MASTER, "PEER %u (%s) stream %u out-of-order; got %u, expected >%u", peerId, connection->identWithQualifier().c_str(),
                            streamId, pktSeq, lastRxSeq);
                    }
                }

                network->m_peers[peerId] = connection;
            }

            // if we don't have a stream ID and are receiving call data -- throw an error and discard
            if (streamId == 0U && req->fneHeader.getFunction() == NET_FUNC::PROTOCOL) {
                std::string peerIdentity = network->resolvePeerIdentity(peerId);
                LogError(LOG_MASTER, "PEER %u (%s) malformed packet (no stream ID for a call?)", peerId, peerIdentity.c_str());

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
                                                // check if jitter buffer is enabled for this peer
                                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                                    AdaptiveJitterBuffer* buffer = connection->getOrCreateJitterBuffer(streamId);
                                                    std::vector<BufferedFrame*> readyFrames;

                                                    buffer->processFrame(req->rtpHeader.getSequence(), req->buffer, req->length, readyFrames);

                                                    // process all frames that are now ready (in sequence order)
                                                    for (BufferedFrame* frame : readyFrames) {
                                                        network->m_tagDMR->processFrame(frame->data, frame->length, peerId, ssrc, frame->seq, streamId);
                                                        delete frame;
                                                    }
                                                } else {
                                                    // zero-latency fast path: no jitter buffer
                                                    network->m_tagDMR->processFrame(req->buffer, req->length, peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                                }
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
                                                // check if jitter buffer is enabled for this peer
                                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                                    AdaptiveJitterBuffer* buffer = connection->getOrCreateJitterBuffer(streamId);
                                                    std::vector<BufferedFrame*> readyFrames;

                                                    buffer->processFrame(req->rtpHeader.getSequence(), req->buffer, req->length, readyFrames);

                                                    // process all frames that are now ready (in sequence order)
                                                    for (BufferedFrame* frame : readyFrames) {
                                                        network->m_tagP25->processFrame(frame->data, frame->length, peerId, ssrc, frame->seq, streamId);
                                                        delete frame;
                                                    }
                                                } else {
                                                    // zero-latency fast path: no jitter buffer
                                                    network->m_tagP25->processFrame(req->buffer, req->length, peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                                }
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
                                                // check if jitter buffer is enabled for this peer
                                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                                    AdaptiveJitterBuffer* buffer = connection->getOrCreateJitterBuffer(streamId);
                                                    std::vector<BufferedFrame*> readyFrames;

                                                    buffer->processFrame(req->rtpHeader.getSequence(), req->buffer, req->length, readyFrames);

                                                    // process all frames that are now ready (in sequence order)
                                                    for (BufferedFrame* frame : readyFrames) {
                                                        network->m_tagNXDN->processFrame(frame->data, frame->length, peerId, ssrc, frame->seq, streamId);
                                                        delete frame;
                                                    }
                                                } else {
                                                    // zero-latency fast path: no jitter buffer
                                                    network->m_tagNXDN->processFrame(req->buffer, req->length, peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                                }
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

                    case NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG:          // Encapsulated analog data frame
                        {
                            if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                                FNEPeerConnection* connection = network->m_peers[peerId];
                                if (connection != nullptr) {
                                    std::string ip = udp::Socket::address(req->address);
                                    connection->lastPing(now);

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip) {
                                        if (network->m_analogEnabled) {
                                            if (network->m_tagAnalog != nullptr) {
                                                // check if jitter buffer is enabled for this peer
                                                if (connection->jitterBufferEnabled() && req->rtpHeader.getSequence() != RTP_END_OF_CALL_SEQ) {
                                                    AdaptiveJitterBuffer* buffer = connection->getOrCreateJitterBuffer(streamId);
                                                    std::vector<BufferedFrame*> readyFrames;

                                                    buffer->processFrame(req->rtpHeader.getSequence(), req->buffer, req->length, readyFrames);

                                                    // process all frames that are now ready (in sequence order)
                                                    for (BufferedFrame* frame : readyFrames) {
                                                        network->m_tagAnalog->processFrame(frame->data, frame->length, peerId, ssrc, frame->seq, streamId);
                                                        delete frame;
                                                    }
                                                } else {
                                                    // zero-latency fast path: no jitter buffer
                                                    network->m_tagAnalog->processFrame(req->buffer, req->length, peerId, ssrc, req->rtpHeader.getSequence(), streamId);
                                                }
                                            }
                                        } else {
                                            network->writePeerNAK(peerId, streamId, TAG_ANALOG_DATA, NET_CONN_NAK_MODE_NOT_ENABLED);
                                        }
                                    }
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, TAG_ANALOG_DATA, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                            }
                        }
                        break;

                    default:
                        Utils::dump("Unknown protocol opcode from peer", req->buffer, req->length);
                        break;
                    }
                }
                break;

            case NET_FUNC::RPTL:                                        // Repeater/Peer Login
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) == network->m_peers.end())) {
                        if (network->m_peers.size() >= MAX_HARD_CONN_CAP) {
                            LogError(LOG_MASTER, "PEER %u attempted to connect with no more connections available, currConnections = %u", peerId, network->m_peers.size());
                            network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_MAX_CONN, req->address, req->addrLen);
                            break;
                        }

                        if (network->m_softConnLimit > 0U && network->m_peers.size() >= network->m_softConnLimit) {
                            LogError(LOG_MASTER, "PEER %u attempted to connect with no more connections available, maxConnections = %u, currConnections = %u", peerId, network->m_softConnLimit, network->m_peers.size());
                            network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_MAX_CONN, req->address, req->addrLen);
                            break;
                        }

                        FNEPeerConnection* connection = new FNEPeerConnection(peerId, req->address, req->addrLen);
                        connection->lastPing(now);

                        network->applyJitterBufferConfig(peerId, connection);
                        network->setupRepeaterLogin(peerId, streamId, connection);

                        // check if the peer is in the peer ACL list
                        if (network->m_peerListLookup->getACL()) {
                            if (network->m_peerListLookup->isPeerListEmpty()) {
                                LogWarning(LOG_MASTER, "Peer List ACL enabled, but we have an empty peer list? Passing all peers.");
                            }

                            if (!network->m_peerListLookup->isPeerAllowed(peerId) && !network->m_peerListLookup->isPeerListEmpty()) {
                                LogWarning(LOG_MASTER, "PEER %u RPTL, failed peer ACL check", peerId);

                                network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);
                                network->disconnectPeer(peerId, connection);
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
                                    LogInfoEx(LOG_MASTER, "PEER %u (%s) resetting peer connection, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                        connection->connectionState());
                                    delete connection;

                                    connection = new FNEPeerConnection(peerId, req->address, req->addrLen);
                                    connection->lastPing(now);

                                    network->applyJitterBufferConfig(peerId, connection);
                                    network->erasePeerAffiliations(peerId);
                                    network->setupRepeaterLogin(peerId, streamId, connection);

                                    // check if the peer is in the peer ACL list
                                    if (network->m_peerListLookup->getACL()) {
                                        if (network->m_peerListLookup->isPeerListEmpty()) {
                                            LogWarning(LOG_MASTER, "Peer List ACL enabled, but we have an empty peer list? Passing all peers.");
                                        }

                                        if (!network->m_peerListLookup->isPeerAllowed(peerId) && !network->m_peerListLookup->isPeerListEmpty()) {
                                            LogWarning(LOG_MASTER, "PEER %u RPTL, failed peer ACL check", peerId);

                                            network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);
                                            network->disconnectPeer(peerId, connection);
                                        }
                                    }
                                } else {
                                    // perform source address validation
                                    if (connection->address() != udp::Socket::address(req->address)) {
                                        LogError(LOG_MASTER, "PEER %u RPTL NAK, IP address mismatch on RPTL attempt while not running, old = %s:%u, new = %s:%u, connectionState = %u", peerId,
                                            connection->address().c_str(), connection->port(), udp::Socket::address(req->address).c_str(), udp::Socket::port(req->address), connection->connectionState());

                                        network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                                        break;
                                    }

                                    network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);

                                    LogWarning(LOG_MASTER, "PEER %u (%s) RPTL NAK, bad connection state, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                        connection->connectionState());
                                    network->disconnectPeer(peerId, connection);
                                }
                            } else {
                                network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                                network->erasePeer(peerId);
                                LogWarning(LOG_MASTER, "PEER %u RPTL NAK, having no connection", peerId);
                            }
                        }
                    }
                }
                break;
            case NET_FUNC::RPTK:                                        // Repeater/Peer Authentication
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            connection->lastPing(now);

                            if (connection->connectionState() == NET_STAT_WAITING_AUTHORISATION) {
                                // get the hash from the frame message
                                DECLARE_UINT8_ARRAY(hash, req->length - 8U);
                                ::memcpy(hash, req->buffer + 8U, req->length - 8U);

                                // generate our own hash
                                uint8_t salt[4U];
                                ::memset(salt, 0x00U, 4U);
                                SET_UINT32(connection->salt(), salt, 0U);

                                std::string passwordForPeer = network->m_password;

                                // check if the peer is in the peer ACL list
                                bool validAcl = true;
                                if (network->m_peerListLookup->getACL()) {
                                    if (!network->m_peerListLookup->isPeerAllowed(peerId) && !network->m_peerListLookup->isPeerListEmpty()) {
                                        LogWarning(LOG_MASTER, "PEER %u RPTK, failed peer ACL check", peerId);
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
                                        LogWarning(LOG_MASTER, "Peer List ACL enabled, but we have an empty peer list? Passing all peers.");
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
                                        LogInfoEx(LOG_MASTER, "PEER %u RPTK ACK, completed the login exchange", peerId);
                                        network->m_peers[peerId] = connection;
                                    }
                                    else {
                                        LogWarning(LOG_MASTER, "PEER %u RPTK NAK, failed the login exchange", peerId);
                                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                                        network->disconnectPeer(peerId, connection);
                                    }
                                } else {
                                    network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_PEER_ACL, req->address, req->addrLen);
                                    network->disconnectPeer(peerId, connection);
                                }
                            }
                            else {
                                // perform source address/port validation
                                if (connection->address() != udp::Socket::address(req->address) ||
                                    connection->port() != udp::Socket::port(req->address)) {
                                    LogError(LOG_MASTER, "PEER %u RPTK NAK, IP address/port mismatch on RPTK attempt while in an incorrect state, old = %s:%u, new = %s:%u, connectionState = %u", peerId,
                                        connection->address().c_str(), connection->port(), udp::Socket::address(req->address).c_str(), udp::Socket::port(req->address), connection->connectionState());

                                    network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                                    break;
                                }

                                LogWarning(LOG_MASTER, "PEER %u RPTK NAK, login exchange while in an incorrect state, connectionState = %u", peerId, connection->connectionState());
                                network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                                network->disconnectPeer(peerId, connection);
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                        network->erasePeer(peerId);
                        LogWarning(LOG_MASTER, "PEER %u RPTK NAK, having no connection", peerId);
                    }
                }
                break;
            case NET_FUNC::RPTC:                                        // Repeater/Peer Configuration
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            connection->lastPing(now);

                            if (connection->connectionState() == NET_STAT_WAITING_CONFIG) {
                                DECLARE_UINT8_ARRAY(rawPayload, req->length - 8U);
                                ::memcpy(rawPayload, req->buffer + 8U, req->length - 8U);
                                std::string payload(rawPayload, rawPayload + (req->length - 8U));

                                // parse JSON body
                                json::value v;
                                std::string err = json::parse(v, payload);
                                if (!err.empty()) {
                                    LogWarning(LOG_MASTER, "PEER %u RPTC NAK, supplied invalid configuration data", peerId);
                                    network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_INVALID_CONFIG_DATA, req->address, req->addrLen);
                                    network->disconnectPeer(peerId, connection);
                                }
                                else  {
                                    // ensure parsed JSON is an object
                                    if (!v.is<json::object>()) {
                                        LogWarning(LOG_MASTER, "PEER %u RPTC NAK, supplied invalid configuration data", peerId);
                                        network->writePeerNAK(peerId, TAG_REPEATER_AUTH, NET_CONN_NAK_INVALID_CONFIG_DATA, req->address, req->addrLen);
                                        network->disconnectPeer(peerId, connection);
                                    }
                                    else {
                                        connection->config(v.get<json::object>());
                                        connection->connectionState(NET_STAT_RUNNING);
                                        connection->connected(true);
                                        connection->pingsReceived(0U);
                                        connection->lastPing(now);
                                        connection->missedMetadataUpdates(0U);

                                        lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                                        if (!peerEntry.peerDefault()) {
                                            if (peerEntry.hasCallPriority()) {
                                                connection->hasCallPriority(peerEntry.hasCallPriority());
                                                LogInfoEx(LOG_MASTER, "PEER %u >> Has Call Priority", peerId);
                                            }
                                        }

                                        network->m_peers[peerId] = connection;

                                        // attach extra notification data to the RPTC ACK to notify the peer of 
                                        // the use of the alternate diagnostic port
                                        uint8_t buffer[1U];
                                        buffer[0U] = 0x80U; // this should really be a defined constant -- but
                                                            // because this is the only option and its *always* sent now
                                                            // we can just hardcode this for now

                                        json::object peerConfig = connection->config();

                                        std::string identity = "* UNK *";
                                        if (peerConfig["identity"].is<std::string>()) {
                                            identity = peerConfig["identity"].getDefault<std::string>("* UNK *");
                                            connection->identity(identity);
                                            LogInfoEx(LOG_MASTER, "PEER %u >> Identity [%8s]", peerId, identity.c_str());
                                        }

                                        if (peerConfig["software"].is<std::string>()) {
                                            std::string software = peerConfig["software"].get<std::string>();
                                            LogInfoEx(LOG_MASTER, "PEER %u >> Software Version [%s]", peerId, software.c_str());
                                        }

                                        // is the peer reporting it is a SysView peer?
                                        if (peerConfig["sysView"].is<bool>()) {
                                            bool sysView = peerConfig["sysView"].get<bool>();
                                            connection->isSysView(sysView);
                                            if (sysView)
                                                LogInfoEx(LOG_MASTER, "PEER %u >> SysView Peer", peerId);
                                        }

                                        // is the peer reporting it is an downstream FNE neighbor peer?
                                        /*
                                        ** bryanb: don't change externalPeer to neighborPeer -- this will break backward
                                        **  compat with older FNE versions (we're stuck with this naming :()
                                        */
                                        if (peerConfig["externalPeer"].is<bool>()) {
                                            bool neighbor = peerConfig["externalPeer"].get<bool>();
                                            connection->isNeighborFNEPeer(neighbor);
                                            if (neighbor)
                                                LogInfoEx(LOG_MASTER, "PEER %u >> Downstream Neighbor FNE Peer", peerId);

                                            uint32_t masterPeerId = 0U;
                                            if (peerConfig["masterPeerId"].is<uint32_t>()) {
                                                masterPeerId = peerConfig["masterPeerId"].get<uint32_t>();
                                                connection->masterId(masterPeerId);
                                                LogInfoEx(LOG_MASTER, "PEER %u >> Master Peer ID [%u]", peerId, masterPeerId);
                                            }

                                            // master peer ID should never be zero for an neighbor peer -- use the peer ID instead
                                            if (masterPeerId == 0U) {
                                                LogWarning(LOG_MASTER, "PEER %u reports to be a downstream FNE neighbor peer but has not supplied a valid masterPeerId, using own peerId as masterPeerId (old FNE perhaps?)", peerId);
                                                masterPeerId = peerId;
                                            }

                                            // check if the peer a peer replication participant
                                            lookups::PeerId peerEntry = network->m_peerListLookup->find(req->peerId);
                                            if (!peerEntry.peerDefault()) {
                                                if (peerEntry.peerReplica()) {
                                                    connection->isReplica(true);
                                                    if (neighbor)
                                                        LogInfoEx(LOG_MASTER, "PEER %u >> Participates in Peer Replication", peerId);
                                                }
                                            }

                                            if (network->m_enableSpanningTree && !connection->isSysView()) {
                                                network->m_treeLock.lock();

                                                // check if this peer is already connected via another peer
                                                SpanningTree* tree = SpanningTree::findByMasterID(masterPeerId);
                                                if (tree != nullptr) {
                                                    // are we allowing a fast reconnect? (this happens when a connecting peer
                                                    //  uses the same peer ID and master ID already announced in the tree, but
                                                    //  the tree entry wasn't yet erased)
                                                    if ((tree->id() == peerId && tree->masterId() == masterPeerId) &&
                                                        network->m_spanningTreeFastReconnect) {
                                                        LogWarning(LOG_STP, "PEER %u (%s) server already announced in server tree, fast peer reconnect, peerId = %u, masterId = %u, treePeerId = %u, treeMasterId = %u, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                                            peerId, masterPeerId, tree->id(), tree->masterId(), connection->connectionState());
                                                        if (identity != tree->identity()) {
                                                            LogWarning(LOG_STP, "PEER %u (%s) why has this server's announced identity changed? *big hmmmm*", peerId, connection->identWithQualifier().c_str());
                                                        }
                                                        SpanningTree::moveParent(tree, network->m_treeRoot);
                                                        network->logSpanningTree(connection);
                                                    } else {
                                                        LogWarning(LOG_STP, "PEER %u (%s) RPTC NAK, server already connected via PEER %u, duplicate connection denied, peerId = %u, masterId = %u, treePeerId = %u, treeMasterId = %u, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                                            peerId, masterPeerId, tree->id(), tree->masterId(), tree->id(), connection->connectionState());
                                                        network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_FNE_DUPLICATE_CONN, req->address, req->addrLen);
                                                        network->m_treeLock.unlock();
                                                        network->disconnectPeer(peerId, connection);
                                                        break;
                                                    }
                                                } else {
                                                    SpanningTree* node = new SpanningTree(peerId, masterPeerId, network->m_treeRoot);
                                                    node->identity(identity);
                                                    network->logSpanningTree(connection);
                                                }

                                                network->m_treeLock.unlock();
                                            }
                                        }

                                        network->writePeerACK(peerId, streamId, buffer, 1U);
                                        LogInfoEx(LOG_MASTER, "PEER %u RPTC ACK, completed the configuration exchange", peerId);

                                        // is the peer reporting it is a conventional peer?
                                        if (peerConfig["conventionalPeer"].is<bool>()) {
                                            if (network->m_allowConvSiteAffOverride) {
                                                bool convPeer = peerConfig["conventionalPeer"].get<bool>();
                                                connection->isConventionalPeer(convPeer);
                                                if (convPeer)
                                                    LogInfoEx(LOG_MASTER, "PEER %u >> Conventional Peer", peerId);
                                            }
                                        }

                                        // setup the affiliations list for this peer
                                        std::stringstream peerName;
                                        peerName << "PEER " << peerId;
                                        network->createPeerAffiliations(peerId, peerName.str());

                                        // spin up a thread and send metadata over to peer
                                        network->peerMetadataUpdate(peerId);
                                    }
                                }
                            }
                            else {
                                // perform source address/port validation
                                if (connection->address() != udp::Socket::address(req->address) ||
                                    connection->port() != udp::Socket::port(req->address)) {
                                    LogError(LOG_MASTER, "PEER %u (%s) RPTC NAK, IP address/port mismatch on RPTC attempt while in an incorrect state, old = %s:%u, new = %s:%u, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                        connection->address().c_str(), connection->port(), udp::Socket::address(req->address).c_str(), udp::Socket::port(req->address), connection->connectionState());

                                    network->writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_FNE_UNAUTHORIZED, req->address, req->addrLen);
                                    break;
                                }

                                LogWarning(LOG_MASTER, "PEER %u (%s) RPTC NAK, login exchange while in an incorrect state, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                    connection->connectionState());
                                network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                                network->disconnectPeer(peerId, connection);
                            }
                        }
                    }
                    else {
                        network->writePeerNAK(peerId, TAG_REPEATER_CONFIG, NET_CONN_NAK_BAD_CONN_STATE, req->address, req->addrLen);
                        LogWarning(LOG_MASTER, "PEER %u RPTC NAK, having no connection", peerId);
                    }
                }
                break;

            case NET_FUNC::RPT_DISC:                                    // Repeater/Peer Disconnect
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                LogInfoEx(LOG_MASTER, "PEER %u (%s) disconnected", peerId, connection->identWithQualifier().c_str());
                                network->disconnectPeer(peerId, connection);
                            }
                        }
                    }
                }
                break;
            case NET_FUNC::PING:                                        // Ping
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
                                    LogInfoEx(LOG_MASTER, "PEER %u (%s) ping, pingsReceived = %u, lastPing = %u, now = %u", peerId, connection->identWithQualifier().c_str(),
                                        connection->pingsReceived(), lastPing, now);
                                }

                                // ensure STP sanity, when we receive a ping from a downstream leaf
                                //  this check ensures a STP entry for a downstream leaf isn't accidentally blown off
                                //  the tree during a fast reconnect
                                if (network->m_enableSpanningTree && connection->isNeighborFNEPeer() && !connection->isSysView()) {
                                    std::lock_guard<std::mutex> guard(network->m_treeLock);

                                    if ((connection->masterId() != peerId) && (connection->masterId() != 0U)) {
                                        // check if this peer is already connected via another peer
                                        SpanningTree* tree = SpanningTree::findByMasterID(connection->masterId());
                                        if (tree == nullptr) {
                                            LogWarning(LOG_STP, "PEER %u (%s) downstream server not announced in server tree, reinitializing STP entry, this is abnormal, peerId = %u, masterId = %u, connectionState = %u", peerId, connection->identWithQualifier().c_str(),
                                                peerId, connection->masterId(), connection->connectionState());
                                            SpanningTree* node = new SpanningTree(peerId, connection->masterId(), network->m_treeRoot);
                                            node->identity(connection->identity());
                                            network->logSpanningTree(connection);
                                        }
                                    }
                                }
                            }
                            else {
                                network->writePeerNAK(peerId, streamId, TAG_REPEATER_PING);
                            }
                        }
                    }
                }
                break;

            case NET_FUNC::GRANT_REQ:                                   // Grant Request
                {
                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                uint32_t srcId = GET_UINT24(req->buffer, 11U);                  // Source Address
                                uint32_t dstId = GET_UINT24(req->buffer, 15U);                  // Destination Address

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
                                    Utils::dump("Unknown state for grant request from the peer", req->buffer, req->length);
                                    break;
                                }
                            }
                        }
                    }
                }
                break;

            case NET_FUNC::INCALL_CTRL:                                 // In-Call Control
                {
                    if (network->m_disallowInCallCtrl)
                        break;

                    if (peerId > 0 && (network->m_peers.find(peerId) != network->m_peers.end())) {
                        FNEPeerConnection* connection = network->m_peers[peerId];
                        if (connection != nullptr) {
                            std::string ip = udp::Socket::address(req->address);

                            // validate peer (simple validation really)
                            if (connection->connected() && connection->address() == ip) {
                                NET_ICC::ENUM command = (NET_ICC::ENUM)req->buffer[10U];
                                uint32_t dstId = GET_UINT24(req->buffer, 11U);
                                uint8_t slot = req->buffer[14U];

                                network->processInCallCtrl(command, req->fneHeader.getSubFunction(), dstId, slot, peerId, ssrc, streamId);
                            }
                        }
                    }
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
                                    lookups::PeerId peerEntry = network->m_peerListLookup->find(peerId);
                                    if (peerEntry.peerDefault()) {
                                        break;
                                    } else {
                                        if (!peerEntry.canRequestKeys()) {
                                            LogError(LOG_MASTER, "PEER %u (%s) requested enc. key but is not allowed, no response", peerId, connection->identWithQualifier().c_str());
                                            break;
                                        }
                                    }
                                }

                                std::unique_ptr<KMMFrame> frame = KMMFactory::create(req->buffer + 11U);
                                if (frame == nullptr) {
                                    LogWarning(LOG_MASTER, "PEER %u (%s), undecodable KMM frame from peer", peerId, connection->identWithQualifier().c_str());
                                    break;
                                }

                                switch (frame->getMessageId()) {
                                case P25DEF::KMM_MessageType::MODIFY_KEY_CMD:
                                    {
                                        KMMModifyKey* modifyKey = static_cast<KMMModifyKey*>(frame.get());
                                        if (modifyKey->getAlgId() > 0U && modifyKey->getKId() > 0U) {
                                            LogInfoEx(LOG_MASTER, "PEER %u (%s) requested enc. key, algId = $%02X, kID = $%04X", peerId, connection->identWithQualifier().c_str(),
                                                modifyKey->getAlgId(), modifyKey->getKId());
                                            ::EKCKeyItem keyItem = network->m_cryptoLookup->find(modifyKey->getKId());
                                            if (!keyItem.isInvalid()) {
                                                uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
                                                ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                                uint8_t keyLength = keyItem.getKey(key);

                                                if (network->m_debug) {
                                                    LogDebugEx(LOG_HOST, "TrafficNetwork::threadedNetworkRx()", "keyLength = %u", keyLength);
                                                    Utils::dump(1U, "TrafficNetwork::taskNetworkRx(), Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
                                                }

                                                LogInfoEx(LOG_MASTER, "PEER %u (%s) local enc. key, algId = $%02X, kID = $%04X", peerId, connection->identWithQualifier().c_str(),
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

                                                network->writePeer(peerId, network->m_peerId, { NET_FUNC::KEY_RSP, NET_SUBFUNC::NOP }, buffer, modifyKeyRsp.length() + 11U, 
                                                    RTP_END_OF_CALL_SEQ, network->createStreamId());
                                            } else {
                                                // attempt to forward KMM key request to replica masters
                                                if (network->m_host->m_peerNetworks.size() > 0) {
                                                    for (auto& peer : network->m_host->m_peerNetworks) {
                                                        if (peer.second != nullptr) {
                                                            if (peer.second->isEnabled() && peer.second->isReplica()) {
                                                                LogInfoEx(LOG_PEER, "PEER %u (%s) no local key or container, requesting key from upstream master, algId = $%02X, kID = $%04X", peerId, connection->identWithQualifier().c_str(),
                                                                    modifyKey->getAlgId(), modifyKey->getKId());

                                                                bool locked = network->s_keyQueueMutex.try_lock_for(std::chrono::milliseconds(60));
                                                                network->m_peerReplicaKeyQueue[peerId] = modifyKey->getKId();

                                                                if (locked)
                                                                    network->s_keyQueueMutex.unlock();

                                                                peer.second->writeMaster({ NET_FUNC::KEY_REQ, NET_SUBFUNC::NOP }, 
                                                                    req->buffer, req->length, RTP_END_OF_CALL_SEQ, 0U, false);
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
                                        LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                    }

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                        uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                                        uint32_t dstId = GET_UINT24(req->buffer, 3U);           // Destination Address
                                        aff->groupUnaff(srcId);
                                        aff->groupAff(srcId, dstId);

                                        // attempt to repeat traffic to replica masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto& peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isReplica()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_GRP_AFFIL }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false);
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
                                    fne_lookups::AffiliationLookup* aff = network->m_peerAffiliations[peerId];
                                    if (aff == nullptr) {
                                        LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                    }

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                        uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                                        aff->unitReg(srcId, ssrc);

                                        // attempt to repeat traffic to replica masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto& peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isReplica()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_REG }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false, 0U, ssrc);
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
                                        LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                    }

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                        uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                                        aff->unitDereg(srcId);

                                        // attempt to repeat traffic to replica masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto& peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isReplica()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_UNIT_DEREG }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false);
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
                                        LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                                        network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                    }

                                    // validate peer (simple validation really)
                                    if (connection->connected() && connection->address() == ip && aff != nullptr) {
                                        uint32_t srcId = GET_UINT24(req->buffer, 0U);           // Source Address
                                        aff->groupUnaff(srcId);

                                        // attempt to repeat traffic to replica masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto& peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isReplica()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_GRP_UNAFFIL }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false);
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
                                            LogError(LOG_MASTER, "PEER %u (%s) has uninitialized affiliations lookup?", peerId, connection->identWithQualifier().c_str());
                                            network->writePeerNAK(peerId, streamId, TAG_ANNOUNCE, NET_CONN_NAK_INVALID);
                                        }

                                        if (aff != nullptr) {
                                            aff->clearGroupAff(0U, true);

                                            // update TGID lists
                                            uint32_t len = GET_UINT32(req->buffer, 0U);
                                            uint32_t offs = 4U;
                                            for (uint32_t i = 0; i < len; i++) {
                                                uint32_t srcId = GET_UINT24(req->buffer, offs);
                                                uint32_t dstId = GET_UINT24(req->buffer, offs + 4U);

                                                aff->groupAff(srcId, dstId);
                                                offs += 8U;
                                            }
                                            LogInfoEx(LOG_MASTER, "PEER %u (%s) announced %u affiliations", peerId, connection->identWithQualifier().c_str(), len);

                                            // attempt to repeat traffic to replica masters
                                            if (network->m_host->m_peerNetworks.size() > 0) {
                                                for (auto& peer : network->m_host->m_peerNetworks) {
                                                    if (peer.second != nullptr) {
                                                        if (peer.second->isEnabled() && peer.second->isReplica()) {
                                                            peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_AFFILS }, 
                                                                req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false);
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
                                        uint32_t len = GET_UINT32(req->buffer, 0U);
                                        uint32_t offs = 4U;
                                        for (uint32_t i = 0; i < len; i++) {
                                            uint32_t vcPeerId = GET_UINT32(req->buffer, offs);
                                            if (vcPeerId > 0 && (network->m_peers.find(vcPeerId) != network->m_peers.end())) {
                                                FNEPeerConnection* vcConnection = network->m_peers[vcPeerId];
                                                if (vcConnection != nullptr) {
                                                    vcConnection->ccPeerId(peerId);
                                                    vcPeers.push_back(vcPeerId);
                                                }
                                            }
                                            offs += 4U;
                                        }
                                        LogInfoEx(LOG_MASTER, "PEER %u (%s) announced %u VCs", peerId, connection->identWithQualifier().c_str(), len);
                                        network->m_ccPeerMap[peerId] = vcPeers;

                                        // attempt to repeat traffic to replica masters
                                        if (network->m_host->m_peerNetworks.size() > 0) {
                                            for (auto& peer : network->m_host->m_peerNetworks) {
                                                if (peer.second != nullptr) {
                                                    if (peer.second->isEnabled() && peer.second->isReplica()) {
                                                        peer.second->writeMaster({ NET_FUNC::ANNOUNCE, NET_SUBFUNC::ANNC_SUBFUNC_SITE_VC }, 
                                                            req->buffer, req->length, req->rtpHeader.getSequence(), streamId, false);
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
                        Utils::dump("Unknown announcement opcode from the peer", req->buffer, req->length);
                    }
                }
                break;

            default:
                Utils::dump("Unknown opcode from the peer", req->buffer, req->length);
                break;
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}

/* Checks if the passed peer ID is blocked from unit-to-unit traffic. */

bool TrafficNetwork::checkU2UDroppedPeer(uint32_t peerId)
{
    if (m_dropU2UPeerTable.empty())
        return false;

    if (std::find(m_dropU2UPeerTable.begin(), m_dropU2UPeerTable.end(), peerId) != m_dropU2UPeerTable.end()) {
        return true;
    }

    return false;
}

/* Helper to dump the current spanning tree configuration to the log. */

void TrafficNetwork::logSpanningTree(FNEPeerConnection* connection)
{
    if (!m_enableSpanningTree)
        return;

    if (m_logSpanningTreeChanges && m_treeRoot->hasChildren()) {
        if (connection != nullptr)
            LogInfoEx(LOG_STP, "PEER %u (%s) Network Tree, Tree Change, Current Tree", connection->id(), connection->identWithQualifier().c_str());
        else
            LogInfoEx(LOG_STP, "PEER %u Network Tree, Tree Display, Current Tree", m_peerId);
        SpanningTree::visualizeTreeToLog(m_treeRoot);
    }
}

/* Applies jitter buffer configuration to a peer connection. */

void TrafficNetwork::applyJitterBufferConfig(uint32_t peerId, FNEPeerConnection* connection)
{
    if (connection == nullptr) {
        return;
    }

    if (m_jitterBufferEnabled) {
        // use global settings
        connection->setJitterBufferParams(m_jitterBufferEnabled, m_jitterMaxSize, m_jitterMaxWait);
        if (m_verbose && m_jitterBufferEnabled) {
            LogInfoEx(LOG_MASTER, "PEER %u jitter buffer configured (global), maxSize = %u, maxWait = %u",
                peerId, m_jitterMaxSize, m_jitterMaxWait);
        }
    } else {
        lookups::PeerId peerEntry = m_peerListLookup->find(peerId);
        if (!peerEntry.peerDefault()) {
            connection->setJitterBufferParams(peerEntry.jitterBufferEnabled(),
                peerEntry.jitterBufferMaxSize(), peerEntry.jitterBufferMaxWait());
            if (m_verbose && peerEntry.jitterBufferEnabled()) {
                LogInfoEx(LOG_MASTER, "PEER %u jitter buffer configured (per-peer), maxSize = %u, maxWait = %u",
                    peerId, peerEntry.jitterBufferMaxSize(), peerEntry.jitterBufferMaxWait());
            }
        }
    }
}

/* Erases a stream ID from the given peer ID connection. */

void TrafficNetwork::eraseStreamPktSeq(uint32_t peerId, uint32_t streamId)
{
    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            connection->erasePktSeq(streamId);
        }
    }
}

/* Helper to create a peer on the peers affiliations list. */

void TrafficNetwork::createPeerAffiliations(uint32_t peerId, std::string peerName)
{
    erasePeerAffiliations(peerId);

    lookups::ChannelLookup* chLookup = new lookups::ChannelLookup();
    m_peerAffiliations[peerId] = new fne_lookups::AffiliationLookup(peerName, chLookup, m_verbose);
    m_peerAffiliations[peerId]->setDisableUnitRegTimeout(true); // FNE doesn't allow unit registration timeouts (notification must come from the peers)
}

/* Helper to erase the peer from the peers affiliations list. */

bool TrafficNetwork::erasePeerAffiliations(uint32_t peerId)
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

/* Helper to disconnect a downstream peer. */

void TrafficNetwork::disconnectPeer(uint32_t peerId, FNEPeerConnection* connection)
{
    if (peerId == 0U)
        return;
    if (connection == nullptr)
        return;

    connection->connected(false);
    connection->connectionState(NET_STAT_INVALID);

    connection->lock();
    erasePeer(peerId);
    connection->unlock();
    if (connection != nullptr) {
        delete connection;
    }
}

/* Helper to erase the peer from the peers list. */

void TrafficNetwork::erasePeer(uint32_t peerId)
{
    bool neighborFNE = false;
    {
        auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
        if (it != m_peers.end()) {
            neighborFNE = it->second->isNeighborFNEPeer();
            m_peers.erase(peerId);
        }
    }

    // erase any CC maps for this peer
    {
        auto it = std::find_if(m_ccPeerMap.begin(), m_ccPeerMap.end(), [&](auto& x) { return x.first == peerId; });
        if (it != m_ccPeerMap.end()) {
            m_ccPeerMap.erase(peerId);
        }
    }

    // erase any peer replication entries for this peer
    {
        auto it = std::find_if(m_peerReplicaPeers.begin(), m_peerReplicaPeers.end(), [&](auto& x) { return x.first == peerId; });
        if (it != m_peerReplicaPeers.end()) {
            m_peerReplicaPeers.erase(peerId);
        }
    }

    // erase any HA parameters for this peer
    {
        auto it = std::find_if(m_peerReplicaHAParams.begin(), m_peerReplicaHAParams.end(), [&](auto& x) { return x.peerId == peerId; });
        if (it != m_peerReplicaHAParams.end()) {
            m_peerReplicaHAParams.erase(it);
        }
    }

    if (neighborFNE && m_enableSpanningTree) {
        std::lock_guard<std::mutex> guard(m_treeLock);

        // erase this peer from the master tree
        SpanningTree* tree = SpanningTree::findByPeerID(peerId);
        if (tree != nullptr) {
            if (tree->hasChildren()) {
                uint32_t totalChildren = tree->countChildren(tree);

                // netsplit be as noisy as possible about it...
                for (uint8_t i = 0U; i < 3U; i++)
                    LogWarning(LOG_MASTER, "PEER %u downstream netsplit, lost %u downstream connections", peerId, totalChildren);
            }

            LogWarning(LOG_MASTER, "PEER %u downstream netsplit, disconnected", peerId);
            SpanningTree::erasePeer(peerId);
        }

        logSpanningTree();
    }

    // cleanup peer affiliations
    erasePeerAffiliations(peerId);
}

/* Helper to determine if the peer is local to this master. */

bool TrafficNetwork::isPeerLocal(uint32_t peerId)
{
    m_peers.shared_lock();
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        m_peers.shared_unlock();
        return true;
    }
    m_peers.shared_unlock();

    return false;
}

/* Helper to find the unit registration for the given source ID. */

uint32_t TrafficNetwork::findPeerUnitReg(uint32_t srcId)
{
    for (auto it = m_peerAffiliations.begin(); it != m_peerAffiliations.end(); ++it) {
        fne_lookups::AffiliationLookup* aff = it->second;
        if (aff != nullptr) {
            if (aff->isUnitReg(srcId)) {
                return aff->getSSRCByUnitReg(srcId);
            }
        }
    }

    return 0U;
}

/* Helper to create a JSON representation of a FNE peer connection. */

json::object TrafficNetwork::fneConnObject(uint32_t peerId, FNEPeerConnection *conn)
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
    auto it = std::find_if(m_ccPeerMap.begin(), m_ccPeerMap.end(), [&](auto& x) { return x.first == peerId; });
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

bool TrafficNetwork::resetPeer(uint32_t peerId)
{
    if (peerId > 0 && (m_peers.find(peerId) != m_peers.end())) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            sockaddr_storage addr = connection->socketStorage();
            uint32_t addrLen = connection->sockStorageLen();

            LogInfoEx(LOG_MASTER, "PEER %u (%s) resetting peer connection", peerId, connection->identWithQualifier().c_str());

            writePeerNAK(peerId, TAG_REPEATER_LOGIN, NET_CONN_NAK_PEER_RESET, addr, addrLen);
            connection->lock();
            erasePeer(peerId);
            connection->unlock();
            delete connection;

            return true;
        }
    }

    LogWarning(LOG_MASTER, "PEER %u reset failed; peer not found", peerId);
    return false;
}

/* Helper to set the master is upstream peer replica flag. */

void TrafficNetwork::setPeerReplica(bool replica)
{
    if (!m_isReplica && replica) {
        LogInfoEx(LOG_MASTER, "Set as upstream peer replica, receiving ACL updates from upstream master");
    }

    m_isReplica = replica;

    // be very noisy about being a peer replica and having multiple upstream peers
    if (m_isReplica) {
        if (m_host->m_peerNetworks.size() > 1) {
            LogWarning(LOG_MASTER, "We are a upstream peer replica, and have multiple upstream peers? This is a bad idea. Peer Replica FNEs should have a single upstream peer connection.");
        }
    }
}

/* Helper to resolve the peer ID to its identity string. */

std::string TrafficNetwork::resolvePeerIdentity(uint32_t peerId)
{
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        if (it->second != nullptr) {
            FNEPeerConnection* peer = it->second;
            return peer->identWithQualifier();
        }
    }

    return std::string();
}

/* Helper to complete setting up a repeater login request. */

void TrafficNetwork::setupRepeaterLogin(uint32_t peerId, uint32_t streamId, FNEPeerConnection* connection)
{
    std::uniform_int_distribution<uint32_t> dist(DVM_RAND_MIN, DVM_RAND_MAX);
    connection->salt(dist(m_random));

    LogInfoEx(LOG_MASTER, "PEER %u started login from, %s:%u", peerId, connection->address().c_str(), connection->port());

    connection->connectionState(NET_STAT_WAITING_AUTHORISATION);
    m_peers[peerId] = connection;

    // transmit salt to peer
    uint8_t salt[4U];
    ::memset(salt, 0x00U, 4U);
    SET_UINT32(connection->salt(), salt, 0U);

    writePeerACK(peerId, streamId, salt, 4U);
    LogInfoEx(LOG_MASTER, "PEER %u RPTL ACK, challenge response sent for login", peerId);
}

/* Helper to process an In-Call Control message. */

void TrafficNetwork::processInCallCtrl(network::NET_ICC::ENUM command, network::NET_SUBFUNC::ENUM subFunc, uint32_t dstId, 
    uint8_t slotNo, uint32_t peerId, uint32_t ssrc, uint32_t streamId)
{
    if (m_debug)
        LogDebugEx(LOG_HOST, "TrafficNetwork::processInCallCtrl()", "peerId = %u, command = $%02X, subFunc = $%02X, dstId = %u, slot = %u, ssrc = %u, streamId = %u", 
            peerId, command, subFunc, dstId, slotNo, ssrc, streamId);

    if (m_disallowInCallCtrl) {
        LogWarning(LOG_MASTER, "PEER %u In-Call Control disabled, ignoring ICC request, dstId = %u, slot = %u, ssrc = %u, streamId = %u", 
            peerId, dstId, slotNo, ssrc, streamId);
        return;
    }

    switch (command) {
    case network::NET_ICC::REJECT_TRAFFIC:
        {
            // is this a local peer?
            if (ssrc > 0 && (m_peers.find(ssrc) != m_peers.end())) {
                FNEPeerConnection* connection = m_peers[ssrc];
                if (connection != nullptr) {
                    // validate peer (simple validation really)
                    if (connection->connected()) {
                        LogInfoEx(LOG_MASTER, "PEER %u In-Call Control Request to Local Peer, dstId = %u, slot = %u, ssrc = %u, streamId = %u", peerId, dstId, slotNo, ssrc, streamId);

                        // send ICC request to local peer
                        writePeerICC(ssrc, streamId, subFunc, command, dstId, slotNo, true);

                        // flag the protocol call handler to allow call takeover on the next audio frame
                        switch (subFunc) {
                        case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:             // Encapsulated DMR data frame
                            m_tagDMR->triggerCallTakeover(dstId);
                            break;

                        case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:             // Encapsulated P25 data frame
                            m_tagP25->triggerCallTakeover(dstId);
                            break;

                        case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:            // Encapsulated NXDN data frame
                            m_tagNXDN->triggerCallTakeover(dstId);
                            break;

                        case NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG:          // Encapsulated analog data frame
                            m_tagAnalog->triggerCallTakeover(dstId);
                            break;

                        default:
                            break;
                        }
                    }
                }
            } else {
                // send ICC request to any peers connected to us that are neighbor FNEs
                m_peers.shared_lock();
                for (auto& peer : m_peers) {
                    if (peer.second == nullptr)
                        continue;
                    if (peerId != peer.first) {
                        FNEPeerConnection* conn = peer.second;
                        if (peerId == ssrc) {
                            // skip the peer if it is the source peer
                            continue;
                        }

                        if (conn->isNeighborFNEPeer()) {
                            LogInfoEx(LOG_MASTER, "PEER %u In-Call Control Request to Neighbors, peerId = %u, dstId = %u, slot = %u, ssrc = %u, streamId = %u", peerId, peer.first, dstId, slotNo, ssrc, streamId);

                            // send ICC request to local peer
                            writePeerICC(peer.first, streamId, subFunc, command, dstId, slotNo, true, false, ssrc);
                        }
                    }
                }
                m_peers.shared_unlock();

                // flag the protocol call handler to allow call takeover on the next audio frame
                switch (subFunc) {
                case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:             // Encapsulated DMR data frame
                    m_tagDMR->triggerCallTakeover(dstId);
                    break;

                case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:             // Encapsulated P25 data frame
                    m_tagP25->triggerCallTakeover(dstId);
                    break;

                case NET_SUBFUNC::PROTOCOL_SUBFUNC_NXDN:            // Encapsulated NXDN data frame
                    m_tagNXDN->triggerCallTakeover(dstId);
                    break;

                case NET_SUBFUNC::PROTOCOL_SUBFUNC_ANALOG:          // Encapsulated analog data frame
                    m_tagAnalog->triggerCallTakeover(dstId);
                    break;

                default:
                    break;
                }

                // send further up the network tree (only if ICC request came from a local peer)
                if (m_host->m_peerNetworks.size() > 0 && m_peers.find(peerId) != m_peers.end()) {
                    writePeerICC(peerId, streamId, subFunc, command, dstId, slotNo, true, true, ssrc);
                }
            }
        }
        break;

    default:
        break;
    }
}

/* Helper to send the network metadata to the specified peer in a separate thread. */

void TrafficNetwork::peerMetadataUpdate(uint32_t peerId)
{
    MetadataUpdateRequest* req = new MetadataUpdateRequest();
    req->obj = this;
    req->peerId = peerId;

    // enqueue the task
    if (!m_threadPool.enqueue(new_pooltask(taskMetadataUpdate, req))) {
        LogError(LOG_NET, "Failed to task enqueue metadata update, peerId = %u", peerId);
        if (req != nullptr)
            delete req;
    }
}

/* Helper to send the network metadata to the specified peer in a separate thread. */

void TrafficNetwork::taskMetadataUpdate(MetadataUpdateRequest* req)
{
    if (req != nullptr) {
        TrafficNetwork* network = static_cast<TrafficNetwork*>(req->obj);
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
            if (connection->connected()) {
                connection->lock();
                uint32_t streamId = network->createStreamId();

                // if the connection is a downstream neighbor FNE peer, and peer is participating in peer link,
                // send the peer proper configuration data
                if (connection->isNeighborFNEPeer() && connection->isReplica()) {
                    LogInfoEx(LOG_MASTER, "PEER %u (%s) sending replica network metadata updates", req->peerId, peerIdentity.c_str());

                    network->writeWhitelistRIDs(req->peerId, streamId, true);
                    network->writeTGIDs(req->peerId, streamId, true);
                    network->writePeerList(req->peerId, streamId);

                    network->writeHAParameters(req->peerId, streamId, true);
                }
                else {
                    LogInfoEx(LOG_MASTER, "PEER %u (%s) sending network metadata updates", req->peerId, peerIdentity.c_str());

                    network->writeWhitelistRIDs(req->peerId, streamId, false);
                    network->writeBlacklistRIDs(req->peerId, streamId);
                    network->writeTGIDs(req->peerId, streamId, false);
                    network->writeDeactiveTGIDs(req->peerId, streamId);

                    network->writeHAParameters(req->peerId, streamId, false);
                }

                connection->unlock();
            }
        }

        delete req;
    }
}

/*
** ACL Message Writing
*/

/* Helper to send the list of whitelisted RIDs to the specified peer. */

void TrafficNetwork::writeWhitelistRIDs(uint32_t peerId, uint32_t streamId, bool sendReplica)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // sending REPL style RID list to replica neighbor FNE peers
    if (sendReplica) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            // save out radio ID table to disk
            std::string tempFile;
            if (m_isReplica) {
                std::ostringstream s;
                std::random_device rd;
                std::mt19937 mt(rd());
                std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
                s << "/tmp/rid_acl.dat." << dist(mt);

                tempFile = s.str();
                std::string origFile = m_ridLookup->filename();
                m_ridLookup->filename(tempFile);
                m_ridLookup->commit(true);
                m_ridLookup->filename(origFile);
            } else {
                tempFile = m_ridLookup->filename();
            }

            // read entire file into string buffer
            std::stringstream b;
            std::ifstream stream(tempFile);
            if (stream.is_open()) {
                while (stream.peek() != EOF) {
                    b << (char)stream.get();
                }

                stream.close();
            }

            if (m_isReplica)
                ::remove(tempFile.c_str());

            // convert to a byte array
            uint32_t len = b.str().size();
            DECLARE_UINT8_ARRAY(buffer, len);
            ::memcpy(buffer, b.str().data(), len);

            PacketBuffer pkt(true, "Peer Replication, RID List");
            pkt.encode((uint8_t*)buffer, len);

            LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, RID List, blocks %u, streamId = %u", peerId, connection->identWithQualifier().c_str(),
                pkt.fragments.size(), streamId);
            if (pkt.fragments.size() > 0U) {
                for (auto frag : pkt.fragments) {
                    writePeer(peerId, m_peerId, { NET_FUNC::REPL, NET_SUBFUNC::REPL_RID_LIST }, 
                        frag.second->data, FRAG_SIZE, 0U, streamId);
                    Thread::sleep(60U); // pace block transmission
                }
            }

            pkt.clear();
        }

        return;
    }

    // send radio ID white/black lists
    std::vector<uint32_t> ridWhitelist;
    for (auto entry : m_ridLookup->table()) {
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
            DECLARE_UINT8_ARRAY(payload, bufSize);

            SET_UINT32(listSize, payload, 0U);

            // write whitelisted IDs to whitelist payload
            uint32_t offs = 4U;
            for (uint32_t j = 0; j < listSize; j++) {
                uint32_t id = ridWhitelist.at(j + (i * MAX_RID_LIST_CHUNK));

                if (m_debug)
                    LogDebug(LOG_MASTER, "PEER %u (%s) whitelisting RID %u (%d / %d)", peerId, connection->identWithQualifier().c_str(),
                        id, i, j);

                SET_UINT32(id, payload, offs);
                offs += 4U;
            }

            writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_SUBFUNC_WL_RID },
                payload, bufSize, streamId, true);
        }

        connection->lastPing(now);
    }
}

/* Helper to send the list of whitelisted RIDs to the specified peer. */

void TrafficNetwork::writeBlacklistRIDs(uint32_t peerId, uint32_t streamId)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    // send radio ID blacklist
    std::vector<uint32_t> ridBlacklist;
    for (auto entry : m_ridLookup->table()) {
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
            DECLARE_UINT8_ARRAY(payload, bufSize);

            SET_UINT32(listSize, payload, 0U);

            // write blacklisted IDs to blacklist payload
            uint32_t offs = 4U;
            for (uint32_t j = 0; j < listSize; j++) {
                uint32_t id = ridBlacklist.at(j + (i * MAX_RID_LIST_CHUNK));

                if (m_debug)
                    LogDebug(LOG_MASTER, "PEER %u (%s) blacklisting RID %u (%d / %d)", peerId, connection->identWithQualifier().c_str(),
                        id, i, j);

                SET_UINT32(id, payload, offs);
                offs += 4U;
            }

            writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_SUBFUNC_BL_RID },
                payload, bufSize, streamId, true);
        }

        connection->lastPing(now);
    }
}

/* Helper to send the list of active TGIDs to the specified peer. */

void TrafficNetwork::writeTGIDs(uint32_t peerId, uint32_t streamId, bool sendReplica)
{
    if (!m_tidLookup->sendTalkgroups()) {
        return;
    }

    // sending REPL style TGID list to replica neighbor FNE peers
    if (sendReplica) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            std::string tempFile;
            if (m_isReplica) {
                std::ostringstream s;
                std::random_device rd;
                std::mt19937 mt(rd());
                std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
                s << "/tmp/talkgroup_rules.yml." << dist(mt);

                tempFile = s.str();
                std::string origFile = m_tidLookup->filename();
                m_tidLookup->filename(tempFile);
                m_tidLookup->commit(true);
                m_tidLookup->filename(origFile);
            } else {
                tempFile = m_tidLookup->filename();
            }

            // read entire file into string buffer
            std::stringstream b;
            std::ifstream stream(tempFile);
            if (stream.is_open()) {
                while (stream.peek() != EOF) {
                    b << (char)stream.get();
                }

                stream.close();
            }

            if (m_isReplica)
                ::remove(tempFile.c_str());

            // convert to a byte array
            uint32_t len = b.str().size();
            DECLARE_UINT8_ARRAY(buffer, len);
            ::memcpy(buffer, b.str().data(), len);

            PacketBuffer pkt(true, "Peer Replication, TGID List");
            pkt.encode((uint8_t*)buffer, len);

            LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, TGID List, blocks %u, streamId = %u", peerId, connection->identWithQualifier().c_str(),
                pkt.fragments.size(), streamId);
            if (pkt.fragments.size() > 0U) {
                for (auto frag : pkt.fragments) {
                    writePeer(peerId, m_peerId, { NET_FUNC::REPL, NET_SUBFUNC::REPL_TALKGROUP_LIST }, 
                        frag.second->data, FRAG_SIZE, 0U, streamId);
                    Thread::sleep(60U); // pace block transmission
                }
            }

            pkt.clear();
        }

        return;
    }

    std::vector<std::pair<uint32_t, uint8_t>> tgidList;
    for (auto entry : m_tidLookup->groupVoice()) {
        std::vector<uint32_t> inclusion = entry.config().inclusion();
        std::vector<uint32_t> exclusion = entry.config().exclusion();
        std::vector<uint32_t> preferred = entry.config().preferred();

        // peer inclusion lists take priority over exclusion lists
        if (inclusion.size() > 0) {
            auto it = std::find(inclusion.begin(), inclusion.end(), peerId);
            if (it == inclusion.end()) {
                // LogDebug(LOG_MASTER, "PEER %u TGID %u TS %u -- not included peer", peerId, entry.source().tgId(), entry.source().tgSlot());
                continue;
            }
        }
        else {
            if (exclusion.size() > 0) {
                auto it = std::find(exclusion.begin(), exclusion.end(), peerId);
                if (it != exclusion.end()) {
                    // LogDebug(LOG_MASTER, "PEER %u TGID %u TS %u -- excluded peer", peerId, entry.source().tgId(), entry.source().tgSlot());
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
    DECLARE_UINT8_ARRAY(payload, 4U + (tgidList.size() * 5U));

    SET_UINT32(tgidList.size(), payload, 0U);

    // write talkgroup IDs to active TGID payload
    uint32_t offs = 4U;
    for (std::pair<uint32_t, uint8_t> tg : tgidList) {
        if (m_debug) {
            std::string peerIdentity = resolvePeerIdentity(peerId);
            LogDebug(LOG_MASTER, "PEER %u (%s) activating TGID %u TS %u", peerId, peerIdentity.c_str(),
                tg.first, tg.second);
        }
        SET_UINT32(tg.first, payload, offs);
        payload[offs + 4U] = tg.second;
        offs += 5U;
    }

    writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_SUBFUNC_ACTIVE_TGS },
        payload, 4U + (tgidList.size() * 5U), streamId, true);
}

/* Helper to send the list of deactivated TGIDs to the specified peer. */

void TrafficNetwork::writeDeactiveTGIDs(uint32_t peerId, uint32_t streamId)
{
    if (!m_tidLookup->sendTalkgroups()) {
        return;
    }

    std::vector<std::pair<uint32_t, uint8_t>> tgidList;
    for (auto entry : m_tidLookup->groupVoice()) {
        std::vector<uint32_t> inclusion = entry.config().inclusion();
        std::vector<uint32_t> exclusion = entry.config().exclusion();

        // peer inclusion lists take priority over exclusion lists
        if (inclusion.size() > 0) {
            auto it = std::find(inclusion.begin(), inclusion.end(), peerId);
            if (it == inclusion.end()) {
                // LogDebug(LOG_MASTER, "PEER %u TGID %u TS %u -- not included peer", peerId, entry.source().tgId(), entry.source().tgSlot());
                continue;
            }
        }
        else {
            if (exclusion.size() > 0) {
                auto it = std::find(exclusion.begin(), exclusion.end(), peerId);
                if (it != exclusion.end()) {
                    // LogDebug(LOG_MASTER, "PEER %u TGID %u TS %u -- excluded peer", peerId, entry.source().tgId(), entry.source().tgSlot());
                    continue;
                }
            }
        }

        if (!entry.config().active()) {
            tgidList.push_back({ entry.source().tgId(), entry.source().tgSlot() });
        }
    }

    // build dataset
    DECLARE_UINT8_ARRAY(payload, 4U + (tgidList.size() * 5U));

    SET_UINT32(tgidList.size(), payload, 0U);

    // write talkgroup IDs to deactive TGID payload
    uint32_t offs = 4U;
    for (std::pair<uint32_t, uint8_t> tg : tgidList) {
        if (m_debug) {
            std::string peerIdentity = resolvePeerIdentity(peerId);
            LogDebug(LOG_MASTER, "PEER %u (%s) deactivating TGID %u TS %u", peerId, peerIdentity.c_str(),
                tg.first, tg.second);
        }
        SET_UINT32(tg.first, payload, offs);
        payload[offs + 4U] = tg.second;
        offs += 5U;
    }

    writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_SUBFUNC_DEACTIVE_TGS }, 
        payload, 4U + (tgidList.size() * 5U), streamId, true);
}

/* Helper to send the list of peers to the specified peer. */

void TrafficNetwork::writePeerList(uint32_t peerId, uint32_t streamId)
{
    // sending REPL style PID list to replica neighbor FNE peers
    FNEPeerConnection* connection = m_peers[peerId];
    if (connection != nullptr) {
        std::string tempFile;
        if (m_isReplica) {
            std::ostringstream s;
            std::random_device rd;
            std::mt19937 mt(rd());
            std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
            s << "/tmp/peer_list.dat." << dist(mt);

            tempFile = s.str();
            std::string origFile = m_peerListLookup->filename();
            m_peerListLookup->filename(tempFile);
            m_peerListLookup->commit(true);
            m_peerListLookup->filename(origFile);
        } else {
            tempFile = m_peerListLookup->filename();
        }

        // read entire file into string buffer
        std::stringstream b;
        std::ifstream stream(tempFile);
        if (stream.is_open()) {
            while (stream.peek() != EOF) {
                b << (char)stream.get();
            }

            stream.close();
        }

        if (m_isReplica)
            ::remove(tempFile.c_str());

        // convert to a byte array
        uint32_t len = b.str().size();
        DECLARE_UINT8_ARRAY(buffer, len);
        ::memcpy(buffer, b.str().data(), len);

        PacketBuffer pkt(true, "Peer Replication, PID List");
        pkt.encode((uint8_t*)buffer, len);

        LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, PID List, blocks %u, streamId = %u", peerId, connection->identWithQualifier().c_str(),
            pkt.fragments.size(), streamId);
        if (pkt.fragments.size() > 0U) {
            for (auto frag : pkt.fragments) {
                writePeer(peerId, m_peerId, { NET_FUNC::REPL, NET_SUBFUNC::REPL_PEER_LIST }, 
                    frag.second->data, FRAG_SIZE, 0U, streamId);
                Thread::sleep(60U); // pace block transmission
            }
        }

        pkt.clear();
    }

    return;
}

/* Helper to send the HA parameters to the specified peer. */

void TrafficNetwork::writeHAParameters(uint32_t peerId, uint32_t streamId, bool sendReplica)
{
    if (!m_haEnabled) {
        return;
    }

    uint32_t len = 4U + (m_peerReplicaHAParams.size() * HA_PARAMS_ENTRY_LEN);
    DECLARE_UINT8_ARRAY(buffer, len);

    SET_UINT32((len - 4U), buffer, 0U);

    uint32_t offs = 4U;
    m_peerReplicaHAParams.lock(false);
    for (uint8_t i = 0U; i < m_peerReplicaHAParams.size(); i++) {
        uint32_t peerId = m_peerReplicaHAParams[i].peerId;
        uint32_t ipAddr = m_peerReplicaHAParams[i].masterIP;
        uint16_t port = m_peerReplicaHAParams[i].masterPort;

        SET_UINT32(peerId, buffer, offs);
        SET_UINT32(ipAddr, buffer, offs + 4U);
        SET_UINT16(port, buffer, offs + 8U);

        offs += HA_PARAMS_ENTRY_LEN;
    }
    m_peerReplicaHAParams.unlock();

    // sending REPL style HA parameters list to replica neighbor FNE peers
    if (sendReplica) {
        FNEPeerConnection* connection = m_peers[peerId];
        if (connection != nullptr) {
            LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, HA parameters, streamId = %u", peerId, connection->identWithQualifier().c_str(), streamId);
            writePeer(peerId, m_peerId, { NET_FUNC::REPL, NET_SUBFUNC::REPL_HA_PARAMS}, 
                buffer, len, 0U, streamId);
        }
    }

    writePeerCommand(peerId, { NET_FUNC::MASTER, NET_SUBFUNC::MASTER_HA_PARAMS },
        buffer, len, streamId, true);
}

/* Helper to send a network tree disconnect to the specified peer. */

void TrafficNetwork::writeTreeDisconnect(uint32_t peerId, uint32_t offendingPeerId)
{
    if (!m_enableSpanningTree)
        return;

    if (peerId == 0)
        return;
    if (offendingPeerId == 0U)
        return;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    SET_UINT32(offendingPeerId, buffer, 0U);                                   // Offending Peer ID

    writePeerCommand(peerId, { NET_FUNC::NET_TREE, NET_SUBFUNC::NET_TREE_DISC }, buffer, 4U, RTP_END_OF_CALL_SEQ, createStreamId());
}

/* Helper to send a In-Call Control command to the specified peer. */

bool TrafficNetwork::writePeerICC(uint32_t peerId, uint32_t streamId, NET_SUBFUNC::ENUM subFunc, NET_ICC::ENUM command, uint32_t dstId, uint8_t slotNo,
    bool systemReq, bool toUpstream, uint32_t ssrc)
{
    if (peerId == 0)
        return false;
    if (!m_enableRIDInCallCtrl && !systemReq)
        return false;
    if (dstId == 0U)
        return false;

    if (systemReq && ssrc == 0U)
        ssrc = peerId;

    if (m_debug)
        LogDebugEx(LOG_HOST, "TrafficNetwork::writePeerICC()", "peerId = %u, command = $%02X, subFunc = $%02X, dstId = %u, slot = %u, ssrc = %u, streamId = %u", 
            peerId, command, subFunc, dstId, slotNo, ssrc, streamId);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    if (systemReq) {
        SET_UINT32(ssrc, buffer, 6U);                                           // Peer ID
    } else {
        SET_UINT32(peerId, buffer, 6U);                                         // Peer ID
    }
    buffer[10U] = (uint8_t)command;                                             // In-Call Control Command
    SET_UINT24(dstId, buffer, 11U);                                             // Destination ID
    buffer[14U] = slotNo;                                                       // DMR Slot No

    // are we sending this ICC request upstream?
    if (toUpstream && systemReq) {
        if (m_host->m_peerNetworks.size() > 0U) {
            for (auto& peer : m_host->m_peerNetworks) {
                if (peer.second != nullptr) {
                    if (peer.first == ssrc) {
                        // skip the peer if it is the source peer
                        continue;
                    }

                    if (peer.second->isEnabled()) {
                        LogInfoEx(LOG_MASTER, "PEER %u In-Call Control Request to Upstream, dstId = %u, slot = %u, ssrc = %u, streamId = %u", peerId, dstId, slotNo, ssrc, streamId);
                        peer.second->writeMaster({ NET_FUNC::INCALL_CTRL, subFunc }, buffer, 15U, RTP_END_OF_CALL_SEQ, streamId, false, 0U, ssrc);
                    }
                }
            }
        }

        return true;
    }
    else
        return writePeer(peerId, ssrc, { NET_FUNC::INCALL_CTRL, subFunc }, buffer, 15U, RTP_END_OF_CALL_SEQ, streamId);
}

/*
** Generic Message Writing
*/

/* Helper to send a data message to the specified peer with a explicit packet sequence. */

bool TrafficNetwork::writePeer(uint32_t peerId, uint32_t ssrc, FrameQueue::OpcodePair opcode, const uint8_t* data,
    uint32_t length, uint16_t pktSeq, uint32_t streamId, bool incPktSeq) const
{
    return writePeerQueue(nullptr, peerId, ssrc, opcode, data, length, pktSeq, streamId, incPktSeq);
}

/* Helper to queue a data message to the specified peer with a explicit packet sequence. */

bool TrafficNetwork::writePeerQueue(udp::BufferQueue* buffers, uint32_t peerId, uint32_t ssrc, FrameQueue::OpcodePair opcode, 
    const uint8_t* data, uint32_t length, uint16_t pktSeq, uint32_t streamId, bool incPktSeq) const
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

            if (incPktSeq && pktSeq != RTP_END_OF_CALL_SEQ) {
                pktSeq = connection->incPktSeq(streamId);
            }
#if DEBUG_RTP_MUX
            if (m_debug)
                LogDebugEx(LOG_NET, "TrafficNetwork::writePeerQueue()", "PEER %u, streamId = %u, pktSeq = %u", peerId, streamId, pktSeq);
#endif
            if (m_maskOutboundPeerID)
                ssrc = m_peerId; // mask the source SSRC to our own peer ID
            else {
                if ((connection->isNeighborFNEPeer() && !connection->isReplica()) && m_maskOutboundPeerIDForNonPL) {
                    // if the peer is a downstream FNE neighbor peer, and not a replica peer, we need to send the packet
                    // to the neighbor FNE peer with our peer ID as the source instead of the originating peer
                    // because we have routed it
                    ssrc = m_peerId;
                }

                if (ssrc == 0U) {
                    LogError(LOG_NET, "BUGBUG: PEER %u, trying to send data with a ssrc of 0?, pktSeq = %u, streamId = %u", peerId, pktSeq, streamId);
                    ssrc = m_peerId; // fallback to our own peer ID
                }
            }

            if (buffers == nullptr)
                return m_frameQueue->write(data, length, streamId, peerId, ssrc, opcode, pktSeq, addr, addrLen);
            else {
                m_frameQueue->enqueueMessage(buffers, data, length, streamId, peerId, ssrc, opcode, pktSeq, addr, addrLen);
                return true;
            }
        }
    }

    return false;
}

/* Helper to send a command message to the specified peer. */

bool TrafficNetwork::writePeerCommand(uint32_t peerId, FrameQueue::OpcodePair opcode,
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
    return writePeer(peerId, m_peerId, opcode, buffer, len, RTP_END_OF_CALL_SEQ, streamId, incPktSeq);
}

/* Helper to send a ACK response to the specified peer. */

bool TrafficNetwork::writePeerACK(uint32_t peerId, uint32_t streamId, const uint8_t* data, uint32_t length)
{
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    SET_UINT32(peerId, buffer, 0U);                                           // Peer ID

    if (data != nullptr && length > 0U) {
        ::memcpy(buffer + 6U, data, length);
    }

    return writePeer(peerId, m_peerId, { NET_FUNC::ACK, NET_SUBFUNC::NOP }, buffer, length + 10U, RTP_END_OF_CALL_SEQ, 
        streamId);
}

/* Helper to log a warning specifying which NAK reason is being sent a peer. */

void TrafficNetwork::logPeerNAKReason(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason)
{
    switch (reason) {
    case NET_CONN_NAK_MODE_NOT_ENABLED:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; digital mode not enabled on FNE", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_ILLEGAL_PACKET:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; illegal/unknown packet", peerId ,tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_FNE_UNAUTHORIZED:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; unauthorized", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_BAD_CONN_STATE:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; bad connection state", peerId ,tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_INVALID_CONFIG_DATA:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; invalid configuration data", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_FNE_MAX_CONN:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; FNE has reached maximum permitted connections", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_PEER_RESET:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; FNE demanded connection reset", peerId, tag, (uint16_t)reason);
        break;
    case NET_CONN_NAK_PEER_ACL:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; ACL rejection", peerId, tag, (uint16_t)reason);
        break;

    case NET_CONN_NAK_FNE_DUPLICATE_CONN:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; duplicate connection drop", peerId, tag, (uint16_t)reason);
        break;

    case NET_CONN_NAK_GENERAL_FAILURE:
    default:
        LogWarning(LOG_MASTER, "PEER %u NAK %s, reason = %u; general failure", peerId, tag, (uint16_t)reason);
        break;
    }
}

/* Helper to send a NAK response to the specified peer. */

bool TrafficNetwork::writePeerNAK(uint32_t peerId, uint32_t streamId, const char* tag, NET_CONN_NAK_REASON reason)
{
    if (peerId == 0)
        return false;
    if (tag == nullptr)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    SET_UINT32(peerId, buffer, 6U);                                             // Peer ID
    SET_UINT16((uint16_t)reason, buffer, 10U);                                  // Reason

    logPeerNAKReason(peerId, tag, reason);
    return writePeer(peerId, m_peerId, { NET_FUNC::NAK, NET_SUBFUNC::NOP }, buffer, 12U, RTP_END_OF_CALL_SEQ, streamId);
}

/* Helper to send a NAK response to the specified peer. */

bool TrafficNetwork::writePeerNAK(uint32_t peerId, const char* tag, NET_CONN_NAK_REASON reason, sockaddr_storage& addr, uint32_t addrLen)
{
    if (peerId == 0)
        return false;
    if (tag == nullptr)
        return false;

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    SET_UINT32(peerId, buffer, 6U);                                             // Peer ID
    SET_UINT16((uint16_t)reason, buffer, 10U);                                  // Reason

    logPeerNAKReason(peerId, tag, reason);
    LogWarning(LOG_MASTER, "PEER %u NAK %s -> %s:%u", peerId, tag, udp::Socket::address(addr).c_str(), udp::Socket::port(addr));
    return m_frameQueue->write(buffer, 12U, createStreamId(), peerId, m_peerId,
        { NET_FUNC::NAK, NET_SUBFUNC::NOP }, RTP_END_OF_CALL_SEQ, addr, addrLen);
}

/*
** Internal KMM Callback.
*/

/* Helper to process a FNE KMM TEK response. */

void TrafficNetwork::processTEKResponse(p25::kmm::KeyItem* rspKi, uint8_t algId, uint8_t keyLength)
{
    using namespace p25::defines;
    using namespace p25::kmm;

    if (rspKi == nullptr)
        return;

    LogInfoEx(LOG_PEER, "upstream master enc. key, algId = $%02X, kID = $%04X", algId, rspKi->kId());

    s_keyQueueMutex.lock();

    std::vector<uint32_t> peersToRemove;
    for (auto entry : m_peerReplicaKeyQueue) {
        uint16_t keyId = entry.second;
        if (keyId == rspKi->kId() && algId > 0U) {
            uint32_t peerId = entry.first;

            uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
            ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
            rspKi->getKey(key);

            if (m_debug) {
                LogDebugEx(LOG_HOST, "TrafficNetwork::processTEKResponse()", "keyLength = %u", keyLength);
                Utils::dump(1U, "TrafficNetwork::processTEKResponse(), Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
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

            writePeer(peerId, m_peerId, { NET_FUNC::KEY_RSP, NET_SUBFUNC::NOP }, buffer, modifyKeyRsp.length() + 11U, 
                RTP_END_OF_CALL_SEQ, createStreamId());

            peersToRemove.push_back(peerId);
        }
    }

    // remove peers who were sent keys
    for (auto& peerId : peersToRemove)
        m_peerReplicaKeyQueue.erase(peerId);

    s_keyQueueMutex.unlock();
}
