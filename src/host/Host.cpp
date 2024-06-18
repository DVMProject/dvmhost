// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2023 Bryan Biedenkapp, N2PLL
*   Copyright (C) 2021 Nat Moore
*
*/
#include "Defines.h"
#include "common/lookups/RSSIInterpolator.h"
#include "common/network/udp/Socket.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "common/ThreadFunc.h"
#include "common/Utils.h"
#include "remote/RESTClient.h"
#include "host/Host.h"
#include "ActivityLog.h"
#include "HostMain.h"

using namespace modem;
using namespace lookups;

#include <cstdio>
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>

#include <unistd.h>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define CW_IDLE_SLEEP_MS 50U
#define IDLE_WARMUP_MS 5U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Host class.
/// </summary>
/// <param name="confFile">Full-path to the configuration file.</param>
Host::Host(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_modem(nullptr),
    m_modemRemote(false),
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
    m_dmrBeaconDurationTimer(1000U),
    m_dmrDedicatedTxTestTimer(1000U, 0U, 125U),
    m_p25BcastDurationTimer(1000U),
    m_p25DedicatedTxTestTimer(1000U, 0U, 125U),
    m_nxdnBcastDurationTimer(1000U),
    m_nxdnDedicatedTxTestTimer(1000U, 0U, 125U),
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
    m_activeTickDelay(5U),
    m_idleTickDelay(5U),
    m_restAddress("0.0.0.0"),
    m_restPort(REST_API_DEFAULT_PORT),
    m_RESTAPI(nullptr)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Host class.
/// </summary>
Host::~Host() = default;

/// <summary>
/// Executes the main modem host processing loop.
/// </summary>
/// <returns>Zero if successful, otherwise error occurred.</returns>
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
    ret = ::LogInitialise(logConf["filePath"].as<std::string>(), logConf["fileRoot"].as<std::string>(),
        logConf["fileLevel"].as<uint32_t>(0U), logConf["displayLevel"].as<uint32_t>(0U), false, logConf["useSyslog"].as<bool>(false));
    if (!ret) {
        ::fatal("unable to open the log file\n");
    }

    ret = ::ActivityLogInitialise(logConf["activityFilePath"].as<std::string>(), logConf["fileRoot"].as<std::string>());
    if (!ret) {
        ::fatal("unable to open the activity log file\n");
    }

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

    ::LogInfo(__BANNER__ "\r\n" __PROG_NAME__ " " __VER__ " (built " __BUILD__ ")\r\n" \
        "Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
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
        ::LogInfoEx(LOG_HOST, "Host is up and running in remote modem mode");

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

    std::unique_ptr<dmr::Control> dmr = nullptr;
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

        dmr = std::make_unique<dmr::Control>(m_authoritative, m_dmrColorCode, callHang, m_dmrQueueSizeBytes,
            embeddedLCOnly, dumpTAData, m_timeout, m_rfTalkgroupHang, m_modem, m_network, m_duplex, m_channelLookup, m_ridLookup, m_tidLookup, 
            m_idenTable, rssi, jitter, dmrDumpDataPacket, dmrRepeatDataPacket, dmrDumpCsbkData, dmrDebug, dmrVerbose);
        dmr->setOptions(m_conf, m_supervisor, m_controlChData, m_dmrNetId, m_siteId, m_channelId, 
            m_channelNo, true);

        if (dmrCtrlChannel) {
            dmr->setCCRunning(true);
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

    std::unique_ptr<p25::Control> p25 = nullptr;
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

        LogInfo("    TDU Preamble before Voice: %u", tduPreambleCount);
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

        p25 = std::make_unique<p25::Control>(m_authoritative, m_p25NAC, callHang, m_p25QueueSizeBytes, m_modem,
            m_network, m_timeout, m_rfTalkgroupHang, m_duplex, m_channelLookup, m_ridLookup, m_tidLookup, m_idenTable, rssi, p25DumpDataPacket, 
            p25RepeatDataPacket, p25DumpTsbkData, p25Debug, p25Verbose);
        p25->setOptions(m_conf, m_supervisor, m_cwCallsign, m_controlChData,
            m_p25NetId, m_sysId, m_p25RfssId, m_siteId, m_channelId, m_channelNo, true);

        if (p25CtrlChannel) {
            p25->setCCRunning(true);
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

    std::unique_ptr<nxdn::Control> nxdn = nullptr;
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

        nxdn = std::make_unique<nxdn::Control>(m_authoritative, m_nxdnRAN, callHang, m_nxdnQueueSizeBytes,
            m_timeout, m_rfTalkgroupHang, m_modem, m_network, m_duplex, m_channelLookup, m_ridLookup, m_tidLookup, m_idenTable, rssi, 
            nxdnDumpRcchData, nxdnDebug, nxdnVerbose);
        nxdn->setOptions(m_conf, m_supervisor, m_cwCallsign, m_controlChData, m_siteId, 
            m_sysId, m_channelId, m_channelNo, true);

        if (nxdnCtrlChannel) {
            nxdn->setCCRunning(true);
        }

        if (nxdnVerbose) {
            LogInfo("    Verbose: yes");
        }
        if (nxdnDebug) {
            LogInfo("    Debug: yes");
        }
    }

    if (!m_dmrEnabled && !m_p25Enabled && !m_nxdnEnabled) {
        ::LogError(LOG_HOST, "No protocols enabled? DMR, P25 and/or NXDN must be enabled!");
        g_killed = true;
    }

    if (m_fixedMode && ((m_dmrEnabled && m_p25Enabled) || (m_dmrEnabled && m_nxdnEnabled) || (m_p25Enabled && m_nxdnEnabled))) {
        ::LogError(LOG_HOST, "Cannot have DMR, P25 and NXDN when using fixed state! Choose one protocol for fixed state operation.");
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
            if (dmr != nullptr)
                setState(STATE_DMR);

            if (p25 != nullptr)
                setState(STATE_P25);

            if (nxdn != nullptr)
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

            if (m_nxdnCtrlChannel) {
                m_fixedMode = true;
                setState(STATE_NXDN);
            }

            setState(STATE_IDLE);
        }

        if (m_RESTAPI != nullptr) {
            m_RESTAPI->setProtocols(dmr.get(), p25.get(), nxdn.get());
        }

        ::LogInfoEx(LOG_HOST, "Host is performing late initialization and warmup");

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

        ::LogInfoEx(LOG_HOST, "Host is up and running");
        stopWatch.start();
    }

    bool hasTxShutdown = false;
    static std::mutex clockingMutex;

    // Macro to start DMR duplex idle transmission (or beacon)
    #define START_DMR_DUPLEX_IDLE(x)                                                                                    \
        if (dmr != nullptr) {                                                                                           \
            if (m_duplex) {                                                                                             \
                m_modem->writeDMRStart(x);                                                                              \
                m_dmrTXTimer.start();                                                                                   \
            }                                                                                                           \
        }

    /** Watchdog */
    ThreadFunc watchdogThread([&, this]() {
        if (g_killed)
            return;

        StopWatch stopWatch;
        stopWatch.start();

        LogDebug(LOG_HOST, "started watchdog");
        while (!g_killed) {
            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            // scope is intentional
            {
                /** Digital Mobile Radio */
                if (dmr != nullptr) {
                    if (m_dmrTx1WatchdogTimer.isRunning())
                        m_dmrTx1WatchdogTimer.clock(ms);
                    if (m_dmrTx1WatchdogTimer.isRunning() && m_dmrTx1WatchdogTimer.hasExpired() && !m_dmrTx1WatchdogTimer.isPaused()) {
                        m_dmrTx1WatchdogTimer.pause();
                        LogError(LOG_HOST, "DMR, slot 1 frame processor hung >%us, ms = %u", m_dmrTx1WatchdogTimer.getTimeout(), m_dmrTx1LoopMS);
                    }

                    if (m_dmrTx2WatchdogTimer.isRunning())
                        m_dmrTx2WatchdogTimer.clock(ms);
                    if (m_dmrTx2WatchdogTimer.isRunning() && m_dmrTx2WatchdogTimer.hasExpired() && !m_dmrTx2WatchdogTimer.isPaused()) {
                        m_dmrTx2WatchdogTimer.pause();
                        LogError(LOG_HOST, "DMR, slot 2 frame processor hung >%us, ms = %u", m_dmrTx2WatchdogTimer.getTimeout(), m_dmrTx2LoopMS);
                    }
                }

                /** Project 25 */
                if (p25 != nullptr) {
                    if (m_p25TxWatchdogTimer.isRunning())
                        m_p25TxWatchdogTimer.clock(ms);
                    if (m_p25TxWatchdogTimer.isRunning() && m_p25TxWatchdogTimer.hasExpired() && !m_p25TxWatchdogTimer.isPaused()) {
                        m_p25TxWatchdogTimer.pause();
                        LogError(LOG_HOST, "P25, frame processor hung >%us, ms = %u", m_p25TxWatchdogTimer.getTimeout(), m_p25TxLoopMS);
                    }
                }

                /** Next Generation Digital Narrowband */
                if (nxdn != nullptr) {
                    if (m_nxdnTxWatchdogTimer.isRunning())
                        m_nxdnTxWatchdogTimer.clock(ms);
                    if (m_nxdnTxWatchdogTimer.isRunning() && m_nxdnTxWatchdogTimer.hasExpired() && !m_nxdnTxWatchdogTimer.isPaused()) {
                        m_nxdnTxWatchdogTimer.pause();
                        LogError(LOG_HOST, "NXDN, frame processor hung >%us, ms = %u", m_nxdnTxWatchdogTimer.getTimeout(), m_nxdnTxLoopMS);
                    }
                }
            }

            // scope is intentional
            {
                if (m_mainLoopWatchdogTimer.isRunning())
                    m_mainLoopWatchdogTimer.clock(ms);
                if (m_mainLoopWatchdogTimer.isRunning() && m_mainLoopWatchdogTimer.hasExpired() && !m_mainLoopWatchdogTimer.isPaused()) {
                    m_mainLoopWatchdogTimer.pause();
                    LogError(LOG_HOST, "main processor hung >%us, stage = %u, ms = %u", m_mainLoopWatchdogTimer.getTimeout(), m_mainLoopStage, m_mainLoopMS);
                }

                if (m_adjSiteLoopWatchdogTimer.isRunning())
                    m_adjSiteLoopWatchdogTimer.clock(ms);
                if (m_adjSiteLoopWatchdogTimer.isRunning() && m_adjSiteLoopWatchdogTimer.hasExpired() && !m_adjSiteLoopWatchdogTimer.isPaused()) {
                    m_adjSiteLoopWatchdogTimer.pause();
                    LogError(LOG_HOST, "adj. site update hung >%us, ms = %u", m_adjSiteLoopWatchdogTimer.getTimeout(), m_adjSiteLoopMS);
                }
            }

            if (m_state != STATE_IDLE)
                Thread::sleep(m_activeTickDelay);
            if (m_state == STATE_IDLE)
                Thread::sleep(m_idleTickDelay);
        }
    });
    watchdogThread.run();
    watchdogThread.setName("host:watchdog");

    /** Digital Mobile Radio Frame Processor */
    ThreadFunc dmrFrame1WriteThread([&, this]() {
        if (g_killed)
            return;

        StopWatch stopWatch;
        stopWatch.start();

        if (dmr != nullptr) {
            LogDebug(LOG_HOST, "DMR, started slot 1 frame processor (modem write)");
            while (!g_killed) {
                m_dmrTx1WatchdogTimer.start();

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();
                m_dmrTx1LoopMS = ms;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(clockingMutex);

                    // ------------------------------------------------------
                    //  -- Write to Modem Processing                      --
                    // ------------------------------------------------------
                    
                    // write DMR slot 1 frames to modem
                    writeFramesDMR1(dmr.get(), [&, this]() {
                        // if there is a P25 CC running; halt the CC
                        if (p25 != nullptr) {
                            if (p25->getCCRunning() && !p25->getCCHalted()) {
                                this->interruptP25Control(p25.get());
                            }
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (nxdn != nullptr) {
                            if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                                this->interruptNXDNControl(nxdn.get());
                            }
                        }
                    });
                }

                if (m_state != STATE_IDLE)
                    Thread::sleep(m_activeTickDelay);
                if (m_state == STATE_IDLE)
                    Thread::sleep(m_idleTickDelay);
            }
        }
    });
    dmrFrame1WriteThread.run();
    dmrFrame1WriteThread.setName("dmr:frame1-w");

    ThreadFunc dmrFrame2WriteThread([&, this]() {
        if (g_killed)
            return;

        StopWatch stopWatch;
        stopWatch.start();

        if (dmr != nullptr) {
            LogDebug(LOG_HOST, "DMR, started slot 2 frame processor (modem write)");
            while (!g_killed) {
                m_dmrTx2WatchdogTimer.start();

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();
                m_dmrTx2LoopMS = ms;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(clockingMutex);

                    // ------------------------------------------------------
                    //  -- Write to Modem Processing                      --
                    // ------------------------------------------------------

                    // write DMR slot 2 frames to modem
                    writeFramesDMR2(dmr.get(), [&, this]() {
                        // if there is a P25 CC running; halt the CC
                        if (p25 != nullptr) {
                            if (p25->getCCRunning() && !p25->getCCHalted()) {
                                this->interruptP25Control(p25.get());
                            }
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (nxdn != nullptr) {
                            if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                                this->interruptNXDNControl(nxdn.get());
                            }
                        }
                    });
                }

                if (m_state != STATE_IDLE)
                    Thread::sleep(m_activeTickDelay);
                if (m_state == STATE_IDLE)
                    Thread::sleep(m_idleTickDelay);
            }
        }
    });
    dmrFrame2WriteThread.run();
    dmrFrame2WriteThread.setName("dmr:frame2-w");

    /** Project 25 Frame Processor */
    ThreadFunc p25FrameWriteThread([&, this]() {
        if (g_killed)
            return;

        StopWatch stopWatch;
        stopWatch.start();

        if (p25 != nullptr) {
            LogDebug(LOG_HOST, "P25, started frame processor (modem write)");
            while (!g_killed) {
                m_p25TxWatchdogTimer.start();

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();
                m_p25TxLoopMS = ms;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(clockingMutex);

                    // ------------------------------------------------------
                    //  -- Write to Modem Processing                      --
                    // ------------------------------------------------------

                    // write P25 frames to modem
                    writeFramesP25(p25.get(), [&, this]() {
                        if (dmr != nullptr) {
                            this->interruptDMRBeacon(dmr.get());
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (nxdn != nullptr) {
                            if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                                this->interruptNXDNControl(nxdn.get());
                            }
                        }
                    });
                }

                if (m_state != STATE_IDLE)
                    Thread::sleep(m_activeTickDelay);
                if (m_state == STATE_IDLE)
                    Thread::sleep(m_idleTickDelay);
            }
        }
    });
    p25FrameWriteThread.run();
    p25FrameWriteThread.setName("p25:frame-w");

    /** Next Generation Digital Narrowband Frame Processor */
    ThreadFunc nxdnFrameWriteThread([&, this]() {
        if (g_killed)
            return;

        StopWatch stopWatch;
        stopWatch.start();

        if (nxdn != nullptr) {
            LogDebug(LOG_HOST, "NXDN, started frame processor (modem write)");
            while (!g_killed) {
                m_nxdnTxWatchdogTimer.start();

                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();
                m_nxdnTxLoopMS = ms;

                // scope is intentional
                {
                    std::lock_guard<std::mutex> lock(clockingMutex);

                    // ------------------------------------------------------
                    //  -- Write to Modem Processing                      --
                    // ------------------------------------------------------

                    // write NXDN frames to modem
                    writeFramesNXDN(nxdn.get(), [&, this]() {
                        if (dmr != nullptr) {
                            this->interruptDMRBeacon(dmr.get());
                        }

                        // if there is a P25 CC running; halt the CC
                        if (p25 != nullptr) {
                            if (p25->getCCRunning() && !p25->getCCHalted()) {
                                this->interruptP25Control(p25.get());
                            }
                        }
                    });
                }

                if (m_state != STATE_IDLE)
                    Thread::sleep(m_activeTickDelay);
                if (m_state == STATE_IDLE)
                    Thread::sleep(m_idleTickDelay);
            }
        }
    });
    nxdnFrameWriteThread.run();
    nxdnFrameWriteThread.setName("nxdn:frame-w");

    /** Adjacent Site and Affiliation Update */
    ThreadFunc siteDataUpdateThread([&, this]() {
        if (g_killed)
            return;

        Timer networkPeerStatusNotify(1000U, 5U);
        networkPeerStatusNotify.start();

        StopWatch stopWatch;
        stopWatch.start();

        LogDebug(LOG_HOST, "started adj. site and affiliation processor");
        while (!g_killed) {
            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();
            m_adjSiteLoopMS = ms;

            if (dmr != nullptr)
                dmr->clockSiteData(ms);
            if (p25 != nullptr)
                p25->clockSiteData(ms);
            if (nxdn != nullptr)
                nxdn->clockSiteData(ms);

            if (m_allowStatusTransfer) {
                networkPeerStatusNotify.clock(ms);
                if (networkPeerStatusNotify.isRunning() && networkPeerStatusNotify.hasExpired()) {
                    networkPeerStatusNotify.start();
                    json::object statusObj = getStatus();
                    m_network->writePeerStatus(statusObj);
                }
            }

            if (m_state != STATE_IDLE)
                Thread::sleep(m_activeTickDelay);
            if (m_state == STATE_IDLE)
                Thread::sleep(m_idleTickDelay);
        }
    });
    siteDataUpdateThread.run();
    siteDataUpdateThread.setName("host:site-data");

    /** Network Presence Notification */
    ThreadFunc presenceThread([&, this]() {
        if (g_killed)
            return;

        Timer presenceNotifyTimer(1000U, m_presenceTime);
        presenceNotifyTimer.start();
        bool hasInitialRegistered = false;

        StopWatch stopWatch;
        stopWatch.start();

        LogDebug(LOG_HOST, "started presence notifier");
        while (!g_killed) {
            // scope is intentional
            {
                uint32_t ms = stopWatch.elapsed();
                stopWatch.start();

                presenceNotifyTimer.clock(ms);

                // VC -> CC presence registration
                if (!m_controlChData.address().empty() && m_controlChData.port() != 0 && m_network != nullptr && m_RESTAPI != nullptr &&
                    !m_dmrCtrlChannel && !m_p25CtrlChannel && !m_nxdnCtrlChannel) {
                    if ((presenceNotifyTimer.isRunning() && presenceNotifyTimer.hasExpired()) || !hasInitialRegistered) {
                        LogMessage(LOG_HOST, "CC %s:%u, notifying CC of VC registration, peerId = %u", m_controlChData.address().c_str(), m_controlChData.port(), m_network->getPeerId());
                        hasInitialRegistered = true;

                        std::string localAddress = network::udp::Socket::getLocalAddress();
                        if (m_restAddress == "0.0.0.0") {
                            m_restAddress = localAddress;
                        }

                        // callback REST API to release the granted TG on the specified control channel
                        json::object req = json::object();
                        req["channelNo"].set<uint32_t>(m_channelNo);
                        uint32_t peerId = m_network->getPeerId();
                        req["peerId"].set<uint32_t>(peerId);
                        req["restAddress"].set<std::string>(m_restAddress);
                        req["restPort"].set<uint16_t>(m_restPort);

                        int ret = RESTClient::send(m_controlChData.address(), m_controlChData.port(), m_controlChData.password(),
                            HTTP_PUT, PUT_REGISTER_CC_VC, req, m_controlChData.ssl(), REST_QUICK_WAIT, false);
                        if (ret != network::rest::http::HTTPPayload::StatusType::OK) {
                            ::LogError(LOG_HOST, "failed to notify the CC %s:%u of VC registration", m_controlChData.address().c_str(), m_controlChData.port());
                        }

                        presenceNotifyTimer.start();
                    }
                }

                // CC -> FNE registered VC announcement
                if (m_dmrCtrlChannel || m_p25CtrlChannel || m_nxdnCtrlChannel) {
                    if ((presenceNotifyTimer.isRunning() && presenceNotifyTimer.hasExpired()) || g_fireCCVCNotification) {
                        g_fireCCVCNotification = false;
                        if (m_network != nullptr && m_voiceChPeerId.size() > 0) {
                            LogMessage(LOG_HOST, "notifying FNE of VC registrations, peerId = %u", m_network->getPeerId());

                            std::vector<uint32_t> peers;
                            for (auto it : m_voiceChPeerId) {
                                peers.push_back(it.second);
                            }

                            m_network->announceSiteVCs(peers);
                        }

                        presenceNotifyTimer.start();
                    }
                }
            }

            if (m_state != STATE_IDLE)
                Thread::sleep(m_activeTickDelay);
            if (m_state == STATE_IDLE)
                Thread::sleep(m_idleTickDelay);
        }
    });

    if (!m_controlChData.address().empty() && m_controlChData.port() != 0 && m_network != nullptr) {
        presenceThread.run();
        presenceThread.setName("host:presence");
    }

    if (m_dmrCtrlChannel || m_p25CtrlChannel || m_nxdnCtrlChannel) {
        presenceThread.run();
        presenceThread.setName("host:presence");
    }

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

        if (!m_fixedMode) {
            if (m_modeTimer.isRunning() && m_modeTimer.hasExpired()) {
                setState(STATE_IDLE);
            }
        }
        else {
            m_modeTimer.stop();
            if (dmr != nullptr && m_state != STATE_DMR && !m_modem->hasTX()) {
                LogDebug(LOG_HOST, "fixed mode state abnormal, m_state = %u, state = %u", m_state, STATE_DMR);
                setState(STATE_DMR);
            }
            if (p25 != nullptr && m_state != STATE_P25 && !m_modem->hasTX()) {
                LogDebug(LOG_HOST, "fixed mode state abnormal, m_state = %u, state = %u", m_state, STATE_P25);
                setState(STATE_P25);
            }
            if (nxdn != nullptr && m_state != STATE_NXDN && !m_modem->hasTX()) {
                LogDebug(LOG_HOST, "fixed mode state abnormal, m_state = %u, state = %u", m_state, STATE_NXDN);
                setState(STATE_NXDN);
            }
        }

        m_mainLoopWatchdogTimer.start();
        m_mainLoopStage = 0U; // intentional magic number

        // scope is intentional
        {
            std::lock_guard<std::mutex> lock(clockingMutex);

            // ------------------------------------------------------
            //  -- Modem Clocking                                 --
            // ------------------------------------------------------

            ms = stopWatch.elapsed();
            stopWatch.start();

            m_modem->clock(ms);
            m_mainLoopStage = 1U; // intentional magic number
        }

        m_mainLoopMS = ms;

        // ------------------------------------------------------
        //  -- Read from Modem Processing                     --
        // ------------------------------------------------------

        /** Digital Mobile Radio */
        if (dmr != nullptr) {
            m_mainLoopStage = 2U; // intentional magic number

            // read DMR slot 1 frames from modem
            readFramesDMR1(dmr.get(), [&, this]() {
                if (dmr != nullptr) {
                    this->interruptDMRBeacon(dmr.get());
                }

                // if there is a P25 CC running; halt the CC
                if (p25 != nullptr) {
                    if (p25->getCCRunning() && !p25->getCCHalted()) {
                        this->interruptP25Control(p25.get());
                    }
                }

                // if there is a NXDN CC running; halt the CC
                if (nxdn != nullptr) {
                    if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                        this->interruptNXDNControl(nxdn.get());
                    }
                }
            });

            // read DMR slot 2 frames from modem
            readFramesDMR2(dmr.get(), [&, this]() {
                if (dmr != nullptr) {
                    this->interruptDMRBeacon(dmr.get());
                }

                // if there is a P25 CC running; halt the CC
                if (p25 != nullptr) {
                    if (p25->getCCRunning() && !p25->getCCHalted()) {
                        this->interruptP25Control(p25.get());
                    }
                }

                // if there is a NXDN CC running; halt the CC
                if (nxdn != nullptr) {
                    if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                        this->interruptNXDNControl(nxdn.get());
                    }
                }
            });
        }

        /** Project 25 */
        if (p25 != nullptr) {
            m_mainLoopStage = 3U; // intentional magic number

            // read P25 frames from modem
            readFramesP25(p25.get(), [&, this]() {
                if (dmr != nullptr) {
                    this->interruptDMRBeacon(dmr.get());
                }

                // if there is a NXDN CC running; halt the CC
                if (nxdn != nullptr) {
                    if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                        this->interruptNXDNControl(nxdn.get());
                    }
                }
            });
        }

        /** Next Generation Digital Narrowband */
        if (nxdn != nullptr) {
            m_mainLoopStage = 4U; // intentional magic number

            // read NXDN frames from modem
            readFramesNXDN(nxdn.get(), [&, this]() {
                if (dmr != nullptr) {
                    this->interruptDMRBeacon(dmr.get());
                }

                // if there is a P25 CC running; halt the CC
                if (p25 != nullptr) {
                    if (p25->getCCRunning() && !p25->getCCHalted()) {
                        this->interruptP25Control(p25.get());
                    }
                }
            });
        }

        // ------------------------------------------------------
        //  -- Network, DMR, and P25 Clocking                 --
        // ------------------------------------------------------

        if (m_network != nullptr) {
            m_mainLoopStage = 5U; // intentional magic number
            m_network->clock(ms);
        }

        if (dmr != nullptr) {
            m_mainLoopStage = 6U; // intentional magic number
            dmr->clock();

            if (m_dmrCtrlChannel) {
                if (!m_dmrDedicatedTxTestTimer.isRunning()) {
                    m_dmrDedicatedTxTestTimer.start();
                } else {
                    m_dmrDedicatedTxTestTimer.clock(ms);
                }
            }
        }

        if (p25 != nullptr) {
            m_mainLoopStage = 7U; // intentional magic number
            p25->clock();

            if (m_p25CtrlChannel) {
                if (!m_p25DedicatedTxTestTimer.isRunning()) {
                    m_p25DedicatedTxTestTimer.start();
                } else {
                    m_p25DedicatedTxTestTimer.clock(ms);
                }
            }
        }

        if (nxdn != nullptr) {
            m_mainLoopStage = 8U; // intentional magic number
            nxdn->clock();

            if (m_nxdnCtrlChannel) {
                if (!m_nxdnDedicatedTxTestTimer.isRunning()) {
                    m_nxdnDedicatedTxTestTimer.start();
                } else {
                    m_nxdnDedicatedTxTestTimer.clock(ms);
                }
            }
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
                std::lock_guard<std::mutex> lock(clockingMutex);

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
        if (dmr != nullptr) {
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
                        dmr->setCCRunning(true);
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
                    dmr->setCCRunning(false);
                }
            }

            // clock and check DMR Tx timer
            if (!m_dmrCtrlChannel) {
                m_dmrTXTimer.clock(ms);
                if (m_dmrTXTimer.isRunning() && m_dmrTXTimer.hasExpired()) {
                    m_modem->writeDMRStart(false);
                    m_dmrTXTimer.stop();
                }
            } else {
                m_dmrTXTimer.stop();
            }
        }

        /** Project 25 */
        if (p25 != nullptr) {
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

                            p25->setCCRunning(true);

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

                        p25->setCCRunning(false);

                        if (m_state == STATE_P25 && !m_modeTimer.isRunning()) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            m_modeTimer.start();
                        }
                    }
                }
            }
        }

        /** Next Generation Digital Narrowband */
        if (nxdn != nullptr) {
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

                            nxdn->setCCRunning(true);

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

                        nxdn->setCCRunning(false);

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
            // shutdown helper threads
            presenceThread.wait();
            siteDataUpdateThread.wait();

            if (dmr != nullptr) {
                dmrFrame1WriteThread.wait();
                dmrFrame2WriteThread.wait();
            }
            if (p25 != nullptr)
                p25FrameWriteThread.wait();
            if (nxdn != nullptr)
                nxdnFrameWriteThread.wait();

            watchdogThread.wait();

            if (dmr != nullptr) {
                if (m_state == STATE_DMR && m_duplex && m_modem->hasTX()) {
                    m_modem->writeDMRStart(false);
                    m_dmrTXTimer.stop();
                }

                if (m_dmrCtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearDMRFrame1();
                        m_modem->clearDMRFrame2();
                    }

                    dmr->setCCRunning(false);
                    dmr->setCCHalted(true);

                    m_dmrBeaconDurationTimer.stop();
                    dmrBeaconIntervalTimer.stop();
                }
            }

            if (p25 != nullptr) {
                if (m_p25CtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearP25Frame();
                        p25->reset();
                    }

                    p25->setCCRunning(false);

                    m_p25BcastDurationTimer.stop();
                    p25BcastIntervalTimer.stop();
                }
            }

            if (nxdn != nullptr) {
                if (m_nxdnCtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearNXDNFrame();
                        nxdn->reset();
                    }

                    nxdn->setCCRunning(false);

                    m_nxdnBcastDurationTimer.stop();
                    nxdnBcastIntervalTimer.stop();
                }
            }

            hasTxShutdown = true;
            if (!m_modem->hasTX()) {
                killed = true;
            }
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

/// <summary>
/// Helper to generate the status of the host in JSON format.
/// </summary>
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
    }

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

        uint8_t protoVer = m_modem->getVersion();
        modemInfo["protoVer"].set<uint8_t>(protoVer);

        response["modem"].set<json::object>(modemInfo);
    }

    return response;
}

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
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

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
bool Host::rmtPortModemClose(Modem* modem)
{
    assert(m_modemRemotePort != nullptr);

    m_modemRemotePort->close();

    // handled modem close
    return true;
}

/// <summary>
///
/// </summary>
/// <param name="modem"></param>
/// <param name="ms"></param>
/// <param name="rspType"></param>
/// <param name="rspDblLen"></param>
/// <param name="data"></param>
/// <param name="len"></param>
/// <returns></returns>
bool Host::rmtPortModemHandler(Modem* modem, uint32_t ms, modem::RESP_TYPE_DVM rspType, bool rspDblLen, const uint8_t* buffer, uint16_t len)
{
    assert(m_modemRemotePort != nullptr);

    if (rspType == RTM_OK && len > 0U) {
        if (modem->getTrace())
            Utils::dump(1U, "TX Remote Data", buffer, len);

        // send entire modem packet over the remote port
        m_modemRemotePort->write(buffer, len);
    }

    // read any data from the remote port for the air interface
    uint8_t data[BUFFER_LENGTH];
    ::memset(data, 0x00U, BUFFER_LENGTH);

    uint32_t ret = m_modemRemotePort->read(data, BUFFER_LENGTH);
    if (ret > 0) {
        if (modem->getTrace())
            Utils::dump(1U, "RX Remote Data", (uint8_t*)data, ret);

        if (ret < 3U) {
            LogError(LOG_MODEM, "Illegal length of remote data must be >3 bytes");
            Utils::dump("Buffer dump", data, ret);

            // handled modem response
            return true;
        }

        uint8_t len = data[1U];
        int ret = modem->write(data, len);
        if (ret != int(len))
            LogError(LOG_MODEM, "Error writing remote data");
    }

    // handled modem response
    return true;
}

/// <summary>
/// Helper to set the host/modem running state.
/// </summary>
/// <param name="state">Mode enumeration to switch the host/modem state to.</param>
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
            createLockFile("DMR");
            break;

        case STATE_P25:
            m_modem->setState(STATE_P25);
            m_state = STATE_P25;
            m_modeTimer.start();
            //m_cwIdTimer.stop();
            createLockFile("P25");
            break;

        case STATE_NXDN:
            m_modem->setState(STATE_NXDN);
            m_state = STATE_NXDN;
            m_modeTimer.start();
            //m_cwIdTimer.stop();
            createLockFile("NXDN");
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
            removeLockFile();
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
            removeLockFile();
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

            removeLockFile();
            m_modeTimer.stop();

            if (m_state == HOST_STATE_QUIT) {
                ::LogInfoEx(LOG_HOST, "Host is shutting down");

                if (m_modem != nullptr) {
                    m_modem->close();
                    delete m_modem;
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
                    m_tidLookup->stop();
                    delete m_tidLookup;
                }
                if (m_ridLookup != nullptr) {
                    m_ridLookup->stop();
                    delete m_ridLookup;
                }
            }
            else {
                m_state = STATE_IDLE;
            }
            break;
    }
}

/// <summary>
///
/// </summary>
/// <param name="state"></param>
void Host::createLockFile(const char* mode) const
{
    FILE* fp = ::fopen(g_lockFile.c_str(), "wt");
    if (fp != nullptr) {
        ::fprintf(fp, "%s\n", mode);
        ::fclose(fp);
    }
}

/// <summary>
///
/// </summary>
void Host::removeLockFile() const
{
    ::remove(g_lockFile.c_str());
}
