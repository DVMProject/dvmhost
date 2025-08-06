// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2023 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2021 Nat Moore
 *
 */
#include "Defines.h"
#include "common/lookups/RSSIInterpolator.h"
#include "common/network/udp/Socket.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "modem/port/specialized/V24UDPPort.h"
#include "host/Host.h"
#include "ActivityLog.h"
#include "HostMain.h"

using namespace modem;
using namespace lookups;

#include <cstdio>
#include <algorithm>
#include <functional>
#include <memory>

#if !defined(_WIN32)
#include <sys/utsname.h>
#include <unistd.h>
#endif // !defined(_WIN32)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

network::NetRPC* g_RPC = nullptr;

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex Host::m_clockingMutex;

uint8_t Host::m_activeTickDelay = 5U;
uint8_t Host::m_idleTickDelay = 5U;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define CW_IDLE_SLEEP_MS 50U
#define IDLE_WARMUP_MS 5U
#define MAX_OVERFLOW_CNT 10U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Host class. */

Host::Host(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_modem(nullptr),
    m_modemRemote(false),
    m_isModemDFSI(false),
    m_udpDFSIRemotePort(nullptr),
    m_network(nullptr),
    m_modemRemotePort(nullptr),
    m_state(STATE_IDLE),
    m_isTxCW(false),
    m_modeTimer(1000U),
    m_dmrTXTimer(1000U),
    m_cwIdTimer(1000U),
    m_dmrEnabled(false),
    m_p25Enabled(false),
    m_nxdnEnabled(false),
    m_duplex(false),
    m_fixedMode(false),
    m_timeout(180U),
    m_rfModeHang(10U),
    m_rfTalkgroupHang(10U),
    m_netModeHang(3U),
    m_lastDstId(0U),
    m_lastSrcId(0U),
    m_allowStatusTransfer(true),
    m_identity(),
    m_cwCallsign(),
    m_cwIdTime(0U),
    m_latitude(0.0F),
    m_longitude(0.0F),
    m_height(0),
    m_power(0U),
    m_location(),
    m_rxFrequency(0U),
    m_txFrequency(0U),
    m_channelId(0U),
    m_channelNo(0U),
    m_channelLookup(),
    m_voiceChPeerId(),
    m_controlChData(),
    m_idenTable(nullptr),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_dmrBeacons(false),
    m_dmrTSCCData(false),
    m_dmrCtrlChannel(false),
    m_p25CCData(false),
    m_p25CtrlChannel(false),
    m_p25CtrlBroadcast(false),
    m_nxdnCCData(false),
    m_nxdnCtrlChannel(false),
    m_nxdnCtrlBroadcast(false),
    m_presenceTime(120U),
    m_siteId(1U),
    m_sysId(1U),
    m_dmrNetId(1U),
    m_dmrColorCode(1U),
    m_p25NAC(0x293U),
    m_p25NetId(0xBB800U),
    m_p25RfssId(1U),
    m_nxdnRAN(1U),
    m_dmrQueueSizeBytes(3960U), // 24 frames
    m_p25QueueSizeBytes(2592U), // 12 frames
    m_nxdnQueueSizeBytes(1488U), // 31 frames
    m_authoritative(true),
    m_supervisor(false),
    m_dmr1RejCnt(0U),
    m_dmr2RejCnt(0U),
    m_p25RejCnt(0U),
    m_nxdnRejCnt(0U),
    m_dmrBeaconDurationTimer(1000U),
    m_dmrDedicatedTxTestTimer(1000U, 0U, 125U),
    m_dmr1RejectTimer(1000U, 0U, 500U),
    m_dmr2RejectTimer(1000U, 0U, 500U),
    m_p25BcastDurationTimer(1000U),
    m_p25DedicatedTxTestTimer(1000U, 0U, 125U),
    m_p25RejectTimer(1000U, 0U, 500U),
    m_nxdnBcastDurationTimer(1000U),
    m_nxdnDedicatedTxTestTimer(1000U, 0U, 125U),
    m_nxdnRejectTimer(1000U, 0U, 500U),
    m_dmrTx1WatchdogTimer(1000U, 1U),
    m_dmrTx1LoopMS(0U),
    m_dmrTx2WatchdogTimer(1000U, 1U),
    m_dmrTx2LoopMS(0U),
    m_p25TxWatchdogTimer(1000U, 1U),
    m_p25TxLoopMS(0U),
    m_nxdnTxWatchdogTimer(1000U, 1U),
    m_nxdnTxLoopMS(0U),
    m_mainLoopStage(0U),
    m_mainLoopMS(0U),
    m_mainLoopWatchdogTimer(1000U, 1U),
    m_adjSiteLoopMS(0U),
    m_adjSiteLoopWatchdogTimer(1000U, 1U),
    m_dmr1OverflowCnt(0U),
    m_dmr2OverflowCnt(0U),
    m_p25OverflowCnt(0U),
    m_nxdnOverflowCnt(0U),
    m_disableWatchdogOverflow(false),
    m_restAddress("0.0.0.0"),
    m_restPort(REST_API_DEFAULT_PORT),
    m_RESTAPI(nullptr),
    m_rpcAddress("0.0.0.0"),
    m_rpcPort(RPC_DEFAULT_PORT),
    m_dmr(nullptr),
    m_p25(nullptr),
    m_nxdn(nullptr)
{
    /* stub */
}

/* Finalizes a instance of the Host class. */

Host::~Host() = default;

/* Executes the main modem host processing loop. */

int Host::run()
{
    bool ret = false;
    try {
        ret = yaml::Parse(m_conf, m_confFile.c_str());
        if (!ret) {
            ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
        }
    }
    catch (yaml::OperationException const& e) {
        ::fatal("cannot read the configuration file - %s (%s)", m_confFile.c_str(), e.message());
    }

    bool m_daemon = m_conf["daemon"].as<bool>(false);
    if (m_daemon && g_foreground)
        m_daemon = false;

    // initialize system logging
    yaml::Node logConf = m_conf["log"];
    bool useSyslog = logConf["useSyslog"].as<bool>(false);
    g_disableNonAuthoritativeLogging = logConf["disableNonAuthoritiveLogging"].as<bool>(false);
    if (g_foreground)
        useSyslog = false;
    ret = ::LogInitialise(logConf["filePath"].as<std::string>(), logConf["fileRoot"].as<std::string>(),
        logConf["fileLevel"].as<uint32_t>(0U), logConf["displayLevel"].as<uint32_t>(0U), false, useSyslog);
    if (!ret) {
        ::fatal("unable to open the log file\n");
    }

    ret = ::ActivityLogInitialise(logConf["activityFilePath"].as<std::string>(), logConf["fileRoot"].as<std::string>());
    if (!ret) {
        ::fatal("unable to open the activity log file\n");
    }

#if !defined(_WIN32)
    // handle POSIX process forking
    if (m_daemon) {
        // create new process
        pid_t pid = ::fork();
        if (pid == -1) {
            ::fprintf(stderr, "%s: Couldn't fork() , exiting\n", g_progExe.c_str());
            ::LogFinalise();
            ::ActivityLogFinalise();
            return EXIT_FAILURE;
        }
        else if (pid != 0) {
            ::LogFinalise();
            ::ActivityLogFinalise();
            exit(EXIT_SUCCESS);
        }

        // create new session and process group
        if (::setsid() == -1) {
            ::fprintf(stderr, "%s: Couldn't setsid(), exiting\n", g_progExe.c_str());
            ::LogFinalise();
            ::ActivityLogFinalise();
            return EXIT_FAILURE;
        }

        // set the working directory to the root directory
        if (::chdir("/") == -1) {
            ::fprintf(stderr, "%s: Couldn't cd /, exiting\n", g_progExe.c_str());
            ::LogFinalise();
            ::ActivityLogFinalise();
            return EXIT_FAILURE;
        }

        ::close(STDIN_FILENO);
        ::close(STDOUT_FILENO);
        ::close(STDERR_FILENO);
    }
#endif // !defined(_WIN32)

    ::LogInfo(__BANNER__ "\r\n" __PROG_NAME__ " " __VER__ " (built " __BUILD__ ")\r\n" \
        "Copyright (c) 2017-2025 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
        "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\r\n" \
        ">> Modem Controller\r\n");

    // read base parameters from configuration
    ret = readParams();
    if (!ret)
        return EXIT_FAILURE;

    // initialize modem
    ret = createModem();
    if (!ret)
        return EXIT_FAILURE;

    // is the modem slaved to a remote DVM host?
    if (m_modemRemote) {
        ::LogInfoEx(LOG_HOST, "[ OK ] Host is running in remote modem mode");

        StopWatch stopWatch;
        stopWatch.start();

        bool killed = false;

        // main execution loop
        while (!killed) {
            if (m_modem->hasLockout() && m_state != HOST_STATE_LOCKOUT)
                setState(HOST_STATE_LOCKOUT);
            else if (!m_modem->hasLockout() && m_state == HOST_STATE_LOCKOUT)
                setState(STATE_IDLE);

            if (m_modem->hasError() && m_state != HOST_STATE_ERROR)
                setState(HOST_STATE_ERROR);
            else if (!m_modem->hasError() && m_state == HOST_STATE_ERROR)
                setState(STATE_IDLE);

            uint32_t ms = stopWatch.elapsed();
            if (ms > 1U)
                m_modem->clock(ms);

            // ------------------------------------------------------
            //  -- Modem Clocking                                 --
            // ------------------------------------------------------

            ms = stopWatch.elapsed();
            stopWatch.start();

            m_modem->clock(ms);

            if (g_killed) {
                if (!m_modem->hasTX()) {
                    killed = true;
                }
            }

            m_modeTimer.clock(ms);

            if (ms < 2U)
                Thread::sleep(1U);
        }

        setState(HOST_STATE_QUIT);

        return EXIT_SUCCESS;
    }

    yaml::Node systemConf = m_conf["system"];

    // try to load radio IDs table
    std::string ridLookupFile = systemConf["radio_id"]["file"].as<std::string>();
    uint32_t ridReloadTime = systemConf["radio_id"]["time"].as<uint32_t>(0U);
    bool ridAcl = systemConf["radio_id"]["acl"].as<bool>(false);

    LogInfo("Radio Id Lookups");
    LogInfo("    File: %s", ridLookupFile.length() > 0U ? ridLookupFile.c_str() : "None");
    if (ridReloadTime > 0U)
        LogInfo("    Reload: %u mins", ridReloadTime);
    LogInfo("    ACL: %s", ridAcl ? "yes" : "no");

    m_ridLookup = new RadioIdLookup(ridLookupFile, ridReloadTime, ridAcl);
    m_ridLookup->read();

    // try to load talkgroup IDs table
    std::string tidLookupFile = systemConf["talkgroup_id"]["file"].as<std::string>();
    uint32_t tidReloadTime = systemConf["talkgroup_id"]["time"].as<uint32_t>(0U);
    bool tidAcl = systemConf["talkgroup_id"]["acl"].as<bool>(false);

    LogInfo("Talkgroup Rule Lookups");
    LogInfo("    File: %s", tidLookupFile.length() > 0U ? tidLookupFile.c_str() : "None");
    if (tidReloadTime > 0U)
        LogInfo("    Reload: %u mins", tidReloadTime);
    LogInfo("    ACL: %s", tidAcl ? "yes" : "no");

    m_tidLookup = new TalkgroupRulesLookup(tidLookupFile, tidReloadTime, tidAcl);
    m_tidLookup->read();

    // initialize networking
    ret = createNetwork();
    if (!ret)
        return EXIT_FAILURE;

    // set CW parameters
    if (systemConf["cwId"]["enable"].as<bool>(false)) {
        uint32_t time = systemConf["cwId"]["time"].as<uint32_t>(10U);
        m_cwCallsign = systemConf["cwId"]["callsign"].as<std::string>();

        LogInfo("CW Id Parameters");
        LogInfo("    Time: %u mins", time);
        LogInfo("    Callsign: %s", m_cwCallsign.c_str());

        m_cwIdTime = time * 60U;

        m_cwIdTimer.setTimeout(m_cwIdTime / 2U);
        m_cwIdTimer.start();
    }

    // for all modes we handle RSSI
    std::string rssiMappingFile = systemConf["modem"]["rssiMappingFile"].as<std::string>();

    RSSIInterpolator* rssi = new RSSIInterpolator;
    if (!rssiMappingFile.empty()) {
        LogInfo("RSSI");
        LogInfo("    Mapping File: %s", rssiMappingFile.c_str());
        rssi->load(rssiMappingFile);
    }

    yaml::Node protocolConf = m_conf["protocols"];

    StopWatch stopWatch;
    stopWatch.start();

    // initialize DMR
    Timer dmrBeaconIntervalTimer(1000U);
    LogInfo("DMR Parameters");
    LogInfo("    Enabled: %s", m_dmrEnabled ? "yes" : "no");
    if (m_dmrEnabled) {
        yaml::Node dmrProtocol = protocolConf["dmr"];
        m_dmrBeacons = dmrProtocol["beacons"]["enable"].as<bool>(false);
        m_dmrTSCCData = dmrProtocol["control"]["enable"].as<bool>(false);
        bool dmrCtrlChannel = dmrProtocol["control"]["dedicated"].as<bool>(false);
        bool embeddedLCOnly = dmrProtocol["embeddedLCOnly"].as<bool>(false);
        bool dmrDumpDataPacket = dmrProtocol["dumpDataPacket"].as<bool>(false);
        bool dmrRepeatDataPacket = dmrProtocol["repeatDataPacket"].as<bool>(true);
        bool dmrDumpCsbkData = dmrProtocol["dumpCsbkData"].as<bool>(false);
        bool dumpTAData = dmrProtocol["dumpTAData"].as<bool>(true);
        uint32_t callHang = dmrProtocol["callHang"].as<uint32_t>(3U);
        uint32_t txHang = dmrProtocol["txHang"].as<uint32_t>(4U);
        bool dmrVerbose = dmrProtocol["verbose"].as<bool>(true);
        bool dmrDebug = dmrProtocol["debug"].as<bool>(false);

        uint32_t jitter = m_conf["network"]["jitter"].as<uint32_t>(360U);

        if (txHang > m_rfModeHang)
            txHang = m_rfModeHang;
        if (txHang > m_netModeHang)
            txHang = m_netModeHang;

        if (callHang > txHang)
            callHang = txHang;

        LogInfo("    Embedded LC Only: %s", embeddedLCOnly ? "yes" : "no");
        LogInfo("    Dump Talker Alias Data: %s", dumpTAData ? "yes" : "no");
        LogInfo("    Dump Packet Data: %s", dmrDumpDataPacket ? "yes" : "no");
        LogInfo("    Repeat Packet Data: %s", dmrRepeatDataPacket ? "yes" : "no");
        LogInfo("    Dump CSBK Data: %s", dmrDumpCsbkData ? "yes" : "no");
        LogInfo("    Call Hang: %us", callHang);
        LogInfo("    TX Hang: %us", txHang);

        // forcibly enable beacons when TSCC is enabled but not in dedicated mode
        if (m_dmrTSCCData && !dmrCtrlChannel && !m_dmrBeacons) {
            m_dmrBeacons = true;
        }

        LogInfo("    Roaming Beacons: %s", m_dmrBeacons ? "yes" : "no");
        if (m_dmrBeacons) {
            uint32_t dmrBeaconInterval = dmrProtocol["beacons"]["interval"].as<uint32_t>(60U);
            uint32_t dmrBeaconDuration = dmrProtocol["beacons"]["duration"].as<uint32_t>(3U);

            LogInfo("    Roaming Beacon Interval: %us", dmrBeaconInterval);
            LogInfo("    Roaming Beacon Duration: %us", dmrBeaconDuration);

            m_dmrBeaconDurationTimer.setTimeout(dmrBeaconDuration);

            dmrBeaconIntervalTimer.setTimeout(dmrBeaconInterval);
            dmrBeaconIntervalTimer.start();

            g_fireDMRBeacon = true;
        }

        LogInfo("    TSCC Control: %s", m_dmrTSCCData ? "yes" : "no");

        if (m_dmrTSCCData) {
            LogInfo("    TSCC Control Channel: %s", dmrCtrlChannel ? "yes" : "no");
            if (dmrCtrlChannel) {
                m_dmrCtrlChannel = dmrCtrlChannel;
            }

            g_fireDMRBeacon = true;
        }

        m_dmr = std::make_unique<dmr::Control>(m_authoritative, m_dmrColorCode, callHang, m_dmrQueueSizeBytes,
            embeddedLCOnly, dumpTAData, m_timeout, m_rfTalkgroupHang, m_modem, m_network, m_duplex, m_channelLookup, m_ridLookup, m_tidLookup, 
            m_idenTable, rssi, jitter, dmrDumpDataPacket, dmrRepeatDataPacket, dmrDumpCsbkData, dmrDebug, dmrVerbose);
        m_dmr->setOptions(m_conf, m_supervisor, m_controlChData, m_dmrNetId, m_siteId, m_channelId, 
            m_channelNo, true);

        if (dmrCtrlChannel) {
            m_dmr->setCCRunning(true);
        }

        m_dmrTXTimer.setTimeout(txHang);

        if (dmrVerbose) {
            LogInfo("    Verbose: yes");
        }
        if (dmrDebug) {
            LogInfo("    Debug: yes");
        }
    }

    // initialize P25
    Timer p25BcastIntervalTimer(1000U);
    LogInfo("P25 Parameters");
    LogInfo("    Enabled: %s", m_p25Enabled ? "yes" : "no");
    if (m_p25Enabled) {
        yaml::Node p25Protocol = protocolConf["p25"];
        uint32_t tduPreambleCount = p25Protocol["tduPreambleCount"].as<uint32_t>(8U);
        m_p25CCData = p25Protocol["control"]["enable"].as<bool>(false);
        bool p25CtrlChannel = p25Protocol["control"]["dedicated"].as<bool>(false);
        bool p25CtrlBroadcast = p25Protocol["control"]["broadcast"].as<bool>(true);
        bool p25DumpDataPacket = p25Protocol["dumpDataPacket"].as<bool>(false);
        bool p25RepeatDataPacket = p25Protocol["repeatDataPacket"].as<bool>(true);
        bool p25DumpTsbkData = p25Protocol["dumpTsbkData"].as<bool>(false);
        uint32_t callHang = p25Protocol["callHang"].as<uint32_t>(3U);
        bool p25Verbose = p25Protocol["verbose"].as<bool>(true);
        bool p25Debug = p25Protocol["debug"].as<bool>(false);

        LogInfo("    TDU Preamble before Voice/Data: %u", tduPreambleCount);
        LogInfo("    Dump Packet Data: %s", p25DumpDataPacket ? "yes" : "no");
        LogInfo("    Repeat Packet Data: %s", p25RepeatDataPacket ? "yes" : "no");
        LogInfo("    Dump TSBK Data: %s", p25DumpTsbkData ? "yes" : "no");
        LogInfo("    Call Hang: %us", callHang);

        LogInfo("    Control: %s", m_p25CCData ? "yes" : "no");

        uint32_t p25ControlBcstInterval = p25Protocol["control"]["interval"].as<uint32_t>(300U);
        uint32_t p25ControlBcstDuration = p25Protocol["control"]["duration"].as<uint32_t>(1U);
        if (m_p25CCData) {
            LogInfo("    Control Broadcast: %s", p25CtrlBroadcast ? "yes" : "no");
            LogInfo("    Control Channel: %s", p25CtrlChannel ? "yes" : "no");
            if (p25CtrlChannel) {
                m_p25CtrlChannel = p25CtrlChannel;
            }
            else {
                LogInfo("    Control Broadcast Interval: %us", p25ControlBcstInterval);
                LogInfo("    Control Broadcast Duration: %us", p25ControlBcstDuration);
            }

            m_p25BcastDurationTimer.setTimeout(p25ControlBcstDuration);

            p25BcastIntervalTimer.setTimeout(p25ControlBcstInterval);
            p25BcastIntervalTimer.start();

            m_p25CtrlBroadcast = p25CtrlBroadcast;
            if (p25CtrlBroadcast) {
                g_fireP25Control = true;
            }
        }

        m_p25 = std::make_unique<p25::Control>(m_authoritative, m_p25NAC, callHang, m_p25QueueSizeBytes, m_modem,
            m_network, m_timeout, m_rfTalkgroupHang, m_duplex, m_channelLookup, m_ridLookup, m_tidLookup, m_idenTable, rssi, p25DumpDataPacket, 
            p25RepeatDataPacket, p25DumpTsbkData, p25Debug, p25Verbose);
        m_p25->setOptions(m_conf, m_supervisor, m_cwCallsign, m_controlChData,
            m_p25NetId, m_sysId, m_p25RfssId, m_siteId, m_channelId, m_channelNo, true);

        if (p25CtrlChannel) {
            m_p25->setCCRunning(true);
        }

        if (p25Verbose) {
            LogInfo("    Verbose: yes");
        }
        if (p25Debug) {
            LogInfo("    Debug: yes");
        }
    }

    // initialize NXDN
    Timer nxdnBcastIntervalTimer(1000U);
    LogInfo("NXDN Parameters");
    LogInfo("    Enabled: %s", m_nxdnEnabled ? "yes" : "no");
    if (m_nxdnEnabled) {
        yaml::Node nxdnProtocol = protocolConf["nxdn"];
        m_nxdnCCData = nxdnProtocol["control"]["enable"].as<bool>(false);
        bool nxdnCtrlChannel = nxdnProtocol["control"]["dedicated"].as<bool>(false);
        bool nxdnCtrlBroadcast = nxdnProtocol["control"]["broadcast"].as<bool>(true);
        bool nxdnDumpRcchData = nxdnProtocol["dumpRcchData"].as<bool>(false);
        uint32_t callHang = nxdnProtocol["callHang"].as<uint32_t>(3U);
        bool nxdnVerbose = nxdnProtocol["verbose"].as<bool>(true);
        bool nxdnDebug = nxdnProtocol["debug"].as<bool>(false);

        LogInfo("    Call Hang: %us", callHang);

        LogInfo("    Control: %s", m_nxdnCCData ? "yes" : "no");

        uint32_t nxdnControlBcstInterval = nxdnProtocol["control"]["interval"].as<uint32_t>(300U);
        uint32_t nxdnControlBcstDuration = nxdnProtocol["control"]["duration"].as<uint32_t>(1U);
        if (m_nxdnCCData) {
            LogInfo("    Control Broadcast: %s", nxdnCtrlBroadcast ? "yes" : "no");
            LogInfo("    Control Channel: %s", nxdnCtrlChannel ? "yes" : "no");
            if (nxdnCtrlChannel) {
                m_nxdnCtrlChannel = nxdnCtrlChannel;
            }
            else {
                LogInfo("    Control Broadcast Interval: %us", nxdnControlBcstInterval);
                LogInfo("    Control Broadcast Duration: %us", nxdnControlBcstDuration);
            }

            m_nxdnBcastDurationTimer.setTimeout(nxdnControlBcstDuration);

            nxdnBcastIntervalTimer.setTimeout(nxdnControlBcstInterval);
            nxdnBcastIntervalTimer.start();

            m_nxdnCtrlBroadcast = nxdnCtrlBroadcast;
            if (nxdnCtrlBroadcast) {
                g_fireNXDNControl = true;
            }
        }

        m_nxdn = std::make_unique<nxdn::Control>(m_authoritative, m_nxdnRAN, callHang, m_nxdnQueueSizeBytes,
            m_timeout, m_rfTalkgroupHang, m_modem, m_network, m_duplex, m_channelLookup, m_ridLookup, m_tidLookup, m_idenTable, rssi, 
            nxdnDumpRcchData, nxdnDebug, nxdnVerbose);
        m_nxdn->setOptions(m_conf, m_supervisor, m_cwCallsign, m_controlChData, m_siteId, 
            m_sysId, m_channelId, m_channelNo, true);

        if (nxdnCtrlChannel) {
            m_nxdn->setCCRunning(true);
        }

        if (nxdnVerbose) {
            LogInfo("    Verbose: yes");
        }
        if (nxdnDebug) {
            LogInfo("    Debug: yes");
        }
    }

    /*
    ** Error Condition Checking
    */

    if (!m_dmrEnabled && !m_p25Enabled && !m_nxdnEnabled) {
        ::LogError(LOG_HOST, "No protocols enabled? DMR, P25 and/or NXDN must be enabled!");
        g_killed = true;
    }

    if (m_fixedMode && ((m_dmrEnabled && m_p25Enabled) || (m_dmrEnabled && m_nxdnEnabled) || (m_p25Enabled && m_nxdnEnabled))) {
        ::LogError(LOG_HOST, "Cannot have DMR, P25 and NXDN when using fixed state! Choose one protocol for fixed state operation.");
        g_killed = true;
    }

    if (m_isModemDFSI && m_dmrEnabled) {
        ::LogError(LOG_HOST, "Cannot use V.24/DFSI modem with DMR protocol!");
        g_killed = true;
    }

    if (m_isModemDFSI && m_nxdnEnabled) {
        ::LogError(LOG_HOST, "Cannot use V.24/DFSI modem with NXDN protocol!");
        g_killed = true;
    }

    // P25 CC checks
    if (m_dmrEnabled && m_p25CtrlChannel) {
        ::LogError(LOG_HOST, "Cannot have DMR enabled when using dedicated P25 control!");
        g_killed = true;
    }

    if (m_nxdnEnabled && m_p25CtrlChannel) {
        ::LogError(LOG_HOST, "Cannot have NXDN enabled when using dedicated P25 control!");
        g_killed = true;
    }

    if (!m_fixedMode && m_p25CtrlChannel) {
        ::LogWarning(LOG_HOST, "Fixed mode should be enabled when using dedicated P25 control!");
    }

    if (!m_duplex && m_p25CCData) {
        ::LogError(LOG_HOST, "Cannot have P25 control and simplex mode at the same time.");
        g_killed = true;
    }

    // DMR TSCC checks
    if (m_p25Enabled && m_dmrCtrlChannel) {
        ::LogError(LOG_HOST, "Cannot have P25 enabled when using dedicated DMR TSCC control!");
        g_killed = true;
    }

    if (m_nxdnEnabled && m_dmrCtrlChannel) {
        ::LogError(LOG_HOST, "Cannot have NXDN enabled when using dedicated DMR TSCC control!");
        g_killed = true;
    }

    if (!m_fixedMode && m_dmrCtrlChannel) {
        ::LogWarning(LOG_HOST, "Fixed mode should be enabled when using dedicated DMR TSCC control!");
    }

    if (!m_duplex && m_dmrTSCCData) {
        ::LogError(LOG_HOST, "Cannot have DMR TSCC control and simplex mode at the same time.");
        g_killed = true;
    }

    // NXDN CC checks
    if (m_dmrEnabled && m_nxdnCtrlChannel) {
        ::LogError(LOG_HOST, "Cannot have DMR enabled when using dedicated NXDN control!");
        g_killed = true;
    }

    if (m_p25Enabled && m_nxdnCtrlChannel) {
        ::LogError(LOG_HOST, "Cannot have P25 enabled when using dedicated NXDN control!");
        g_killed = true;
    }

    if (!m_fixedMode && m_nxdnCtrlChannel) {
        ::LogWarning(LOG_HOST, "Fixed mode should be enabled when using dedicated NXDN control!");
    }

    if (!m_duplex && m_nxdnCCData) {
        ::LogError(LOG_HOST, "Cannot have NXDN control and simplex mode at the same time.");
        g_killed = true;
    }

    // DMR beacon checks
    if (m_dmrBeacons && m_p25CCData) {
        ::LogError(LOG_HOST, "Cannot have DMR roaming becaons and P25 control at the same time.");
        g_killed = true;
    }

    if (!m_duplex && m_dmrBeacons) {
        ::LogError(LOG_HOST, "Cannot have DMR roaming beacons and simplex mode at the same time.");
        g_killed = true;
    }

    bool killed = false;

    if (!g_killed) {
        // fixed mode will force a state change
        if (m_fixedMode) {
            if (m_dmr != nullptr)
                setState(STATE_DMR);

            if (m_p25 != nullptr)
                setState(STATE_P25);

            if (m_nxdn != nullptr)
                setState(STATE_NXDN);
        }
        else {
            if (m_dmrCtrlChannel) {
                m_fixedMode = true;
                setState(STATE_DMR);
            }

            if (m_p25CtrlChannel) {
                m_fixedMode = true;
                setState(STATE_P25);
            }

            if (m_isModemDFSI) {
                m_fixedMode = true;
                setState(STATE_P25);
            }

            if (m_nxdnCtrlChannel) {
                m_fixedMode = true;
                setState(STATE_NXDN);
            }

            setState(STATE_IDLE);
        }

        if (m_RESTAPI != nullptr) {
            m_RESTAPI->setProtocols(m_dmr.get(), m_p25.get(), m_nxdn.get());
        }

        ::LogInfoEx(LOG_HOST, "[WAIT] Host is performing late initialization and warmup");

        m_modem->clearNXDNFrame();
        m_modem->clearP25Frame();
        m_modem->clearDMRFrame2();
        m_modem->clearDMRFrame1();

        // perform early pumping of the modem clock (this is so the DSP has time to setup its buffers),
        // and clock the network (so it may perform early connect)
        uint32_t elapsedMs = 0U;
        while (!g_killed) {
            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            elapsedMs += ms;
            m_modem->clock(ms);

            if (m_network != nullptr)
                m_network->clock(ms);

            if (m_p25 != nullptr)
                m_p25->reset();
            if (m_nxdn != nullptr)
                m_nxdn->reset();

            Thread::sleep(IDLE_WARMUP_MS);

            if (elapsedMs > 15000U)
                break;
        }

        // check if the modem is a hotspot (this check must always be done after late init)
        if (m_modem->isHotspot())
        {
            if ((m_dmrEnabled && m_p25Enabled) ||
                (m_nxdnEnabled && m_dmrEnabled) ||
                (m_nxdnEnabled && m_p25Enabled)) {
                ::LogError(LOG_HOST, "Multi-mode (DMR, P25 and NXDN) is not supported for hotspots!");
                g_killed = true;
                killed = true;
            }
            else {
                if (!m_fixedMode) {
                    ::LogInfoEx(LOG_HOST, "Host is running on a hotspot modem! Fixed mode is forced.");
                    m_fixedMode = true;
                }
            }
        }

        stopWatch.start();
    } else {
        return EXIT_SUCCESS;
    }

    bool hasTxShutdown = false;

    // Macro to start DMR duplex idle transmission (or beacon)
    #define START_DMR_DUPLEX_IDLE(x)                                                                                    \
        if (m_dmr != nullptr) {                                                                                         \
            if (m_duplex) {                                                                                             \
                m_modem->writeDMRStart(x);                                                                              \
                m_dmrTXTimer.start();                                                                                   \
            }                                                                                                           \
        }

    /*
    ** Initialize Threads
    */

    /** RPC */
    if (!Thread::runAsThread(this, threadRPC))
        return EXIT_FAILURE;

    /** Watchdog */
    if (!Thread::runAsThread(this, threadWatchdog))
        return EXIT_FAILURE;

    /** Modem */
    if (!Thread::runAsThread(this, threadModem))
        return EXIT_FAILURE;

    /** Digital Mobile Radio Frame Processor */
    if (m_dmr != nullptr) {
        if (!Thread::runAsThread(this, threadDMRReader1))
            return EXIT_FAILURE;
        if (!Thread::runAsThread(this, threadDMRWriter1))
            return EXIT_FAILURE;
        if (!Thread::runAsThread(this, threadDMRReader2))
            return EXIT_FAILURE;
        if (!Thread::runAsThread(this, threadDMRWriter2))
            return EXIT_FAILURE;
    }

    /** Project 25 Frame Processor */
    if (m_p25 != nullptr) {
        if (!Thread::runAsThread(this, threadP25Reader))
            return EXIT_FAILURE;
        if (!Thread::runAsThread(this, threadP25Writer))
            return EXIT_FAILURE;
    }

    /** Next Generation Digital Narrowband Frame Processor */
    if (m_nxdn != nullptr) {
        if (!Thread::runAsThread(this, threadNXDNReader))
            return EXIT_FAILURE;
        if (!Thread::runAsThread(this, threadNXDNWriter))
            return EXIT_FAILURE;
    }

    /** Adjacent Site and Affiliation Update */
    if (!Thread::runAsThread(this, threadSiteData))
        return EXIT_FAILURE;

    /** Network Presence Notification */
    {
        if (!m_controlChData.address().empty() && m_controlChData.port() != 0 && m_network != nullptr) {
            if (!Thread::runAsThread(this, threadPresence))
                return EXIT_FAILURE;
        }

        if (m_dmrCtrlChannel || m_p25CtrlChannel || m_nxdnCtrlChannel) {
            if (!Thread::runAsThread(this, threadPresence))
                return EXIT_FAILURE;
        }
    }

    /*
    ** Main execution loop
    */
#if defined(_WIN32)
    ::LogInfoEx(LOG_HOST, "[ OK ] Host is up and running on Win32");
#else
    struct utsname utsinfo;
    ::memset(&utsinfo, 0, sizeof(utsinfo));
    ::uname(&utsinfo);

    ::LogInfoEx(LOG_HOST, "[ OK ] Host is up and running on %s %s %s", utsinfo.sysname, utsinfo.release, utsinfo.machine);
#endif // defined(_WIN32)
    while (!killed) {
        if (m_modem->hasLockout() && m_state != HOST_STATE_LOCKOUT)
            setState(HOST_STATE_LOCKOUT);
        else if (!m_modem->hasLockout() && m_state == HOST_STATE_LOCKOUT)
            setState(STATE_IDLE);

        if (m_modem->hasError() && m_state != HOST_STATE_ERROR)
            setState(HOST_STATE_ERROR);
        else if (!m_modem->hasError() && m_state == HOST_STATE_ERROR)
            setState(STATE_IDLE);

        uint32_t ms = stopWatch.elapsed();
        stopWatch.start();

        if (!m_modem->hasError()) {
            if (!m_fixedMode) {
                if (m_modeTimer.isRunning() && m_modeTimer.hasExpired()) {
                    setState(STATE_IDLE);
                }
            }
            else {
                m_modeTimer.stop();
                if (m_dmr != nullptr && m_state != STATE_DMR && !m_modem->hasTX()) {
                    LogDebug(LOG_HOST, "fixed mode state abnormal, m_state = %u, state = %u", m_state, STATE_DMR);
                    setState(STATE_DMR);
                }
                if (m_p25 != nullptr && m_state != STATE_P25 && !m_modem->hasTX()) {
                    LogDebug(LOG_HOST, "fixed mode state abnormal, m_state = %u, state = %u", m_state, STATE_P25);
                    setState(STATE_P25);
                }
                if (m_nxdn != nullptr && m_state != STATE_NXDN && !m_modem->hasTX()) {
                    LogDebug(LOG_HOST, "fixed mode state abnormal, m_state = %u, state = %u", m_state, STATE_NXDN);
                    setState(STATE_NXDN);
                }
            }
        }

        m_mainLoopWatchdogTimer.start();
        m_mainLoopStage = 0U; // intentional magic number
        m_mainLoopMS = ms;

        // ------------------------------------------------------
        //  -- Network, DMR, and P25 Clocking                 --
        // ------------------------------------------------------

        if (m_network != nullptr) {
            m_mainLoopStage = 5U; // intentional magic number
            m_network->clock(ms);
        }

        if (m_dmr != nullptr) {
            m_mainLoopStage = 6U; // intentional magic number
            m_dmr->clock();

            if (m_dmrCtrlChannel) {
                if (!m_dmrDedicatedTxTestTimer.isRunning()) {
                    m_dmrDedicatedTxTestTimer.start();
                } else {
                    m_dmrDedicatedTxTestTimer.clock(ms);
                }
            }
        }

        if (m_p25 != nullptr) {
            m_mainLoopStage = 7U; // intentional magic number
            m_p25->clock();

            if (m_p25CtrlChannel) {
                if (!m_p25DedicatedTxTestTimer.isRunning()) {
                    m_p25DedicatedTxTestTimer.start();
                } else {
                    m_p25DedicatedTxTestTimer.clock(ms);
                }
            }
        }

        if (m_nxdn != nullptr) {
            m_mainLoopStage = 8U; // intentional magic number
            m_nxdn->clock();

            if (m_nxdnCtrlChannel) {
                if (!m_nxdnDedicatedTxTestTimer.isRunning()) {
                    m_nxdnDedicatedTxTestTimer.start();
                } else {
                    m_nxdnDedicatedTxTestTimer.clock(ms);
                }
            }
        }

        if (m_udpDFSIRemotePort != nullptr) {
            m_mainLoopStage = 11U; // intentional magic number
            modem::port::specialized::V24UDPPort* udpPort = dynamic_cast<modem::port::specialized::V24UDPPort*>(m_udpDFSIRemotePort);
            udpPort->clock(ms);
        }

        // ------------------------------------------------------
        //  -- Timer Clocking                                 --
        // ------------------------------------------------------

        // clock and check CW timer
        m_cwIdTimer.clock(ms);
        if (m_cwIdTimer.isRunning() && m_cwIdTimer.hasExpired()) {
            m_mainLoopStage = 9U; // intentional magic number
            if (!m_modem->hasTX() && !m_p25CtrlChannel && !m_dmrCtrlChannel && !m_nxdnCtrlChannel) {
                if (m_dmrBeaconDurationTimer.isRunning() || m_p25BcastDurationTimer.isRunning() ||
                    m_nxdnBcastDurationTimer.isRunning()) {
                    LogDebug(LOG_HOST, "CW, beacon or CC timer running, ceasing");

                    m_dmrBeaconDurationTimer.stop();
                    m_p25BcastDurationTimer.stop();
                    m_nxdnBcastDurationTimer.stop();
                }

                LogMessage(LOG_HOST, "CW, start transmitting");
                m_isTxCW = true;
                std::lock_guard<std::mutex> lock(m_clockingMutex);

                setState(STATE_IDLE);
                m_modem->sendCWId(m_cwCallsign);

                Thread::sleep(CW_IDLE_SLEEP_MS);

                bool first = true;
                do {
                    // ------------------------------------------------------
                    //  -- Modem Clocking                                 --
                    // ------------------------------------------------------

                    ms = stopWatch.elapsed();
                    stopWatch.start();

                    m_mainLoopWatchdogTimer.start();
                    m_mainLoopMS = ms;

                    m_modem->clock(ms);

                    if (!first && !m_modem->hasTX()) {
                        LogMessage(LOG_HOST, "CW, finished transmitting");
                        break;
                    }

                    if (first) {
                        first = false;
                        Thread::sleep(200U + CW_IDLE_SLEEP_MS); // ~250ms; poll time of the modem
                    }
                    else
                        Thread::sleep(CW_IDLE_SLEEP_MS);
                } while (true);

                m_isTxCW = false;
                m_cwIdTimer.setTimeout(m_cwIdTime);
                m_cwIdTimer.start();
            }
        }

        m_mainLoopStage = 10U; // intentional magic number

        /** Digial Mobile Radio */
        if (m_dmr != nullptr) {
            if (m_dmrTSCCData && m_dmrCtrlChannel) {
                if (m_state != STATE_DMR)
                    setState(STATE_DMR);

                if (!m_modem->hasTX()) {
                    START_DMR_DUPLEX_IDLE(true);
                }
            }

            // clock and check DMR roaming beacon interval timer
            dmrBeaconIntervalTimer.clock(ms);
            if ((dmrBeaconIntervalTimer.isRunning() && dmrBeaconIntervalTimer.hasExpired()) || g_fireDMRBeacon) {
                if ((m_state == STATE_IDLE || m_state == STATE_DMR) && !m_modem->hasTX()) {
                    if (m_modeTimer.isRunning()) {
                        m_modeTimer.stop();
                    }

                    if (m_state != STATE_DMR)
                        setState(STATE_DMR);

                    if (m_fixedMode) {
                        START_DMR_DUPLEX_IDLE(true);
                    }

                    if (m_dmrTSCCData) {
                        m_dmr->setCCRunning(true);
                    }

                    g_fireDMRBeacon = false;
                    if (m_dmrTSCCData) {
                        LogMessage(LOG_HOST, "DMR, start CC broadcast");
                    }
                    else {
                        LogMessage(LOG_HOST, "DMR, roaming beacon burst");
                    }
                    dmrBeaconIntervalTimer.start();
                    m_dmrBeaconDurationTimer.start();
                }
            }

            // clock and check DMR roaming beacon duration timer
            m_dmrBeaconDurationTimer.clock(ms);
            if (m_dmrBeaconDurationTimer.isRunning() && m_dmrBeaconDurationTimer.hasExpired()) {
                m_dmrBeaconDurationTimer.stop();

                if (!m_fixedMode) {
                    if (m_state == STATE_DMR && !m_modeTimer.isRunning()) {
                        m_modeTimer.setTimeout(m_rfModeHang);
                        m_modeTimer.start();
                    }
                }

                if (m_dmrTSCCData) {
                    m_dmr->setCCRunning(false);
                }
            }

            // clock and check DMR Tx timer
            if (!m_dmrCtrlChannel) {
                m_dmrTXTimer.clock(ms);
                if (m_dmrTXTimer.isRunning() && m_dmrTXTimer.hasExpired()) {
                    m_modem->writeDMRStart(false);
                    m_modem->clearDMRFrame1();
                    m_modem->clearDMRFrame2();
                    m_dmrTXTimer.stop();
                }
            } else {
                m_dmrTXTimer.stop();
            }
        }

        /** Project 25 */
        if (m_p25 != nullptr) {
            if (m_p25CCData) {
                p25BcastIntervalTimer.clock(ms);

                if (!m_p25CtrlChannel && m_p25CtrlBroadcast) {
                    // clock and check P25 CC broadcast interval timer
                    if ((p25BcastIntervalTimer.isRunning() && p25BcastIntervalTimer.hasExpired()) || g_fireP25Control) {
                        if ((m_state == STATE_IDLE || m_state == STATE_P25) && !m_modem->hasTX()) {
                            if (m_modeTimer.isRunning()) {
                                m_modeTimer.stop();
                            }

                            if (m_state != STATE_P25)
                                setState(STATE_P25);

                            m_p25->setCCRunning(true);

                            // hide this message for continuous CC -- otherwise display every time we process
                            if (!m_p25CtrlChannel) {
                                LogMessage(LOG_HOST, "P25, start CC broadcast");
                            }

                            g_fireP25Control = false;
                            p25BcastIntervalTimer.start();
                            m_p25BcastDurationTimer.start();

                            // if the CC is continuous -- clock one cycle into the duration timer
                            if (m_p25CtrlChannel) {
                                m_p25BcastDurationTimer.clock(ms);
                            }
                        }
                    }

                    if (m_p25BcastDurationTimer.isPaused()) {
                        m_p25BcastDurationTimer.resume();
                    }

                    // clock and check P25 CC broadcast duration timer
                    m_p25BcastDurationTimer.clock(ms);
                    if (m_p25BcastDurationTimer.isRunning() && m_p25BcastDurationTimer.hasExpired()) {
                        m_p25BcastDurationTimer.stop();

                        m_p25->setCCRunning(false);

                        if (m_state == STATE_P25 && !m_modeTimer.isRunning()) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            m_modeTimer.start();
                        }
                    }
                }
            }
        }

        /** Next Generation Digital Narrowband */
        if (m_nxdn != nullptr) {
            if (m_nxdnCCData) {
                nxdnBcastIntervalTimer.clock(ms);

                if (!m_nxdnCtrlChannel && m_nxdnCtrlBroadcast) {
                    // clock and check NXDN CC broadcast interval timer
                    if ((nxdnBcastIntervalTimer.isRunning() && nxdnBcastIntervalTimer.hasExpired()) || g_fireNXDNControl) {
                        if ((m_state == STATE_IDLE || m_state == STATE_NXDN) && !m_modem->hasTX()) {
                            if (m_modeTimer.isRunning()) {
                                m_modeTimer.stop();
                            }

                            if (m_state != STATE_NXDN)
                                setState(STATE_NXDN);

                            m_nxdn->setCCRunning(true);

                            // hide this message for continuous CC -- otherwise display every time we process
                            if (!m_nxdnCtrlChannel) {
                                LogMessage(LOG_HOST, "NXDN, start CC broadcast");
                            }

                            g_fireNXDNControl = false;
                            nxdnBcastIntervalTimer.start();
                            m_nxdnBcastDurationTimer.start();

                            // if the CC is continuous -- clock one cycle into the duration timer
                            if (m_nxdnCtrlChannel) {
                                m_nxdnBcastDurationTimer.clock(ms);
                            }
                        }
                    }

                    if (m_nxdnBcastDurationTimer.isPaused()) {
                        m_nxdnBcastDurationTimer.resume();
                    }

                    // clock and check NXDN CC broadcast duration timer
                    m_nxdnBcastDurationTimer.clock(ms);
                    if (m_nxdnBcastDurationTimer.isRunning() && m_nxdnBcastDurationTimer.hasExpired()) {
                        m_nxdnBcastDurationTimer.stop();

                        m_nxdn->setCCRunning(false);

                        if (m_state == STATE_NXDN && !m_modeTimer.isRunning()) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            m_modeTimer.start();
                        }
                    }
                }
                else {
                    // simply use the NXDN CC interval timer in a non-broadcast state to transmit adjacent site data over
                    // the network
                    if (nxdnBcastIntervalTimer.isRunning() && nxdnBcastIntervalTimer.hasExpired()) {
                        if ((m_state == STATE_IDLE || m_state == STATE_NXDN) && !m_modem->hasTX()) {
                            nxdnBcastIntervalTimer.start();
                        }
                    }
                }
            }
        }

        if (g_killed) {
            if (m_dmr != nullptr) {
                if (m_state == STATE_DMR && m_duplex && m_modem->hasTX()) {
                    m_modem->writeDMRStart(false);
                    m_dmrTXTimer.stop();
                }

                if (m_dmrCtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearDMRFrame1();
                        m_modem->clearDMRFrame2();
                    }

                    m_dmr->setCCRunning(false);
                    m_dmr->setCCHalted(true);

                    m_dmrBeaconDurationTimer.stop();
                    dmrBeaconIntervalTimer.stop();
                }
            }

            if (m_p25 != nullptr) {
                if (m_p25CtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearP25Frame();
                        m_p25->reset();
                    }

                    m_p25->setCCRunning(false);

                    m_p25BcastDurationTimer.stop();
                    p25BcastIntervalTimer.stop();
                }
            }

            if (m_nxdn != nullptr) {
                if (m_nxdnCtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearNXDNFrame();
                        m_nxdn->reset();
                    }

                    m_nxdn->setCCRunning(false);

                    m_nxdnBcastDurationTimer.stop();
                    nxdnBcastIntervalTimer.stop();
                }
            }

            hasTxShutdown = true;
            killed = true;
        }

        m_modeTimer.clock(ms);

        if ((m_state != STATE_IDLE) && ms <= m_activeTickDelay)
            Thread::sleep(m_activeTickDelay);
        if (m_state == STATE_IDLE)
            Thread::sleep(m_idleTickDelay);
    }

    if (rssi != nullptr) {
        delete rssi;
    }

    setState(HOST_STATE_QUIT);
    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to generate the status of the host in JSON format. */

json::object Host::getStatus()
{
    json::object response = json::object();

    yaml::Node systemConf = m_conf["system"];
    yaml::Node networkConf = m_conf["network"];
    {
        response["state"].set<uint8_t>(m_state);

        response["isTxCW"].set<bool>(m_isTxCW);

        response["fixedMode"].set<bool>(m_fixedMode);

        response["dmrTSCCEnable"].set<bool>(m_dmrTSCCData);
        response["dmrCC"].set<bool>(m_dmrCtrlChannel);
        response["p25CtrlEnable"].set<bool>(m_p25CCData);
        response["p25CC"].set<bool>(m_p25CtrlChannel);
        response["nxdnCtrlEnable"].set<bool>(m_nxdnCCData);
        response["nxdnCC"].set<bool>(m_nxdnCtrlChannel);

        yaml::Node p25Protocol = m_conf["protocols"]["p25"];
        yaml::Node nxdnProtocol = m_conf["protocols"]["nxdn"];

        response["tx"].set<bool>(m_modem->m_tx);

        response["channelId"].set<uint8_t>(m_channelId);
        response["channelNo"].set<uint32_t>(m_channelNo);

        response["lastDstId"].set<uint32_t>(m_lastDstId);
        response["lastSrcId"].set<uint32_t>(m_lastSrcId);

        uint32_t peerId = networkConf["id"].as<uint32_t>();
        response["peerId"].set<uint32_t>(peerId);

        response["sysId"].set<uint32_t>(m_sysId);
        response["siteId"].set<uint8_t>(m_siteId);
        response["p25RfssId"].set<uint8_t>(m_p25RfssId);
        response["p25NetId"].set<uint32_t>(m_p25NetId);
        response["p25NAC"].set<uint32_t>(m_p25NAC);
    }

    json::array vcChannels = json::array();
    bool _true = true;
    bool _false = false;

    if (m_channelLookup->rfChDataSize() > 0) {
        for (auto entry : m_channelLookup->rfChDataTable()) {
            json::object chData = json::object();

            uint32_t chNo = entry.first;
            chData["channelNo"].set<uint32_t>(chNo);

            uint8_t chId = entry.second.chId();
            chData["channelId"].set<uint8_t>(chId);

            uint32_t dstId = 0U, srcId = 0U;

            // fetch affiliations from DMR if we're a DMR CC
            if (m_dmrTSCCData && m_dmr->affiliations() != nullptr) {
                if (m_dmr->affiliations()->isChBusy(chNo)) {
                    chData["tx"].set<bool>(_true);
                } else {
                    chData["tx"].set<bool>(_false);
                }

                dstId = m_dmr->affiliations()->getGrantedDstByCh(chNo);
                if (dstId > 0U)
                    srcId = m_dmr->affiliations()->getGrantedSrcId(dstId);
            }

            // fetch affiliations from P25 if we're a P25 CC
            if (m_p25CCData && m_p25->affiliations() != nullptr) {
                if (m_p25->affiliations()->isChBusy(chNo)) {
                    chData["tx"].set<bool>(_true);
                } else {
                    chData["tx"].set<bool>(_false);
                }

                dstId = m_p25->affiliations()->getGrantedDstByCh(chNo);
                if (dstId > 0U)
                    srcId = m_p25->affiliations()->getGrantedSrcId(dstId);
            }

            // fetch affiliations from NXDN if we're a NXDN CC
            if (m_nxdnCCData && m_nxdn->affiliations() != nullptr) {
                if (m_nxdn->affiliations()->isChBusy(chNo)) {
                    chData["tx"].set<bool>(_true);
                } else {
                    chData["tx"].set<bool>(_false);
                }

                dstId = m_nxdn->affiliations()->getGrantedDstByCh(chNo);
                if (dstId > 0U)
                    srcId = m_nxdn->affiliations()->getGrantedSrcId(dstId);
            }

            chData["lastDstId"].set<uint32_t>(dstId);
            chData["lastSrcId"].set<uint32_t>(srcId);

            vcChannels.push_back(json::value(chData));
        }
    }
    response["vcChannels"].set<json::array>(vcChannels);

    yaml::Node modemConfig = m_conf["system"]["modem"];
    {
        json::object modemInfo = json::object();
        std::string portType = modemConfig["protocol"]["type"].as<std::string>();
        modemInfo["portType"].set<std::string>(portType);

        yaml::Node uartConfig = modemConfig["protocol"]["uart"];
        std::string modemPort = uartConfig["port"].as<std::string>();
        modemInfo["modemPort"].set<std::string>(modemPort);
        uint32_t portSpeed = uartConfig["speed"].as<uint32_t>(115200U);
        modemInfo["portSpeed"].set<uint32_t>(portSpeed);

        if (!m_modem->isHotspot()) {
            modemInfo["pttInvert"].set<bool>(m_modem->m_pttInvert);
            modemInfo["rxInvert"].set<bool>(m_modem->m_rxInvert);
            modemInfo["txInvert"].set<bool>(m_modem->m_txInvert);
            modemInfo["dcBlocker"].set<bool>(m_modem->m_dcBlocker);
        }

        modemInfo["rxLevel"].set<float>(m_modem->m_rxLevel);
        modemInfo["cwTxLevel"].set<float>(m_modem->m_cwIdTXLevel);
        modemInfo["dmrTxLevel"].set<float>(m_modem->m_dmrTXLevel);
        modemInfo["p25TxLevel"].set<float>(m_modem->m_p25TXLevel);
        modemInfo["nxdnTxLevel"].set<float>(m_modem->m_nxdnTXLevel);

        modemInfo["rxDCOffset"].set<int>(m_modem->m_rxDCOffset);
        modemInfo["txDCOffset"].set<int>(m_modem->m_txDCOffset);

        if (!m_modem->isHotspot()) {
            modemInfo["dmrSymLevel3Adj"].set<int>(m_modem->m_dmrSymLevel3Adj);
            modemInfo["dmrSymLevel1Adj"].set<int>(m_modem->m_dmrSymLevel1Adj);
            modemInfo["p25SymLevel3Adj"].set<int>(m_modem->m_p25SymLevel3Adj);
            modemInfo["p25SymLevel1Adj"].set<int>(m_modem->m_p25SymLevel1Adj);

            // are we on a protocol version 3 firmware?
            if (m_modem->getVersion() >= 3U) {
                modemInfo["nxdnSymLevel3Adj"].set<int>(m_modem->m_nxdnSymLevel3Adj);
                modemInfo["nxdnSymLevel1Adj"].set<int>(m_modem->m_nxdnSymLevel1Adj);
            }
        }

        if (m_modem->isHotspot()) {
            modemInfo["dmrDiscBW"].set<int8_t>(m_modem->m_dmrDiscBWAdj);
            modemInfo["dmrPostBW"].set<int8_t>(m_modem->m_dmrPostBWAdj);
            modemInfo["p25DiscBW"].set<int8_t>(m_modem->m_p25DiscBWAdj);
            modemInfo["p25PostBW"].set<int8_t>(m_modem->m_p25PostBWAdj);

            // are we on a protocol version 3 firmware?
            if (m_modem->getVersion() >= 3U) {
                modemInfo["nxdnDiscBW"].set<int8_t>(m_modem->m_nxdnDiscBWAdj);
                modemInfo["nxdnPostBW"].set<int8_t>(m_modem->m_nxdnPostBWAdj);

                modemInfo["afcEnabled"].set<bool>(m_modem->m_afcEnable);
                modemInfo["afcKI"].set<uint8_t>(m_modem->m_afcKI);
                modemInfo["afcKP"].set<uint8_t>(m_modem->m_afcKP);
                modemInfo["afcRange"].set<uint8_t>(m_modem->m_afcRange);
            }

            switch (m_modem->m_adfGainMode) {
                case ADF_GAIN_AUTO_LIN:
                    modemInfo["gainMode"].set<std::string>("ADF7021 Gain Mode: Auto High Linearity");
                break;
                case ADF_GAIN_LOW:
                    modemInfo["gainMode"].set<std::string>("ADF7021 Gain Mode: Low");
                break;
                case ADF_GAIN_HIGH:
                    modemInfo["gainMode"].set<std::string>("ADF7021 Gain Mode: High");
                break;
                case ADF_GAIN_AUTO:
                default:
                    modemInfo["gainMode"].set<std::string>("ADF7021 Gain Mode: Auto");
                break;
            }
        }

        modemInfo["fdmaPreambles"].set<uint8_t>(m_modem->m_fdmaPreamble);
        modemInfo["dmrRxDelay"].set<uint8_t>(m_modem->m_dmrRxDelay);
        modemInfo["p25CorrCount"].set<uint8_t>(m_modem->m_p25CorrCount);

        modemInfo["rxFrequency"].set<uint32_t>(m_modem->m_rxFrequency);
        modemInfo["txFrequency"].set<uint32_t>(m_modem->m_txFrequency);
        modemInfo["rxTuning"].set<int>(m_modem->m_rxTuning);
        modemInfo["txTuning"].set<int>(m_modem->m_txTuning);
        uint32_t rxFreqEffective = m_modem->m_rxFrequency + m_modem->m_rxTuning;
        modemInfo["rxFrequencyEffective"].set<uint32_t>(rxFreqEffective);
        uint32_t txFreqEffective = m_modem->m_txFrequency + m_modem->m_txTuning;
        modemInfo["txFrequencyEffective"].set<uint32_t>(txFreqEffective);

        modemInfo["v24Connected"].set<bool>(m_modem->m_v24Connected);

        uint8_t protoVer = m_modem->getVersion();
        modemInfo["protoVer"].set<uint8_t>(protoVer);

        response["modem"].set<json::object>(modemInfo);
    }

    return response;
}

/* Modem port open callback. */

bool Host::rmtPortModemOpen(Modem* modem)
{
    assert(m_modemRemotePort != nullptr);

    bool ret = m_modemRemotePort->open();
    if (!ret)
        return false;

    LogMessage(LOG_MODEM, "Modem Ready [Remote Mode]");

    // handled modem open
    return true;
}

/* Modem port close callback. */

bool Host::rmtPortModemClose(Modem* modem)
{
    assert(m_modemRemotePort != nullptr);

    m_modemRemotePort->close();

    // handled modem close
    return true;
}

/* Modem clock callback. */

bool Host::rmtPortModemHandler(Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len)
{
    assert(m_modemRemotePort != nullptr);

    if (rspType == RTM_OK && len > 0U) {
        if (modem->getTrace())
            Utils::dump(1U, "Host::rmtPortModemHandler(), TX Remote Data", buffer, len);

        // never send less then 3 bytes
        if (len < 3U)
            return false;

        // send entire modem packet over the remote port
        m_modemRemotePort->write(buffer, len);
    }

    // read any data from the remote port for the air interface
    uint8_t data[BUFFER_LENGTH];
    ::memset(data, 0x00U, BUFFER_LENGTH);

    uint32_t ret = m_modemRemotePort->read(data, BUFFER_LENGTH);
    if (ret > 0) {
        if (modem->getTrace())
            Utils::dump(1U, "Host::rmtPortModemHandler(), RX Remote Data", (uint8_t*)data, ret);

        if (ret < 3U) {
            LogError(LOG_MODEM, "Illegal length of remote data must be >3 bytes");
            Utils::dump("Host::rmtPortModemHandler(), data", data, ret);

            // handled modem response
            return true;
        }

        uint32_t pktLength = 0U;
        switch (data[0U]) {
            case DVM_SHORT_FRAME_START:
                pktLength = data[1U];
                break;
            case DVM_LONG_FRAME_START:
                pktLength = ((data[1U] & 0xFFU) << 8) + (data[2U] & 0xFFU);
                break;
            default:
                LogError(LOG_MODEM, "Invalid start of modem frame!");
                break;
        }

        if (pktLength > 0U) {
            int ret = modem->write(data, pktLength);
            if (ret != int(pktLength))
                LogError(LOG_MODEM, "Error writing remote data");
        }
    }

    // handled modem response
    return true;
}

/* Helper to set the host/modem running state. */

void Host::setState(uint8_t state)
{
    assert(m_modem != nullptr);

    //if (m_state != state) {
    //    LogDebug(LOG_HOST, "setState, m_state = %u, state = %u", m_state, state);
    //}

    switch (state) {
        case STATE_DMR:
            m_modem->setState(STATE_DMR);
            m_state = STATE_DMR;
            m_modeTimer.start();
            //m_cwIdTimer.stop();
            break;

        case STATE_P25:
            m_modem->setState(STATE_P25);
            m_state = STATE_P25;
            m_modeTimer.start();
            //m_cwIdTimer.stop();
            break;

        case STATE_NXDN:
            m_modem->setState(STATE_NXDN);
            m_state = STATE_NXDN;
            m_modeTimer.start();
            //m_cwIdTimer.stop();
            break;

        case HOST_STATE_LOCKOUT:
            LogWarning(LOG_HOST, "Mode change, HOST_STATE_LOCKOUT");
            if (m_network != nullptr)
                m_network->enable(false);

            if (m_state == STATE_DMR && m_duplex && m_modem->hasTX()) {
                m_modem->writeDMRStart(false);
                m_dmrTXTimer.stop();
            }

            m_modem->setState(STATE_IDLE);
            m_state = HOST_STATE_LOCKOUT;
            m_modeTimer.stop();
            //m_cwIdTimer.stop();
            break;

        case HOST_STATE_ERROR:
            LogWarning(LOG_HOST, "Mode change, HOST_STATE_ERROR");
            if (m_network != nullptr)
                m_network->enable(false);

            if (m_state == STATE_DMR && m_duplex && m_modem->hasTX()) {
                m_modem->writeDMRStart(false);
                m_dmrTXTimer.stop();
            }

            m_state = HOST_STATE_ERROR;
            m_modeTimer.stop();
            m_cwIdTimer.stop();
            break;

        default:
            if (m_network != nullptr)
                m_network->enable(true);

            if (m_state == STATE_DMR && m_duplex && m_modem->hasTX()) {
                m_modem->writeDMRStart(false);
                m_dmrTXTimer.stop();
            }

            m_modem->setState(STATE_IDLE);

            m_modem->clearDMRFrame1();
            m_modem->clearDMRFrame2();
            m_modem->clearP25Frame();

            if (m_state == HOST_STATE_ERROR) {
                m_modem->sendCWId(m_cwCallsign);

                m_cwIdTimer.setTimeout(m_cwIdTime);
                m_cwIdTimer.start();
            }

            m_modeTimer.stop();

            if (state == HOST_STATE_QUIT) {
                ::LogInfoEx(LOG_HOST, "Host is shutting down");

                if (m_udpDFSIRemotePort != nullptr) {
                    modem::port::specialized::V24UDPPort* udpPort = dynamic_cast<modem::port::specialized::V24UDPPort*>(m_udpDFSIRemotePort);
                    udpPort->closeFSC();
                }

                if (m_modem != nullptr) {
                    m_modem->close();
                    delete m_modem;
                }

                if (g_RPC != nullptr) {
                    g_RPC->close();
                    delete g_RPC;
                }

                ::LogSetNetwork(nullptr);
                if (m_network != nullptr) {
                    m_network->close();
                    delete m_network;
                }

                if (m_RESTAPI != nullptr) {
                    m_RESTAPI->close();
                    delete m_RESTAPI;
                }

                if (m_channelLookup != nullptr) {
                    delete m_channelLookup;
                }

                if (m_tidLookup != nullptr) {
                    m_tidLookup->setReloadTime(0U);
                    m_tidLookup->stop();
                }
                if (m_ridLookup != nullptr) {
                    m_tidLookup->setReloadTime(0U);
                    m_ridLookup->stop();
                }
            }
            else {
                m_state = STATE_IDLE;
            }
            break;
    }
}

/* Entry point to RPC clock thread. */

void* Host::threadRPC(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("host:rpc");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogMessage(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            // scope is intentional
            {
                // ------------------------------------------------------
                //  -- RPC Clocking                                   --
                // ------------------------------------------------------

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();

                g_RPC->clock(ms);
            }

            if (host->m_state != STATE_IDLE)
                Thread::sleep(m_activeTickDelay);
            if (host->m_state == STATE_IDLE)
                Thread::sleep(m_idleTickDelay);
        }

        LogMessage(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to modem clock thread. */

void* Host::threadModem(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("host:modem");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogMessage(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            // scope is intentional
            {
                std::lock_guard<std::mutex> lock(m_clockingMutex);

                // ------------------------------------------------------
                //  -- Modem Clocking                                 --
                // ------------------------------------------------------

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();

                host->m_modem->clock(ms);
            }

            if (host->m_state != STATE_IDLE)
                Thread::sleep(m_activeTickDelay);
            if (host->m_state == STATE_IDLE)
                Thread::sleep(m_idleTickDelay);
        }

        LogMessage(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to watchdog thread. */

void* Host::threadWatchdog(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("host:watchdog");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogMessage(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            if (host->m_isTxCW) {
                Thread::sleep(1U);
                continue;
            }

            // scope is intentional
            {
                /** Digital Mobile Radio */
                if (host->m_dmr != nullptr) {
                    if (host->m_dmrTx1WatchdogTimer.isRunning())
                        host->m_dmrTx1WatchdogTimer.clock(ms);
                    if (host->m_dmrTx1WatchdogTimer.isRunning() && host->m_dmrTx1WatchdogTimer.hasExpired() && !host->m_dmrTx1WatchdogTimer.isPaused()) {
                        host->m_dmrTx1WatchdogTimer.pause();
                        LogError(LOG_HOST, "PANIC; DMR, slot 1 Tx frame processor hung >%us, ms = %u", host->m_dmrTx1WatchdogTimer.getTimeout(), host->m_dmrTx1LoopMS);
                    }

                    if (!host->m_disableWatchdogOverflow) {
                        if (host->m_modem->gotModemStatus() && !host->m_modem->hasDMRSpace1() && host->m_dmr->isQueueFull(1U) &&
                            !host->m_dmrCtrlChannel && !host->m_dmrBeaconDurationTimer.isRunning()) {
                            if (host->m_dmr1OverflowCnt > MAX_OVERFLOW_CNT) {
                                std::lock_guard<std::mutex> lock(m_clockingMutex);

                                LogError(LOG_HOST, "PANIC; DMR, has no DMR slot 1 FIFO space, and DMR slot 1 queue is full! Resetting states.");

                                host->setState(STATE_IDLE);
                                host->m_modem->writeDMRStart(false);
                                host->m_modem->clearDMRFrame1();

                                // respect the fixed mode state
                                if (host->m_fixedMode) {
                                    host->setState(STATE_DMR);
                                }

                                host->m_dmr1OverflowCnt = 0U;
                            } else {
                                host->m_dmr1OverflowCnt++;
                            }
                        } else {
                            host->m_dmr1OverflowCnt = 0U;
                        }
                    }

                    if (host->m_dmrTx2WatchdogTimer.isRunning())
                        host->m_dmrTx2WatchdogTimer.clock(ms);
                    if (host->m_dmrTx2WatchdogTimer.isRunning() && host->m_dmrTx2WatchdogTimer.hasExpired() && !host->m_dmrTx2WatchdogTimer.isPaused()) {
                        host->m_dmrTx2WatchdogTimer.pause();
                        LogError(LOG_HOST, "PANIC; DMR, slot 2 Tx frame processor hung >%us, ms = %u", host->m_dmrTx2WatchdogTimer.getTimeout(), host->m_dmrTx2LoopMS);
                    }

                    if (!host->m_disableWatchdogOverflow) {
                        if (host->m_modem->gotModemStatus() && !host->m_modem->hasDMRSpace2() && host->m_dmr->isQueueFull(2U) &&
                            !host->m_dmrCtrlChannel && !host->m_dmrBeaconDurationTimer.isRunning()) {
                            if (host->m_dmr2OverflowCnt > MAX_OVERFLOW_CNT) {
                                std::lock_guard<std::mutex> lock(m_clockingMutex);

                                LogError(LOG_HOST, "PANIC; DMR, has no DMR slot 2 FIFO space, and DMR slot 2 queue is full! Resetting states.");

                                host->setState(STATE_IDLE);
                                host->m_modem->writeDMRStart(false);
                                host->m_modem->clearDMRFrame2();

                                // respect the fixed mode state
                                if (host->m_fixedMode) {
                                    host->setState(STATE_DMR);
                                }

                                host->m_dmr2OverflowCnt = 0U;
                            } else {
                                host->m_dmr2OverflowCnt++;
                            }
                        } else {
                            host->m_dmr2OverflowCnt = 0U;
                        }
                    }
                }

                /** Project 25 */
                if (host->m_p25 != nullptr) {
                    if (host->m_p25TxWatchdogTimer.isRunning())
                        host->m_p25TxWatchdogTimer.clock(ms);
                    if (host->m_p25TxWatchdogTimer.isRunning() && host->m_p25TxWatchdogTimer.hasExpired() && !host->m_p25TxWatchdogTimer.isPaused()) {
                        host->m_p25TxWatchdogTimer.pause();
                        LogError(LOG_HOST, "PANIC; P25, Tx frame processor hung >%us, ms = %u", host->m_p25TxWatchdogTimer.getTimeout(), host->m_p25TxLoopMS);
                    }

                    if (!host->m_disableWatchdogOverflow) {
                        if (host->m_modem->gotModemStatus() && !host->m_modem->hasP25Space(P25DEF::P25_LDU_FRAME_LENGTH_BYTES) && host->m_p25->isQueueFull() &&
                            !host->m_p25CtrlChannel && !host->m_p25BcastDurationTimer.isRunning()) {
                            if (host->m_p25OverflowCnt > MAX_OVERFLOW_CNT) {
                                std::lock_guard<std::mutex> lock(m_clockingMutex);

                                LogError(LOG_HOST, "PANIC; P25, modem has no P25 FIFO space, and internal P25 queue is full! Resetting states.");

                                host->setState(STATE_IDLE);
                                host->m_modem->clearP25Frame();
                                host->m_p25->reset();

                                // respect the fixed mode state
                                if (host->m_fixedMode) {
                                    host->setState(STATE_P25);
                                }

                                host->m_p25OverflowCnt = 0U;
                            } else {
                                host->m_p25OverflowCnt++;
                            }
                        } else {
                            host->m_p25OverflowCnt = 0U;
                        }
                    }
                }

                /** Next Generation Digital Narrowband */
                if (host->m_nxdn != nullptr) {
                    if (host->m_nxdnTxWatchdogTimer.isRunning())
                        host->m_nxdnTxWatchdogTimer.clock(ms);
                    if (host->m_nxdnTxWatchdogTimer.isRunning() && host->m_nxdnTxWatchdogTimer.hasExpired() && !host->m_nxdnTxWatchdogTimer.isPaused()) {
                        host->m_nxdnTxWatchdogTimer.pause();
                        LogError(LOG_HOST, "PANIC; NXDN, Tx frame processor hung >%us, ms = %u", host->m_nxdnTxWatchdogTimer.getTimeout(), host->m_nxdnTxLoopMS);
                    }

                    if (!host->m_disableWatchdogOverflow) {
                        if (host->m_modem->gotModemStatus() && !host->m_modem->hasNXDNSpace() && host->m_nxdn->isQueueFull() &&
                            !host->m_nxdnCtrlChannel && !host->m_nxdnBcastDurationTimer.isRunning()) {
                            if (host->m_nxdnOverflowCnt > MAX_OVERFLOW_CNT) {
                                std::lock_guard<std::mutex> lock(m_clockingMutex);

                                LogError(LOG_HOST, "PANIC; NXDN, modem has no NXDN FIFO space, and NXDN queue is full! Resetting states.");

                                host->setState(STATE_IDLE);
                                host->m_modem->clearNXDNFrame();
                                host->m_nxdn->reset();

                                // respect the fixed mode state
                                if (host->m_fixedMode) {
                                    host->setState(STATE_NXDN);
                                }

                                host->m_nxdnOverflowCnt = 0U;
                            } else {
                                host->m_nxdnOverflowCnt++;
                            }
                        } else {
                            host->m_nxdnOverflowCnt = 0U;
                        }
                    }
                }
            }

            // scope is intentional
            {
                if (host->m_mainLoopWatchdogTimer.isRunning())
                    host->m_mainLoopWatchdogTimer.clock(ms);
                if (host->m_mainLoopWatchdogTimer.isRunning() && host->m_mainLoopWatchdogTimer.hasExpired() && !host->m_mainLoopWatchdogTimer.isPaused()) {
                    host->m_mainLoopWatchdogTimer.pause();
                    LogError(LOG_HOST, "main processor hung >%us, stage = %u, ms = %u", host->m_mainLoopWatchdogTimer.getTimeout(), host->m_mainLoopStage, host->m_mainLoopMS);
                }

                if (host->m_adjSiteLoopWatchdogTimer.isRunning())
                    host->m_adjSiteLoopWatchdogTimer.clock(ms);
                if (host->m_adjSiteLoopWatchdogTimer.isRunning() && host->m_adjSiteLoopWatchdogTimer.hasExpired() && !host->m_adjSiteLoopWatchdogTimer.isPaused()) {
                    host->m_adjSiteLoopWatchdogTimer.pause();
                    LogError(LOG_HOST, "adj. site update hung >%us, ms = %u", host->m_adjSiteLoopWatchdogTimer.getTimeout(), host->m_adjSiteLoopMS);
                }
            }

            if (host->m_state != STATE_IDLE)
                Thread::sleep(m_activeTickDelay);
            if (host->m_state == STATE_IDLE)
                Thread::sleep(m_idleTickDelay);
        }

        LogMessage(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to site data update thread. */

void* Host::threadSiteData(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("host:site-data");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogMessage(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        Timer networkPeerStatusNotify(1000U, 2U);
        networkPeerStatusNotify.start();

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();
            host->m_adjSiteLoopMS = ms;

            if (host->m_dmr != nullptr)
                host->m_dmr->clockSiteData(ms);
            if (host->m_p25 != nullptr)
                host->m_p25->clockSiteData(ms);
            if (host->m_nxdn != nullptr)
                host->m_nxdn->clockSiteData(ms);

            if (host->m_allowStatusTransfer && host->m_network != nullptr) {
                networkPeerStatusNotify.clock(ms);
                if (networkPeerStatusNotify.isRunning() && networkPeerStatusNotify.hasExpired()) {
                    networkPeerStatusNotify.start();
                    json::object statusObj = host->getStatus();
                    host->m_network->writePeerStatus(statusObj);
                }
            }

            if (host->m_state != STATE_IDLE)
                Thread::sleep(m_activeTickDelay);
            if (host->m_state == STATE_IDLE)
                Thread::sleep(m_idleTickDelay);
        }

        LogMessage(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}

/* Entry point to presence update thread. */

void* Host::threadPresence(void* arg)
{
    thread_t* th = (thread_t*)arg;
    if (th != nullptr) {
#if defined(_WIN32)
        ::CloseHandle(th->thread);
#else
        ::pthread_detach(th->thread);
#endif // defined(_WIN32)

        std::string threadName("host:presence");
        Host* host = static_cast<Host*>(th->obj);
        if (host == nullptr) {
            g_killed = true;
            LogError(LOG_HOST, "[FAIL] %s", threadName.c_str());
        }

        if (g_killed) {
            delete th;
            return nullptr;
        }

        LogMessage(LOG_HOST, "[ OK ] %s", threadName.c_str());
#ifdef _GNU_SOURCE
        ::pthread_setname_np(th->thread, threadName.c_str());
#endif // _GNU_SOURCE

        // register VC -> CC notification RPC handler
        g_RPC->registerHandler(RPC_REGISTER_CC_VC, [=](json::object &req, json::object &reply) {
            g_RPC->defaultResponse(reply, "OK", network::NetRPC::OK);

            if (!host->m_dmrTSCCData && !host->m_p25CCData && !host->m_nxdnCCData) {
                g_RPC->defaultResponse(reply, "Host is not a control channel, cannot register voice channel");
                return;
            }

            // validate channelNo is a string within the JSON blob
            if (!req["channelNo"].is<int>()) {
                g_RPC->defaultResponse(reply, "channelNo was not a valid integer", network::NetRPC::INVALID_ARGS);
                return;
            }

            uint32_t channelNo = req["channelNo"].get<uint32_t>();

            // validate peerId is a string within the JSON blob
            if (!req["peerId"].is<int>()) {
                g_RPC->defaultResponse(reply, "peerId was not a valid integer", network::NetRPC::INVALID_ARGS);
                return;
            }

            uint32_t peerId = req["peerId"].get<uint32_t>();

            // LogDebug(LOG_REST, "RPC_REGISTER_CC_VC callback, channelNo = %u, peerId = %u", channelNo, peerId);

            // validate restAddress is a string within the JSON blob
            if (!req["rpcAddress"].is<std::string>()) {
                g_RPC->defaultResponse(reply, "rpcAddress was not a valid string", network::NetRPC::INVALID_ARGS);
                return;
            }

            if (!req["rpcPort"].is<int>()) {
                g_RPC->defaultResponse(reply, "rpcPort was not a valid integer", network::NetRPC::INVALID_ARGS);
                return;
            }

            std::string rpcAddress = req["rpcAddress"].get<std::string>();
            uint16_t rpcPort = (uint16_t)req["rpcPort"].get<int>();

            auto voiceChData = host->rfCh()->rfChDataTable();
            if (voiceChData.find(channelNo) != voiceChData.end()) {
                ::lookups::VoiceChData voiceCh = host->rfCh()->getRFChData(channelNo);

                if (voiceCh.address() == "0.0.0.0") {
                    voiceCh.address(rpcAddress);
                }

                if (voiceCh.port() == 0U || voiceCh.port() == RPC_DEFAULT_PORT) {
                    voiceCh.port(rpcPort);
                }

                host->rfCh()->setRFChData(channelNo, voiceCh);

                host->m_voiceChPeerId[channelNo] = peerId;
                LogMessage(LOG_REST, "VC %s:%u, registration notice, peerId = %u, chNo = %u-%u", voiceCh.address().c_str(), voiceCh.port(), peerId, voiceCh.chId(), channelNo);
                LogInfoEx(LOG_HOST, "Voice Channel Id %u Channel No $%04X REST API Address %s:%u", voiceCh.chId(), channelNo, voiceCh.address().c_str(), voiceCh.port());

                g_fireCCVCNotification = true; // announce this registration immediately to the FNE
            } else {
                LogMessage(LOG_REST, "VC, registration rejected, peerId = %u, chNo = %u, VC wasn't a defined member of the CC voice channel list", peerId, channelNo);
                g_RPC->defaultResponse(reply, "registration rejected", network::NetRPC::BAD_REQUEST);
            } 
        });

        Timer presenceNotifyTimer(1000U, host->m_presenceTime);
        presenceNotifyTimer.start();
        bool hasInitialRegistered = false;

        StopWatch stopWatch;
        stopWatch.start();

        while (!g_killed) {
            // scope is intentional
            {
                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();

                presenceNotifyTimer.clock(ms);

                // VC -> CC presence registration
                if (!host->m_controlChData.address().empty() && host->m_controlChData.port() != 0 && host->m_network != nullptr && 
                    !host->m_dmrCtrlChannel && !host->m_p25CtrlChannel && !host->m_nxdnCtrlChannel) {
                    if ((presenceNotifyTimer.isRunning() && presenceNotifyTimer.hasExpired()) || !hasInitialRegistered) {
                        LogMessage(LOG_HOST, "CC %s:%u, notifying CC of VC registration, peerId = %u", host->m_controlChData.address().c_str(), host->m_controlChData.port(), host->m_network->getPeerId());
                        hasInitialRegistered = true;

                        std::string localAddress = network::udp::Socket::getLocalAddress();
                        if (host->m_rpcAddress == "0.0.0.0") {
                            host->m_rpcAddress = localAddress;
                        }

                        // callback REST API to release the granted TG on the specified control channel
                        json::object req = json::object();
                        req["channelNo"].set<uint32_t>(host->m_channelNo);
                        uint32_t peerId = host->m_network->getPeerId();
                        req["peerId"].set<uint32_t>(peerId);
                        req["rpcAddress"].set<std::string>(host->m_rpcAddress);
                        req["rpcPort"].set<uint16_t>(host->m_rpcPort);

                        g_RPC->req(RPC_REGISTER_CC_VC, req, [=](json::object &req, json::object &reply) {
                            if (!req["status"].is<int>()) {
                                ::LogError(LOG_HOST, "failed to notify the CC %s:%u of VC registration, invalid RPC response", host->m_controlChData.address().c_str(), host->m_controlChData.port());
                                return;
                            }

                            int status = req["status"].get<int>();
                            if (status != network::NetRPC::OK) {
                                ::LogError(LOG_HOST, "failed to notify the CC %s:%u of VC registration", host->m_controlChData.address().c_str(), host->m_controlChData.port());
                                if (req["message"].is<std::string>()) {
                                    std::string retMsg = req["message"].get<std::string>();
                                    ::LogError(LOG_HOST, "RPC failed, %s", retMsg.c_str());
                                }
                            }
                            else
                                ::LogMessage(LOG_HOST, "CC %s:%u, VC registered, peerId = %u", host->m_controlChData.address().c_str(), host->m_controlChData.port(), host->m_network->getPeerId());
                        }, host->m_controlChData.address(), host->m_controlChData.port());

                        presenceNotifyTimer.start();
                    }
                }

                // CC -> FNE registered VC announcement
                if (host->m_dmrCtrlChannel || host->m_p25CtrlChannel || host->m_nxdnCtrlChannel) {
                    if ((presenceNotifyTimer.isRunning() && presenceNotifyTimer.hasExpired()) || g_fireCCVCNotification) {
                        g_fireCCVCNotification = false;
                        if (host->m_network != nullptr && host->m_voiceChPeerId.size() > 0) {
                            LogMessage(LOG_HOST, "notifying FNE of VC registrations, peerId = %u", host->m_network->getPeerId());

                            std::vector<uint32_t> peers;
                            for (auto it : host->m_voiceChPeerId) {
                                peers.push_back(it.second);
                            }

                            host->m_network->announceSiteVCs(peers);
                        }

                        presenceNotifyTimer.start();
                    }
                }
            }

            if (host->m_state != STATE_IDLE)
                Thread::sleep(m_activeTickDelay);
            if (host->m_state == STATE_IDLE)
                Thread::sleep(m_idleTickDelay);
        }

        LogMessage(LOG_HOST, "[STOP] %s", threadName.c_str());
        delete th;
    }

    return nullptr;
}
