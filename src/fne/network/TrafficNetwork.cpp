// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "common/p25/kmm/KMMFactory.h"
#include "common/json/json.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Utils.h"
#include "network/TrafficNetwork.h"
#include "network/callhandler/TagDMRData.h"
#include "network/callhandler/TagP25Data.h"
#include "network/callhandler/TagP25P2Data.h"
#include "network/callhandler/TagNXDNData.h"
#include "network/callhandler/TagAnalogData.h"
#include "network/P25OTARService.h"
#include "fne/ActivityLog.h"
#include "HostFNE.h"
#include "FNEMain.h"

using namespace network;
using namespace network::callhandler;

#include <cassert>
#include <chrono>
#include <fstream>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t MAX_PEER_LIST_BEFORE_FLUSH = 10U;
const uint32_t MAX_RID_LIST_CHUNK = 50U;

const uint32_t MAX_MISSED_ACL_UPDATES = 10U;

const uint64_t PACKET_LATE_TIME = 250U; // 250ms

const uint32_t FIXED_HA_UPDATE_INTERVAL = 30U; // 30s

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::timed_mutex TrafficNetwork::s_keyQueueMutex;
std::timed_mutex TrafficNetwork::s_llaKeyQueueMutex;

std::array<std::mutex, PEER_STATE_LOCK_STRIPES> TrafficNetwork::s_peerStateLocks;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the TrafficNetwork class. */

TrafficNetwork::TrafficNetwork(HostFNE* host, const std::string& address, uint16_t port, uint32_t peerId, const std::string& password,
    std::string identity, bool debug, bool kmfDebug, bool verbose, bool reportPeerPing,
    bool dmr, bool p25, bool p25P2, bool nxdn, bool analog,
    uint32_t parrotDelay, bool parrotGrantDemand, bool allowActivityTransfer, bool allowDiagnosticTransfer, 
    uint32_t pingTime, uint32_t updateLookupTime, uint16_t workerCnt) :
    BaseNetwork(peerId, true, debug, true, true, allowActivityTransfer, allowDiagnosticTransfer),
    m_tagDMR(nullptr),
    m_tagP25(nullptr),
    m_tagP25P2(nullptr),
    m_tagNXDN(nullptr),
    m_tagAnalog(nullptr),
    m_p25OTARService(nullptr),
    m_host(host),
    m_address(address),
    m_port(port),
    m_password(password),
    m_encryptedTrafficConn(false),
    m_isReplica(false),
    m_dmrEnabled(dmr),
    m_p25Enabled(p25),
    m_p25P2Enabled(p25P2),
    m_nxdnEnabled(nxdn),
    m_analogEnabled(analog),
    m_parrotDelay(parrotDelay),
    m_parrotDelayTimer(1000U, 0U, parrotDelay),
    m_parrotGrantDemand(parrotGrantDemand),
    m_parrotOnlyOriginating(false),
    m_parrotOverrideSrcId(0U),
    m_kmfServicesEnabled(false),
    m_kmfAllowRID0(false),
    m_kmfEncKeyRequest(false),
    m_kmfPresharedKey(nullptr),
    m_ridLookup(nullptr),
    m_ridAliasLookup(nullptr),
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
    m_peerReplicaLLAKeyQueue(),
    m_globalAff(nullptr),
    m_treeRoot(nullptr),
    m_treeLock(),
    m_peerReplicaHAParams(),
    m_advertisedHAAddress(),
    m_advertisedHAPort(TRAFFIC_DEFAULT_PORT),
    m_haEnabled(false),
    m_maintainenceTimer(1000U, pingTime),
    m_updateLookupTimer(1000U, (updateLookupTime * 60U)),
    m_haUpdateTimer(1000U, FIXED_HA_UPDATE_INTERVAL),
    m_patchStatusRegistry(),
    m_patchStatusEnabled(true),
    m_softConnLimit(0U),
    m_enableSpanningTree(true),
    m_logSpanningTreeChanges(false),
    m_spanningTreeFastReconnect(true),
    m_callCollisionTimeout(5U),
    m_disallowAdjStsBcast(false),
    m_disallowExtAdjStsBcast(true),
    m_disallowRadioMonitor(true),
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
    m_enableMetrics(false),
    m_metricsLogRawData(false),
    m_enableInfluxDB(false),
    m_influxServerAddress("127.0.0.1"),
    m_influxServerPort(8086U),
    m_influxServerToken(),
    m_influxOrg("dvm"),
    m_influxBucket("dvm"),
    m_enableSQLite(false),
    m_sqliteDBFile("metrics.db"),
    m_sqliteDB(nullptr),
    m_sqlitePruneAfterDays(30U),
    m_sqlitePruneIntervalMinutes(60U),
    m_jitterBufferEnabled(false),
    m_jitterMaxSize(4U),
    m_jitterMaxWait(40000U),
    m_threadPool(workerCnt, "fne"),
    m_metadataUpdateThreadPool(workerCnt / 2U, "mupdt"),
    m_metadataUpdateMutex(),
    m_metadataUpdateState(),
    m_disablePacketData(false),
    m_dumpPacketData(false),
    m_verbosePacketData(false),
    m_vtunQueueMaxFrames(128U),
    m_vtunQueueMaxBytes(262144U),
    m_sndcpStartAddr(__IP_FROM_STR("10.10.1.10")),
    m_sndcpEndAddr(__IP_FROM_STR("10.10.1.254")),
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
    m_tagP25P2 = new TagP25P2Data(this, debug);
    m_tagNXDN = new TagNXDNData(this, debug);
    m_tagAnalog = new TagAnalogData(this, debug);

    m_p25OTARService = new P25OTARService(this, m_tagP25->packetData(), kmfDebug, verbose);

    m_globalAff = new fne_lookups::AffiliationLookup("GlobalAffiliations", nullptr, false);

    SpanningTree::s_maxUpdatesBeforeReparent = (uint8_t)host->m_maxMissedPings;
    m_treeRoot = new SpanningTree(peerId, peerId, nullptr);
    m_treeRoot->identity(identity);

    /*
    ** Initialize Threads
    */

#if !defined(CATCH2_TEST_COMPILATION)
    Thread::runAsThread(this, threadParrotHandler);
#endif
}

/* Finalizes a instance of the TrafficNetwork class. */

TrafficNetwork::~TrafficNetwork()
{
    if (m_kmfServicesEnabled) {
        m_p25OTARService->close();
    }

    TrafficNetwork::MetricsLogging::finalize(this);

    delete m_p25OTARService;

    delete m_tagDMR;
    delete m_tagP25;
    delete m_tagP25P2;
    delete m_tagNXDN;
    delete m_tagAnalog;
}

/* Helper to set configuration options. */

void TrafficNetwork::setOptions(yaml::Node& conf, bool printOptions)
{
    m_disallowAdjStsBcast = conf["disallowAdjStsBcast"].as<bool>(false);
    m_disallowExtAdjStsBcast = conf["disallowExtAdjStsBcast"].as<bool>(true);
    m_disallowRadioMonitor = conf["disallowRadioMonitor"].as<bool>(true);
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

    yaml::Node patchStatusConf = conf["patchStatus"];
    m_patchStatusEnabled = patchStatusConf["enabled"].as<bool>(true);
    uint32_t patchStatusDefaultTtl = patchStatusConf["defaultTtlSeconds"].as<uint32_t>(PatchStatusRegistry::DEFAULT_TTL_SECONDS);
    uint32_t patchStatusMinTtl = patchStatusConf["minTtlSeconds"].as<uint32_t>(PatchStatusRegistry::MIN_TTL_SECONDS);
    uint32_t patchStatusMaxTtl = patchStatusConf["maxTtlSeconds"].as<uint32_t>(PatchStatusRegistry::MAX_TTL_SECONDS);
    m_patchStatusRegistry.configure(patchStatusDefaultTtl, patchStatusMinTtl, patchStatusMaxTtl);

    // always force disable ADJ_STS_BCAST to neighbor FNE peers if the all option
    // is enabled
    if (m_disallowAdjStsBcast) {
        m_disallowExtAdjStsBcast = true;
    }

    yaml::Node metricsConf = conf["metrics"];
    m_enableMetrics = metricsConf["enable"].as<bool>(false);
    m_metricsLogRawData = metricsConf["logRawData"].as<bool>(false);

    yaml::Node influxConf = metricsConf["influx"];
    m_enableInfluxDB = influxConf["enable"].as<bool>(false);
    m_influxServerAddress = influxConf["serverAddress"].as<std::string>("127.0.0.1");
    m_influxServerPort = influxConf["serverPort"].as<uint16_t>(8086U);
    m_influxServerToken = influxConf["serverToken"].as<std::string>();
    m_influxOrg = influxConf["org"].as<std::string>("dvm");
    m_influxBucket = influxConf["bucket"].as<std::string>("dvm");
    if (m_enableInfluxDB) {
        m_influxServer = influxdb::ServerInfo(m_influxServerAddress, m_influxServerPort, m_influxOrg, m_influxServerToken, m_influxBucket);
    }

    yaml::Node sqliteConf = metricsConf["sqlite"];
    m_enableSQLite = sqliteConf["enable"].as<bool>(false);
    m_sqliteDBFile = sqliteConf["file"].as<std::string>("metrics.db");
    m_sqlitePruneAfterDays = sqliteConf["pruneAfterDays"].as<uint32_t>(30U);
    m_sqlitePruneIntervalMinutes = sqliteConf["pruneIntervalMinutes"].as<uint32_t>(60U);

    if (m_sqlitePruneAfterDays == 0U) {
        m_sqlitePruneIntervalMinutes = 0U;
    } else if (m_sqlitePruneIntervalMinutes > 0U && m_sqlitePruneIntervalMinutes < 5U) {
        LogWarning(LOG_MASTER, "SQLite prune interval is too low (%u minutes), clamping to 5 minutes.", m_sqlitePruneIntervalMinutes);
        m_sqlitePruneIntervalMinutes = 5U;
    }

    TrafficNetwork::MetricsLogging::initialize(this);

    if (m_enableInfluxDB && m_enableSQLite) {
        LogWarning(LOG_MASTER, "Both InfluxDB and SQLite metrics logging are enabled. This could cause performance penalties, are you sure?");
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
    m_kmfAllowRID0 = conf["kmfAllowRID0"].as<bool>(false);

    // scope is intentional
    {
        bool encrypted = conf["kmfEncKeyRequest"].as<bool>(false);
        std::string key = conf["kmfPresharedKey"].as<std::string>();
        if (!key.empty()) {
            if (key.size() == 32) {
                // bryanb: shhhhhhh....dirty nasty hacks
                key = key.append(key); // since the key is 32 characters (16 hex pairs), double it on itself for 64 characters (32 hex pairs)
                LogWarning(LOG_HOST, "Half-length KMF preshared encryption key detected, doubling key on itself.");
            }

            if (key.size() == 64) {
                if ((key.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
                    const char* keyPtr = key.c_str();
                    m_kmfPresharedKey = new uint8_t[AES_WRAPPED_PCKT_KEY_LEN];
                    ::memset(m_kmfPresharedKey, 0x00U, AES_WRAPPED_PCKT_KEY_LEN);

                    for (uint8_t i = 0; i < AES_WRAPPED_PCKT_KEY_LEN; i++) {
                        char t[4] = {keyPtr[0], keyPtr[1], 0};
                        m_kmfPresharedKey[i] = (uint8_t)::strtoul(t, NULL, 16);
                        keyPtr += 2 * sizeof(char);
                    }
                }
                else {
                    LogWarning(LOG_HOST, "Invalid characters in the KMF preshared encryption key. Encryption disabled.");
                    encrypted = false;
                }
            }
            else {
                LogWarning(LOG_HOST, "Invalid KMF preshared encryption key length, key should be 32 hex pairs, or 64 characters. Encryption disabled.");
                encrypted = false;
            }
        }

        m_kmfEncKeyRequest = encrypted;
    }

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
    m_vtunQueueMaxFrames = 128U;
    m_vtunQueueMaxBytes = 262144U;
    yaml::Node& vtun = conf["vtun"];
    if (vtun.size() > 0U) {
        yaml::Node& queue = vtun["queue"];
        if (queue.size() > 0U) {
            m_vtunQueueMaxFrames = queue["maxFrames"].as<uint32_t>(128U);
            m_vtunQueueMaxBytes = queue["maxBytes"].as<uint32_t>(262144U);

            if (m_vtunQueueMaxFrames == 0U) {
                LogWarning(LOG_MASTER, "VTUN queue maxFrames is 0, clamping to 1");
                m_vtunQueueMaxFrames = 1U;
            }

            if (m_vtunQueueMaxBytes == 0U) {
                LogWarning(LOG_MASTER, "VTUN queue maxBytes is 0, clamping to 512");
                m_vtunQueueMaxBytes = 512U;
            }
        }

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
        LogInfo("    Console Patch Status Enabled: %s", m_patchStatusEnabled ? "yes" : "no");
        if (m_patchStatusEnabled) {
            LogInfo("    Console Patch Status Default TTL: %us", m_patchStatusRegistry.defaultTtlSeconds());
            LogInfo("    Console Patch Status Minimum TTL: %us", m_patchStatusRegistry.minTtlSeconds());
            LogInfo("    Console Patch Status Maximum TTL: %us", m_patchStatusRegistry.maxTtlSeconds());
        }
        LogInfo("    Disable adjacent site broadcasts to any peers: %s", m_disallowAdjStsBcast ? "yes" : "no");
        if (m_disallowAdjStsBcast) {
            LogWarning(LOG_MASTER, "NOTICE: All P25 ADJ_STS_BCAST messages will be blocked and dropped!");
        }
        LogInfo("    Disable Packet Data: %s", m_disablePacketData ? "yes" : "no");
        LogInfo("    Dump Packet Data: %s", m_dumpPacketData ? "yes" : "no");
        LogInfo("    VTUN Queue Max Frames: %u", m_vtunQueueMaxFrames);
        LogInfo("    VTUN Queue Max Bytes: %u", m_vtunQueueMaxBytes);
        LogInfo("    Disable P25 ADJ_STS_BCAST to neighbor peers: %s", m_disallowExtAdjStsBcast ? "yes" : "no");
        LogInfo("    Disable P25 Radio Monitor to any peers: %s", m_disallowRadioMonitor ? "yes" : "no");
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
        LogInfo("    Metrics Reporting Enabled: %s", m_enableMetrics ? "yes" : "no");
        LogInfo("    Metrics Log Raw TSBK/CSBK/RCCH: %s", m_metricsLogRawData ? "yes" : "no");
        LogInfo("    InfluxDB Reporting Enabled: %s", m_enableInfluxDB ? "yes" : "no");
        if (m_enableInfluxDB) {
            LogInfo("    InfluxDB Address: %s", m_influxServerAddress.c_str());
            LogInfo("    InfluxDB Port: %u", m_influxServerPort);
            LogInfo("    InfluxDB Organization: %s", m_influxOrg.c_str());
            LogInfo("    InfluxDB Bucket: %s", m_influxBucket.c_str());
        }
        LogInfo("    SQLite Metrics Logging Enabled: %s", m_enableSQLite ? "yes" : "no");
        if (m_enableSQLite) {
            LogInfo("    SQLite DB File: %s", m_sqliteDBFile.c_str());
            if (m_sqlitePruneAfterDays > 0U) {
                LogInfo("    SQLite Metrics Retention: %u day(s)", m_sqlitePruneAfterDays);
                if (m_sqlitePruneIntervalMinutes > 0U) {
                    LogInfo("    SQLite Metrics Prune Interval: %u minute(s)", m_sqlitePruneIntervalMinutes);
                } else {
                    LogInfo("    SQLite Metrics Prune Interval: startup only");
                }
            } else {
                LogWarning(LOG_MASTER, "SQLite Metrics pruning is disabled. This can result in extremely large database files depending on system traffic.");
            }
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
        LogInfo("    P25 KMF Allow RID 0 Requests: %s", m_kmfAllowRID0 ? "yes" : "no");
        LogInfo("    P25 KMF Peer Request Encrypted: %s", m_kmfEncKeyRequest ? "yes" : "no");
        if (!m_encryptedTrafficConn && !m_kmfEncKeyRequest) {
            LogWarning(LOG_MASTER, "Peers can make key requests, but the encrypted traffic connection is not enabled and KMF requests are not encrypted! Key requests will be sent in the clear.");
        }
        LogInfo("    High Availability Enabled: %s", m_haEnabled ? "yes" : "no");
        if (m_haEnabled) {
            LogInfo("    Advertised HA WAN IP: %s", m_advertisedHAAddress.c_str());
            LogInfo("    Advertised HA WAN Port: %u", m_advertisedHAPort);
        }
    }
}

/* Sets the instances of the Radio ID, Radio Alias, Talkgroup ID, Peer List, and Crypto lookup tables. */

void TrafficNetwork::setLookups(lookups::RadioIdLookup* ridLookup, lookups::RadioAliasLookup* ridAliasLookup, lookups::TalkgroupRulesLookup* tidLookup,
    lookups::PeerListLookup* peerListLookup, CryptoContainer* cryptoLookup, lookups::AdjSiteMapLookup* adjSiteMapLookup)
{
    m_ridLookup = ridLookup;
    m_ridAliasLookup = ridAliasLookup;
    m_tidLookup = tidLookup;
    m_peerListLookup = peerListLookup;
    m_cryptoLookup = cryptoLookup;
    m_adjSiteMapLookup = adjSiteMapLookup;
}

/* Sets endpoint preshared encryption key. */

void TrafficNetwork::setPresharedKey(const uint8_t* presharedKey)
{
    if (presharedKey != nullptr) {
        m_encryptedTrafficConn = true;
    }

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

/* Processes a replicated console patch status update. */

void TrafficNetwork::processReplicatedPatchStatus(uint32_t peerId, json::object obj)
{
    if (!m_patchStatusEnabled)
        return;

    if (!obj["peerId"].is<uint32_t>() || !obj["patches"].is<json::array>())
        return;

    json::object response = json::object();
    std::string errorMessage;
    bool changed = false;
    if (!m_patchStatusRegistry.publish(obj, response, errorMessage, &changed)) {
        LogWarning(LOG_MASTER, "PEER %u invalid replicated patch status payload, %s", peerId, errorMessage.c_str());
        return;
    }

    if (!changed)
        return;

    writePatchStatusToConsoles(response);
    replicatePatchStatus(obj, peerId);
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

        if (m_forceListUpdate && connection != nullptr) {
            peerMetadataUpdate(peer.first);
        }
    }
    m_peers.shared_unlock();

    // reset force flag
    if (m_forceListUpdate) {
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
                if (connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR || connection->isReplica())
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
                            peer.second->setLLAKeyResponseCallback([=](uint32_t srcId, p25::kmm::KeyItem ki, uint8_t keyLength) {
                                processLLAResponse(srcId, &ki, keyLength);
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
        if (m_patchStatusEnabled && m_patchStatusRegistry.cleanupExpired() > 0U)
            writePatchStatusToConsoles(m_patchStatusRegistry.snapshot());
        m_tagDMR->packetData()->cleanupStale();
        m_tagP25->packetData()->cleanupStale();

        MetricsLogging::resetActiveCalls();

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

    m_globalAff->clock(ms);

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

    // start metadata thread pool
    m_metadataUpdateThreadPool.start();

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
        m_frameQueue = new FrameQueue(m_socket, m_peerId, false);
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

    // stop metadata thread pool
    m_metadataUpdateThreadPool.stop();
    m_metadataUpdateThreadPool.wait();

    // scope is intentional
    {
        std::lock_guard<std::mutex> lock(m_metadataUpdateMutex);
        m_metadataUpdateState.clear();
    }

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

                    // if the P25 Phase 2 handler has parrot frames to playback, playback a frame
                    if (fne->m_tagP25P2->hasParrotFrames()) {
                        fne->m_tagP25P2->playbackParrot();
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

                if (!fne->m_tagDMR->hasParrotFrames() && !fne->m_tagP25->hasParrotFrames() && !fne->m_tagP25P2->hasParrotFrames() &&
                    !fne->m_tagNXDN->hasParrotFrames() && !fne->m_tagAnalog->hasParrotFrames() &&
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

                    // if the P25 Phase 2 handler is marked as playing back parrot frames, but has no more frames in the queue
                    // clear the playback flag
                    if (fne->m_tagP25P2->isParrotPlayback() && !fne->m_tagP25P2->hasParrotFrames()) {
                        LogInfoEx(LOG_MASTER, "P25 Phase 2, Parrot Call End, peer = %u, srcId = %u, dstId = %u",
                                   fne->m_tagP25P2->lastParrotPeerId(), fne->m_tagP25P2->lastParrotSrcId(), fne->m_tagP25P2->lastParrotDstId());
                        fne->m_tagP25P2->clearParrotPlayback();
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

/*
** Packet Processing
*/

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

            static const std::unordered_map<uint8_t, PacketHandlerFunc> handlers = {
                { NET_FUNC::PROTOCOL, &TrafficNetwork::PacketHandler::protocol },

                { NET_FUNC::RPTL, &TrafficNetwork::PacketHandler::repeaterLogin },
                { NET_FUNC::RPTK, &TrafficNetwork::PacketHandler::repeaterAuth },
                { NET_FUNC::RPTC, &TrafficNetwork::PacketHandler::repeaterConfig },
                { NET_FUNC::RPT_DISC, &TrafficNetwork::PacketHandler::repeaterDisconnect },

                { NET_FUNC::PING, &TrafficNetwork::PacketHandler::ping },

                { NET_FUNC::GRANT_REQ, &TrafficNetwork::PacketHandler::grantRequest },

                { NET_FUNC::INCALL_CTRL, &TrafficNetwork::PacketHandler::inCallControl },

                { NET_FUNC::KEY_REQ, &TrafficNetwork::PacketHandler::keyRequest },
                { NET_FUNC::KEY_LLA_REQ, &TrafficNetwork::PacketHandler::llaKeyRequest },
            };

            // dispatch to the appropriate handler based on the function opcode
            uint8_t func = req->fneHeader.getFunction();

            if ((func == NET_FUNC::RPTK && !validRepeaterAuthLength(req->length)) ||
                (func == NET_FUNC::RPTC && !validRepeaterConfigLength(req->length))) {
                LogWarning(LOG_MASTER, "PEER %u malformed FNE login packet, func = $%02X, length = %d", peerId, func, req->length);
                if (req->buffer != nullptr)
                    delete[] req->buffer;
                delete req;
                return;
            }

            // bryanb: temporary support to allow announce packets on the traffic port but ultimately
            //  this should be removed and handled like TRANSFER is handled here
            if (func == NET_FUNC::ANNOUNCE) {
                network->m_host->m_mdNetwork->taskNetworkRx(req);
                return; // don't break, return because taskNetworkRx will cleanup req
            }

            auto it = handlers.find(func);
            if (it != handlers.end()) {
                it->second(network, req, peerId, ssrc, streamId, now);
            } else {
                Utils::dump("Unknown opcode from the peer", req->buffer, req->length);
            }
        }

        if (req->buffer != nullptr)
            delete[] req->buffer;
        delete req;
    }
}

/*
** General Helper Functions
*/

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
    std::lock_guard<std::mutex> lock(m_peerAffiliationsMutex);

    auto it = m_peerAffiliations.find(peerId);
    if (it != m_peerAffiliations.end()) {
        m_peerAffiliations.erase(peerId);
    }

    lookups::ChannelLookup* chLookup = new lookups::ChannelLookup();
    std::shared_ptr<fne_lookups::AffiliationLookup> aff(
        new fne_lookups::AffiliationLookup(peerName, chLookup, m_verbose),
        [](fne_lookups::AffiliationLookup* p) {
            if (p != nullptr) {
                lookups::ChannelLookup* rfCh = p->rfCh();
                if (rfCh != nullptr) {
                    delete rfCh;
                }
                delete p;
            }
        });

    aff->setDisableUnitRegTimeout(true); // FNE doesn't allow unit registration timeouts (notification must come from the peers)
    aff->setDisableGrpAffTimeout(true);  // FNE doesn't allow group affiliation timeouts (notification must come from the peers)
    m_peerAffiliations.insert(peerId, aff);
}

/* Helper to erase the peer from the peers affiliations list. */

bool TrafficNetwork::erasePeerAffiliations(uint32_t peerId)
{
    std::lock_guard<std::mutex> lock(m_peerAffiliationsMutex);

    auto it = m_peerAffiliations.find(peerId);
    if (it != m_peerAffiliations.end()) {
        m_peerAffiliations.erase(peerId);
        return true;
    }

    return false;
}

/* Helper to get the peer affiliations entry for a peer. */

std::shared_ptr<fne_lookups::AffiliationLookup> TrafficNetwork::getPeerAffiliations(uint32_t peerId) const
{
    std::lock_guard<std::mutex> lock(m_peerAffiliationsMutex);

    auto it = m_peerAffiliations.find(peerId);
    if (it != m_peerAffiliations.end()) {
        return it->second;
    }

    return nullptr;
}

/* Helper to create a snapshot of all peer affiliation entries. */

std::vector<TrafficNetwork::PeerAffiliationMapPair> TrafficNetwork::peerAffiliationsSnapshot() const
{
    std::vector<TrafficNetwork::PeerAffiliationMapPair> snapshot;

    std::lock_guard<std::mutex> lock(m_peerAffiliationsMutex);
    snapshot.reserve(m_peerAffiliations.size());
    for (auto it = m_peerAffiliations.begin(); it != m_peerAffiliations.end(); ++it) {
        snapshot.push_back(*it);
    }

    return snapshot;
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
            FNEPeerConnection* conn = it->second;
            if (conn != nullptr) {
                neighborFNE = conn->peerClass() == PEER_CONN_CLASS_NEIGHBOR;
            }
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

    // erase any console patch status records for this peer
    if (m_patchStatusEnabled && m_patchStatusRegistry.removePeer(peerId)) {
        json::object clearPatchStatus = json::object();
        uint32_t originFnePeerId = m_peerId;
        uint32_t ttlSeconds = m_patchStatusRegistry.defaultTtlSeconds();
        clearPatchStatus["type"].set<std::string>("publish");
        clearPatchStatus["peerId"].set<uint32_t>(peerId);
        clearPatchStatus["originFnePeerId"].set<uint32_t>(originFnePeerId);
        clearPatchStatus["ttlSeconds"].set<uint32_t>(ttlSeconds);
        clearPatchStatus["patches"].set<json::array>(json::array());
        writePatchStatusToConsoles(m_patchStatusRegistry.snapshot());
        replicatePatchStatus(clearPatchStatus);
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
    std::vector<PeerAffiliationMapPair> affSnapshot = peerAffiliationsSnapshot();
    for (const auto& entry : affSnapshot) {
        std::shared_ptr<fne_lookups::AffiliationLookup> aff = entry.second;
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

/* Helper to send patch status state to one console peer. */

bool TrafficNetwork::writePatchStatusToPeer(uint32_t peerId, json::object obj)
{
    if (peerId == 0U)
        return false;
    if (!m_patchStatusEnabled)
        return false;

    bool ret = false;
    m_peers.shared_lock();
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end() && it->second != nullptr)
        ret = writePatchStatusPayload(it->second, obj);
    m_peers.shared_unlock();

    return ret;
}

/* Helper to broadcast patch status state to connected console peers. */

void TrafficNetwork::writePatchStatusToConsoles(json::object obj, uint32_t exceptPeerId)
{
    if (!m_patchStatusEnabled)
        return;

    m_peers.shared_lock();
    if (m_peers.size() == 0U) {
        m_peers.shared_unlock();
        return;
    }

    for (auto peer : m_peers) {
        if (peer.first == exceptPeerId)
            continue;
        if (peer.second == nullptr)
            continue;
        if (!peer.second->connected() || peer.second->peerClass() != PEER_CONN_CLASS_CONSOLE)
            continue;

        writePatchStatusPayload(peer.second, obj);
    }
    m_peers.shared_unlock();
}

/* Helper to replicate patch status state to neighboring FNE peers. */

void TrafficNetwork::replicatePatchStatus(json::object obj, uint32_t exceptPeerId)
{
    if (!m_patchStatusEnabled)
        return;

    obj["type"].set<std::string>("publish");

    if (m_host->m_peerNetworks.size() > 0U) {
        for (auto peer : m_host->m_peerNetworks) {
            if (peer.first == exceptPeerId)
                continue;
            if (peer.second != nullptr && peer.second->isEnabled() && peer.second->isReplica())
                peer.second->writePatchStatus(obj);
        }
    }

    m_peers.shared_lock();
    for (auto peer : m_peers) {
        if (peer.first == exceptPeerId)
            continue;
        if (peer.second == nullptr)
            continue;
        if (!peer.second->connected() || peer.second->peerClass() != PEER_CONN_CLASS_NEIGHBOR || !peer.second->isReplica())
            continue;

        writePatchStatusReplicationPayload(peer.second, obj);
    }
    m_peers.shared_unlock();
}

/* Helper to serialize and queue a patch status transfer payload. */

bool TrafficNetwork::writePatchStatusPayload(FNEPeerConnection* connection, json::object obj)
{
    if (connection == nullptr)
        return false;
    if (!m_patchStatusEnabled)
        return false;
    if (m_host->m_mdNetwork == nullptr)
        return false;
    if (!connection->connected())
        return false;
    if (connection->peerClass() != PEER_CONN_CLASS_CONSOLE)
        return false;

    obj["type"].set<std::string>("registry");
    json::value v = json::value(obj);
    std::string payload = std::string(v.serialize());
    uint32_t len = static_cast<uint32_t>(payload.length());
    if ((len + 11U) > DATA_PACKET_LENGTH) {
        LogError(LOG_MASTER, "PEER %u (%s) patch status registry payload too large, len = %u", connection->id(), connection->identWithQualifier().c_str(), len);
        return false;
    }

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);
    ::memcpy(buffer + 11U, payload.c_str(), len);

    if (m_debug) {
        LogDebug(LOG_MASTER, "PEER %u (%s) sending patch status registry, len = %u", connection->id(), connection->identWithQualifier().c_str(), len);
    }

    return m_host->m_mdNetwork->writePeerMetadata(connection, m_peerId,
        { NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_PATCH_STATUS }, buffer, len + 11U, RTP_END_OF_CALL_SEQ, createStreamId());
}

/* Helper to serialize and queue a patch status replication payload. */

bool TrafficNetwork::writePatchStatusReplicationPayload(FNEPeerConnection* connection, json::object obj)
{
    if (connection == nullptr)
        return false;
    if (!m_patchStatusEnabled)
        return false;
    if (m_host->m_mdNetwork == nullptr)
        return false;
    if (!connection->connected())
        return false;
    if (connection->peerClass() != PEER_CONN_CLASS_NEIGHBOR || !connection->isReplica())
        return false;

    obj["type"].set<std::string>("publish");
    json::value v = json::value(obj);
    std::string json = std::string(v.serialize());

    size_t len = json.length() + 9U;
    DECLARE_CHAR_ARRAY(buffer, len);

    ::memcpy(buffer + 0U, TAG_PEER_REPLICA, 4U);
    ::snprintf(buffer + 8U, json.length() + 1U, "%s", json.c_str());

    PacketBuffer pkt(true, "Peer Replication, Patch Status");
    pkt.encode((uint8_t*)buffer, len);

    uint32_t streamId = createStreamId();
    LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, Patch Status, blocks %u, streamId = %u", connection->id(),
        connection->identWithQualifier().c_str(), pkt.fragments.size(), streamId);
    if (pkt.fragments.size() > 0U) {
        for (auto frag : pkt.fragments) {
            m_host->m_mdNetwork->writePeerMetadata(connection, m_peerId, { NET_FUNC::REPL, NET_SUBFUNC::REPL_PATCH_STATUS },
                frag.second->data, FRAG_SIZE, RTP_END_OF_CALL_SEQ, streamId);
            Thread::sleep(60U); // pace block transmission
        }
    }

    pkt.clear();
    return true;
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
    m_peers.shared_lock();
    auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](PeerMapPair x) { return x.first == peerId; });
    if (it != m_peers.end()) {
        if (it->second != nullptr) {
            FNEPeerConnection* peer = it->second;
            m_peers.shared_unlock();
            return peer->identWithQualifier();
        }
    }
    m_peers.shared_unlock();

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
    case network::NET_ICC::DMR_RC_CEASE_TRANSMIT:
    case network::NET_ICC::DMR_RC_REQUEST_CEASE_TRANSMIT:
    case network::NET_ICC::DMR_RC_MAXIMUM_POWER:
    case network::NET_ICC::DMR_RC_MINIMUM_POWER:
    case network::NET_ICC::DMR_RC_POWER_INCREASE_ONE_STEP:
    case network::NET_ICC::DMR_RC_POWER_DECREASE_ONE_STEP:
        {
            const bool callTakeover = (command == network::NET_ICC::REJECT_TRAFFIC);

            // is this a local peer?
            if (ssrc > 0 && (m_peers.find(ssrc) != m_peers.end())) {
                FNEPeerConnection* connection = m_peers[ssrc];
                if (connection != nullptr) {
                    // validate peer (simple validation really)
                    if (connection->connected()) {
                        LogInfoEx(LOG_MASTER, "PEER %u In-Call Control Request to Local Peer, dstId = %u, slot = %u, ssrc = %u, streamId = %u", peerId, dstId, slotNo, ssrc, streamId);

                        // send ICC request to local peer
                        writePeerICC(ssrc, streamId, subFunc, command, dstId, slotNo, true);

                        if (callTakeover) {
                            // flag the protocol call handler to allow call takeover on the next audio frame
                            switch (subFunc) {
                            case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:             // Encapsulated DMR data frame
                                m_tagDMR->triggerCallTakeover(dstId);
                                break;

                            case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:             // Encapsulated P25 data frame
                                m_tagP25->triggerCallTakeover(dstId);
                                break;

                            case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2:          // Encapsulated P25 Phase 2 data frame
                                m_tagP25P2->triggerCallTakeover(dstId);
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
                }
            } else {
                // collect target neighbors while holding the peers lock, then send after unlock
                // to avoid lock re-entry via writePeerICC() -> writePeerQueue().
                std::vector<uint32_t> neighborPeers;
                bool localRequestPeer = false;
                m_peers.shared_lock();
                localRequestPeer = (m_peers.find(peerId) != m_peers.end());
                for (auto& peer : m_peers) {
                    if (peer.second == nullptr)
                        continue;
                    if (peerId != peer.first) {
                        FNEPeerConnection* conn = peer.second;
                        if (peerId == ssrc) {
                            // skip the peer if it is the source peer
                            continue;
                        }

                        if (conn->peerClass() == PEER_CONN_CLASS_NEIGHBOR) {
                            neighborPeers.push_back(peer.first);
                        }
                    }
                }
                m_peers.shared_unlock();

                // send ICC request to any peers connected to us that are neighbor FNEs
                for (auto& neighborPeerId : neighborPeers) {
                    LogInfoEx(LOG_MASTER, "PEER %u In-Call Control Request to Neighbors, peerId = %u, dstId = %u, slot = %u, ssrc = %u, streamId = %u", peerId, neighborPeerId, dstId, slotNo, ssrc, streamId);

                    // send ICC request to local peer
                    writePeerICC(neighborPeerId, streamId, subFunc, command, dstId, slotNo, true, false, ssrc);
                }

                if (callTakeover) {
                    // flag the protocol call handler to allow call takeover on the next audio frame
                    switch (subFunc) {
                    case NET_SUBFUNC::PROTOCOL_SUBFUNC_DMR:             // Encapsulated DMR data frame
                        m_tagDMR->triggerCallTakeover(dstId);
                        break;

                    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25:             // Encapsulated P25 data frame
                        m_tagP25->triggerCallTakeover(dstId);
                        break;

                    case NET_SUBFUNC::PROTOCOL_SUBFUNC_P25_P2:          // Encapsulated P25 Phase 2 data frame
                        m_tagP25P2->triggerCallTakeover(dstId);
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

                // send further up the network tree (only if ICC request came from a local peer)
                if (m_host->m_peerNetworks.size() > 0 && localRequestPeer) {
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
    if (peerId == 0U) {
        return;
    }

    bool enqueueTask = false;

    // scope is intentional
    {
        std::lock_guard<std::mutex> lock(m_metadataUpdateMutex);
        MetadataUpdateState& state = m_metadataUpdateState[peerId];

        if (state.inFlight) {
            // coalesce duplicate requests while one update is running
            LogWarning(LOG_MASTER, "PEER %u metadata update already in flight, coalescing duplicate request", peerId);
            state.pending = true;
            return;
        }

        if (state.pending) {
            // a request is already queued for this peer
            LogWarning(LOG_MASTER, "PEER %u metadata update already pending, coalescing duplicate request", peerId);
            return;
        }

        state.pending = true;
        enqueueTask = true;
    }

    if (!enqueueTask) {
        return;
    }

    MetadataUpdateRequest* req = new MetadataUpdateRequest();
    req->obj = this;
    req->peerId = peerId;

    // enqueue the task
    if (!m_metadataUpdateThreadPool.enqueue(new_pooltask(taskMetadataUpdate, req))) {
        LogError(LOG_NET, "Failed to task enqueue metadata update, peerId = %u", peerId);

        // scope is intentional
        {
            std::lock_guard<std::mutex> lock(m_metadataUpdateMutex);
            auto it = m_metadataUpdateState.find(peerId);
            if (it != m_metadataUpdateState.end()) {
                it->second.pending = false;
                if (!it->second.inFlight) {
                    m_metadataUpdateState.erase(it);
                }
            }
        }

        if (req != nullptr) {
            delete req;
        }
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

        while (true) {
            // scope is intentional
            {
                std::lock_guard<std::mutex> lock(network->m_metadataUpdateMutex);

                // check if there is a pending metadata update for this peer
                MetadataUpdateState& state = network->m_metadataUpdateState[req->peerId];
                if (!state.pending) {
                    // no pending metadata update for this peer, exit the loop
                    state.inFlight = false;
                    network->m_metadataUpdateState.erase(req->peerId);
                    break;
                }

                // check if the peer connection is still valid and connected
                FNEPeerConnection* connection = network->m_peers[req->peerId];
                if (connection != nullptr) {
                    if (!connection->connected()) {
                        // peer connection is not connected, skip the metadata update
                        LogWarning(LOG_MASTER, "PEER %u (%s) not connected, skipping metadata update", req->peerId, connection->identWithQualifier().c_str());
                        state.pending = false;
                        state.inFlight = false;
                        network->m_metadataUpdateState.erase(req->peerId);
                        break;
                    }
                } else {
                    // peer connection is not found, skip the metadata update
                    LogWarning(LOG_MASTER, "PEER %u not found, skipping metadata update", req->peerId);
                    state.pending = false;
                    state.inFlight = false;
                    network->m_metadataUpdateState.erase(req->peerId);
                    break;
                }

                state.pending = false;
                state.inFlight = true;
            }

            std::string peerIdentity = network->resolvePeerIdentity(req->peerId);

            FNEPeerConnection* connection = network->m_peers[req->peerId];
            if (connection != nullptr) {
                if (connection->connected()) {
                    connection->lock();
                    uint32_t streamId = network->createStreamId();

                    // if the connection is a downstream neighbor FNE peer, and peer is participating in peer link,
                    // send the peer proper configuration data
                    if (connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR && connection->isReplica()) {
                        LogInfoEx(LOG_MASTER, "PEER %u (%s) sending replica network metadata updates", req->peerId, peerIdentity.c_str());

                        network->writeWhitelistRIDs(req->peerId, streamId, true);
                        network->writeTGIDs(req->peerId, streamId, true);
                        network->writePeerList(req->peerId, streamId);
                        network->writeRadioAliasList(req->peerId, streamId);

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

            // set the $20 bit of the slot number to identify if this TG is strapped or not
            if (entry.config().strapping() == lookups::TG_STRAPPING_STRAPPED) {
                slotNo |= 0x20U;
            }

            // set the $10 bit of the slot number to identify if this TG is clear only
            if (entry.config().strapping() == lookups::TG_STRAPPING_CLEAR) {
                slotNo |= 0x10U;
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
                tg.first, tg.second & 0x03U);
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

/* Helper to send the list of radio aliases to the specified peer. */

void TrafficNetwork::writeRadioAliasList(uint32_t peerId, uint32_t streamId)
{
    // sending REPL style RID alias list to replica neighbor FNE peers
    FNEPeerConnection* connection = m_peers[peerId];
    if (connection != nullptr) {
        std::string tempFile;
        if (m_isReplica) {
            std::ostringstream s;
            std::random_device rd;
            std::mt19937 mt(rd());
            std::uniform_int_distribution<uint32_t> dist(0x00U, 0xFFFFFFFFU);
            s << "/tmp/rid_alias.dat." << dist(mt);

            tempFile = s.str();
            std::string origFile = m_ridAliasLookup->filename();
            m_ridAliasLookup->filename(tempFile);
            m_ridAliasLookup->commit(true);
            m_ridAliasLookup->filename(origFile);
        } else {
            tempFile = m_ridAliasLookup->filename();
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

        PacketBuffer pkt(true, "Peer Replication, RID Alias List");
        pkt.encode((uint8_t*)buffer, len);

        LogInfoEx(LOG_REPL, "PEER %u (%s) Peer Replication, RID Alias List, blocks %u, streamId = %u", peerId, connection->identWithQualifier().c_str(),
            pkt.fragments.size(), streamId);
        if (pkt.fragments.size() > 0U) {
            for (auto frag : pkt.fragments) {
                writePeer(peerId, m_peerId, { NET_FUNC::REPL, NET_SUBFUNC::REPL_RID_ALIAS_LIST }, 
                    frag.second->data, FRAG_SIZE, 0U, streamId);
                Thread::sleep(60U); // pace block transmission
            }
        }

        pkt.clear();
    }

    return;
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
    for (uint32_t i = 0U; i < m_peerReplicaHAParams.size(); i++) {
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
    buffer[14U] = slotNo;                                                       // DMR/P25P2 Slot No

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
                        LogInfoEx(LOG_MASTER, "PEER %u In-Call Control Request to Upstream, command = $%02X, dstId = %u, slot = %u, ssrc = %u, streamId = %u", peerId, command, dstId, slotNo, ssrc, streamId);
                        peer.second->writeMaster({ NET_FUNC::INCALL_CTRL, subFunc }, buffer, 15U, RTP_END_OF_CALL_SEQ, streamId, false, 0U, ssrc);
                    }
                }
            }
        }

        return true;
    }
    else {
        LogInfoEx(LOG_MASTER, "PEER %u In-Call Control Request, command = $%02X, dstId = %u, slot = %u, ssrc = %u, streamId = %u", peerId, command, dstId, slotNo, ssrc, streamId);
        return writePeer(peerId, ssrc, { NET_FUNC::INCALL_CTRL, subFunc }, buffer, 15U, RTP_END_OF_CALL_SEQ, streamId);
    }
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

    m_peers.shared_lock();
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
                if ((connection->peerClass() == PEER_CONN_CLASS_NEIGHBOR && !connection->isReplica()) && m_maskOutboundPeerIDForNonPL) {
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

            if (buffers == nullptr) {
                m_peers.shared_unlock();
                return m_frameQueue->write(data, length, streamId, peerId, ssrc, opcode, pktSeq, addr, addrLen);
            } else {
                m_peers.shared_unlock();
                m_frameQueue->enqueueMessage(buffers, data, length, streamId, peerId, ssrc, opcode, pktSeq, addr, addrLen);
                return true;
            }
        }
    }
    m_peers.shared_unlock();

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
            modifyKeyRsp.setDecryptInfoFmt(m_kmfEncKeyRequest ? KMM_DECRYPT_PEER_ENC : KMM_DECRYPT_INSTRUCT_NONE);
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

/* Helper to process a FNE KMM LLA response. */

void TrafficNetwork::processLLAResponse(uint32_t srcId, p25::kmm::KeyItem* rspKi, uint8_t keyLength)
{
    using namespace p25::defines;
    using namespace p25::kmm;

    if (rspKi == nullptr)
        return;

    LogInfoEx(LOG_PEER, "upstream master LLA enc. key, rsi = %u", srcId);

    s_llaKeyQueueMutex.lock();

    std::vector<uint32_t> peersToRemove;
    for (auto entry : m_peerReplicaLLAKeyQueue) {
        uint32_t requestingRid = entry.second;
        if (requestingRid == srcId) {
            uint32_t peerId = entry.first;

            uint8_t key[P25DEF::MAX_ENC_KEY_LENGTH_BYTES];
            ::memset(key, 0x00U, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
            rspKi->getKey(key);

            if (m_debug) {
                LogDebugEx(LOG_HOST, "TrafficNetwork::processLLAResponse()", "keyLength = %u", keyLength);
                Utils::dump(1U, "TrafficNetwork::processLLAResponse(), Key", key, P25DEF::MAX_ENC_KEY_LENGTH_BYTES);
            }

            // build response buffer
            uint8_t buffer[DATA_PACKET_LENGTH];
            ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

            KMMModifyKey modifyKeyRsp = KMMModifyKey();
            modifyKeyRsp.setDecryptInfoFmt(m_kmfEncKeyRequest ? KMM_DECRYPT_PEER_ENC : KMM_DECRYPT_INSTRUCT_NONE);
            modifyKeyRsp.setAlgId(ALGO_AES_128);
            modifyKeyRsp.setKId(0U);
            modifyKeyRsp.setSrcLLId(WUID_FNE);
            modifyKeyRsp.setDstLLId(srcId);

            KeysetItem ks = KeysetItem();
            ks.keysetId(1U);
            ks.algId(ALGO_AES_128);
            ks.keyLength(keyLength);

            p25::kmm::KeyItem ki = p25::kmm::KeyItem();
            ki.keyFormat(KEY_FORMAT_TEK);
            ki.kId(rspKi->kId());
            ki.sln(rspKi->sln());
            ki.setKey(key, keyLength);

            ks.push_back(ki);
            modifyKeyRsp.setKeysetItem(ks);

            modifyKeyRsp.encode(buffer + 11U);

            writePeer(peerId, m_peerId, { NET_FUNC::KEY_LLA_RSP, NET_SUBFUNC::NOP }, buffer, modifyKeyRsp.length() + 11U, 
                RTP_END_OF_CALL_SEQ, createStreamId());

            peersToRemove.push_back(peerId);
        } else {
            LogError(LOG_PEER, "upstream master LLA enc. key, peerId = %u, requestingRSI = %u, rsi = %u -- mismatch!", entry.first, requestingRid, srcId);
        }
    }

    // remove peers who were sent keys
    for (auto& peerId : peersToRemove)
        m_peerReplicaLLAKeyQueue.erase(peerId);

    s_llaKeyQueueMutex.unlock();
}
