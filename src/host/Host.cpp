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
*   Copyright (C) 2017-2023 by Bryan Biedenkapp N2PLL
*   Copyright (C) 2021 by Nat Moore <https://github.com/jelimoore>
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
#include "dmr/Control.h"
#include "dmr/DMRUtils.h"
#include "p25/Control.h"
#include "p25/P25Utils.h"
#include "nxdn/Control.h"
#include "modem/port/ModemNullPort.h"
#include "modem/port/UARTPort.h"
#include "modem/port/PseudoPTYPort.h"
#include "modem/port/UDPPort.h"
#include "network/UDPSocket.h"
#include "lookups/RSSIInterpolator.h"
#include "host/Host.h"
#include "HostMain.h"
#include "Log.h"
#include "StopWatch.h"
#include "Thread.h"
#include "Utils.h"

using namespace network;
using namespace modem;
using namespace lookups;

#include <cstdio>
#include <cstdarg>
#include <algorithm>
#include <functional>
#include <vector>

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

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
    m_voiceChNo(),
    m_voiceChData(),
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
    m_siteId(1U),
    m_sysId(1U),
    m_dmrNetId(1U),
    m_dmrColorCode(1U),
    m_p25NAC(0x293U),
    m_p25PatchSuperGroup(0xFFFFU),
    m_p25NetId(0xBB800U),
    m_p25RfssId(1U),
    m_nxdnRAN(1U),
    m_dmrQueueSizeBytes(3960U), // 24 frames
    m_p25QueueSizeBytes(2592U), // 12 frames
    m_nxdnQueueSizeBytes(1488U), // 31 frames
    m_authoritative(true),
    m_supervisor(false),
    m_activeTickDelay(5U),
    m_idleTickDelay(5U),
    m_RESTAPI(nullptr)
{
    UDPSocket::startup();
}

/// <summary>
/// Finalizes a instance of the Host class.
/// </summary>
Host::~Host()
{
    UDPSocket::shutdown();
}

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
        ::fatal("cannot read the configuration file, %s", e.message());
    }

    bool m_daemon = m_conf["daemon"].as<bool>(false);
    if (m_daemon && g_foreground)
        m_daemon = false;

    // initialize system logging
    yaml::Node logConf = m_conf["log"];
    ret = ::LogInitialise(logConf["filePath"].as<std::string>(), logConf["fileRoot"].as<std::string>(),
        logConf["fileLevel"].as<uint32_t>(0U), logConf["displayLevel"].as<uint32_t>(0U));
    if (!ret) {
        ::fatal("unable to open the log file\n");
    }

    ret = ::ActivityLogInitialise(logConf["activityFilePath"].as<std::string>(), logConf["fileRoot"].as<std::string>());
    if (!ret) {
        ::fatal("unable to open the activity log file\n");
    }

#if !defined(_WIN32) && !defined(_WIN64)
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
#endif // !defined(_WIN32) && !defined(_WIN64)

    getHostVersion();
    ::LogInfo(">> Modem Controller");

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

    LogInfo("Talkgroup Id Lookups");
    LogInfo("    File: %s", tidLookupFile.length() > 0U ? tidLookupFile.c_str() : "None");
    if (tidReloadTime > 0U)
        LogInfo("    Reload: %u mins", tidReloadTime);
    LogInfo("    ACL: %s", tidAcl ? "yes" : "no");

    m_tidLookup = new TalkgroupIdLookup(tidLookupFile, tidReloadTime, tidAcl);
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
    Timer dmrBeaconDurationTimer(1000U);

    std::unique_ptr<dmr::Control> dmr = nullptr;
#if defined(ENABLE_DMR)
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

            dmrBeaconDurationTimer.setTimeout(dmrBeaconDuration);

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

        dmr = std::unique_ptr<dmr::Control>(new dmr::Control(m_authoritative, m_dmrColorCode, callHang, m_dmrQueueSizeBytes, embeddedLCOnly, dumpTAData, m_timeout, m_rfTalkgroupHang,
            m_modem, m_network, m_duplex, m_ridLookup, m_tidLookup, m_idenTable, rssi, jitter, dmrDumpDataPacket, dmrRepeatDataPacket,
            dmrDumpCsbkData, dmrDebug, dmrVerbose));
        dmr->setOptions(m_conf, m_supervisor, m_voiceChNo, m_voiceChData, m_dmrNetId, m_siteId, m_channelId, m_channelNo, true);

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
#endif // defined(ENABLE_DMR)

    // initialize P25
    Timer p25BcastIntervalTimer(1000U);
    Timer p25BcastDurationTimer(1000U);

    std::unique_ptr<p25::Control> p25 = nullptr;
#if defined(ENABLE_P25)
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

            p25BcastDurationTimer.setTimeout(p25ControlBcstDuration);

            p25BcastIntervalTimer.setTimeout(p25ControlBcstInterval);
            p25BcastIntervalTimer.start();

            m_p25CtrlBroadcast = p25CtrlBroadcast;
            if (p25CtrlBroadcast) {
                g_fireP25Control = true;
            }
        }

        p25 = std::unique_ptr<p25::Control>(new p25::Control(m_authoritative, m_p25NAC, callHang, m_p25QueueSizeBytes, m_modem, m_network, m_timeout, m_rfTalkgroupHang,
            m_duplex, m_ridLookup, m_tidLookup, m_idenTable, rssi, p25DumpDataPacket, p25RepeatDataPacket,
            p25DumpTsbkData, p25Debug, p25Verbose));
        p25->setOptions(m_conf, m_supervisor, m_cwCallsign, m_voiceChNo, m_voiceChData, m_p25PatchSuperGroup, m_p25NetId, m_sysId, m_p25RfssId,
            m_siteId, m_channelId, m_channelNo, true);

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
#endif // defined(ENABLE_P25)

    // initialize NXDN
    Timer nxdnBcastIntervalTimer(1000U);
    Timer nxdnBcastDurationTimer(1000U);

    std::unique_ptr<nxdn::Control> nxdn = nullptr;
#if defined(ENABLE_NXDN)
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

            nxdnBcastDurationTimer.setTimeout(nxdnControlBcstDuration);

            nxdnBcastIntervalTimer.setTimeout(nxdnControlBcstInterval);
            nxdnBcastIntervalTimer.start();

            m_nxdnCtrlBroadcast = nxdnCtrlBroadcast;
            if (nxdnCtrlBroadcast) {
                g_fireNXDNControl = true;
            }
        }

        nxdn = std::unique_ptr<nxdn::Control>(new nxdn::Control(m_authoritative, m_nxdnRAN, callHang, m_nxdnQueueSizeBytes, m_timeout, m_rfTalkgroupHang,
            m_modem, m_network, m_duplex, m_ridLookup, m_tidLookup, m_idenTable, rssi,
            nxdnDumpRcchData, nxdnDebug, nxdnVerbose));
        nxdn->setOptions(m_conf, m_supervisor, m_cwCallsign, m_voiceChNo, m_voiceChData, m_siteId, m_sysId, m_channelId, m_channelNo, true);

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
#endif // defined(ENABLE_NXDN)

    if (!m_dmrEnabled && !m_p25Enabled && !m_nxdnEnabled) {
        ::LogError(LOG_HOST, "No modes enabled? DMR, P25 and/or NXDN must be enabled!");
        g_killed = true;
    }

    if (m_fixedMode && ((m_dmrEnabled && m_p25Enabled) || (m_dmrEnabled && m_nxdnEnabled) || (m_p25Enabled && m_nxdnEnabled))) {
        ::LogError(LOG_HOST, "Cannot have DMR, P25 and NXDN when using fixed state! Choose one protocol for fixed state operation.");
        g_killed = true;
    }

#if defined(ENABLE_P25) && defined(ENABLE_DFSI)
    // DFSI checks
    if (m_useDFSI && m_dmrEnabled) {
        ::LogError(LOG_HOST, "Cannot have DMR enabled when using DFSI!");
        g_killed = true;
    }

    if (m_useDFSI && m_nxdnEnabled) {
        ::LogError(LOG_HOST, "Cannot have NXDN enabled when using DFSI!");
        g_killed = true;
    }
#endif // defined(ENABLE_P25) && defined(ENABLE_DFSI)

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
#if defined(ENABLE_DMR)
            if (dmr != nullptr)
                setState(STATE_DMR);
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
            if (p25 != nullptr)
                setState(STATE_P25);
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
            if (nxdn != nullptr)
                setState(STATE_NXDN);
#endif // defined(ENABLE_NXDN)
        }
        else {
#if defined(ENABLE_DMR)
            if (m_dmrCtrlChannel) {
                m_fixedMode = true;
                setState(STATE_DMR);
            }
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
            if (m_p25CtrlChannel) {
                m_fixedMode = true;
                setState(STATE_P25);
            }
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
            if (m_nxdnCtrlChannel) {
                m_fixedMode = true;
                setState(STATE_NXDN);
            }
#endif // defined(ENABLE_NXDN)

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

    // Macro to interrupt a running P25 control channel transmission
    #define INTERRUPT_P25_CONTROL                                                                                       \
        if (p25 != nullptr) {                                                                                           \
            LogDebug(LOG_HOST, "interrupt P25 control, m_state = %u", m_state);                                         \
            p25->setCCHalted(true);                                                                                     \
            if (p25BcastDurationTimer.isRunning() && !p25BcastDurationTimer.isPaused()) {                               \
                p25BcastDurationTimer.pause();                                                                          \
            }                                                                                                           \
        }

    // Macro to interrupt a running DMR roaming beacon
    #define INTERRUPT_DMR_BEACON                                                                                        \
        if (dmr != nullptr) {                                                                                           \
            if (dmrBeaconDurationTimer.isRunning() && !dmrBeaconDurationTimer.hasExpired()) {                           \
                if (m_dmrTSCCData && !m_dmrCtrlChannel) {                                                               \
                    LogDebug(LOG_HOST, "interrupt DMR control, m_state = %u", m_state);                                 \
                    dmr->setCCHalted(true);                                                                             \
                    dmr->setCCRunning(false);                                                                           \
                }                                                                                                       \
            }                                                                                                           \
            dmrBeaconDurationTimer.stop();                                                                              \
        }

    // Macro to interrupt a running NXDN control channel transmission
    #define INTERRUPT_NXDN_CONTROL                                                                                      \
        if (nxdn != nullptr) {                                                                                          \
            LogDebug(LOG_HOST, "interrupt NXDN control, m_state = %u", m_state);                                        \
            nxdn->setCCHalted(true);                                                                                    \
            if (nxdnBcastDurationTimer.isRunning() && !nxdnBcastDurationTimer.isPaused()) {                             \
                nxdnBcastDurationTimer.pause();                                                                         \
            }                                                                                                           \
        }

    // Macro to start DMR duplex idle transmission (or beacon)
    #define START_DMR_DUPLEX_IDLE(x)                                                                                    \
        if (dmr != nullptr) {                                                                                           \
            if (m_duplex) {                                                                                             \
                m_modem->writeDMRStart(x);                                                                              \
                m_dmrTXTimer.start();                                                                                   \
            }                                                                                                           \
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

        uint8_t data[220U];
        uint32_t len;
        bool ret;

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

        // ------------------------------------------------------
        //  -- Write to Modem Processing                      --
        // ------------------------------------------------------

        /** Digital Mobile Radio */
#if defined(ENABLE_DMR)
        if (dmr != nullptr) {
            // check if there is space on the modem for DMR slot 1 frames,
            // if there is read frames from the DMR controller and write it
            // to the modem
            ret = m_modem->hasDMRSpace1();
            if (ret) {
                len = dmr->getFrame(1U, data);
                if (len > 0U) {
                    // if the state is idle; set to DMR, start mode timer and start DMR idle frames
                    if (m_state == STATE_IDLE) {
                        m_modeTimer.setTimeout(m_netModeHang);
                        setState(STATE_DMR);
                        START_DMR_DUPLEX_IDLE(true);
                    }

                    // if the state is DMR; start DMR idle frames and write DMR slot 1 data
                    if (m_state == STATE_DMR) {
                        START_DMR_DUPLEX_IDLE(true);

                        m_modem->writeDMRData1(data, len);

                        // if there is no DMR CC running; run the interrupt macro to stop
                        // any running DMR beacon
                        if (!dmr->getCCRunning()) {
                            INTERRUPT_DMR_BEACON;
                        }

                        // if there is a P25 CC running; halt the CC
                        if (p25 != nullptr) {
                            if (p25->getCCRunning() && !p25->getCCHalted()) {
                                INTERRUPT_P25_CONTROL;
                            }
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (nxdn != nullptr) {
                            if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                                INTERRUPT_NXDN_CONTROL;
                            }
                        }

                        m_modeTimer.start();
                    }
                }
            }

            // check if there is space on the modem for DMR slot 2 frames,
            // if there is read frames from the DMR controller and write it
            // to the modem
            ret = m_modem->hasDMRSpace2();
            if (ret) {
                len = dmr->getFrame(2U, data);
                if (len > 0U) {
                    // if the state is idle; set to DMR, start mode timer and start DMR idle frames
                    if (m_state == STATE_IDLE) {
                        m_modeTimer.setTimeout(m_netModeHang);
                        setState(STATE_DMR);
                        START_DMR_DUPLEX_IDLE(true);
                    }

                    // if the state is DMR; start DMR idle frames and write DMR slot 2 data
                    if (m_state == STATE_DMR) {
                        START_DMR_DUPLEX_IDLE(true);

                        m_modem->writeDMRData2(data, len);

                        // if there is no DMR CC running; run the interrupt macro to stop
                        // any running DMR beacon
                        if (!dmr->getCCRunning()) {
                            INTERRUPT_DMR_BEACON;
                        }

                        // if there is a P25 CC running; halt the CC
                        if (p25 != nullptr) {
                            if (p25->getCCRunning() && !p25->getCCHalted()) {
                                INTERRUPT_P25_CONTROL;
                            }
                        }

                        // if there is a NXDN CC running; halt the CC
                        if (nxdn != nullptr) {
                            if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                                INTERRUPT_NXDN_CONTROL;
                            }
                        }

                        m_modeTimer.start();
                    }
                }
            }
        }
#endif // defined(ENABLE_DMR)

        /** Project 25 */
#if defined(ENABLE_P25)
        // check if there is space on the modem for P25 frames,
        // if there is read frames from the P25 controller and write it
        // to the modem
        if (p25 != nullptr) {
            uint8_t nextLen = p25->peekFrameLength();
            if (nextLen > 0U) {
                ret = m_modem->hasP25Space(nextLen);
                if (ret) {
                    len = p25->getFrame(data);
                    if (len > 0U) {
                        // if the state is idle; set to P25 and start mode timer
                        if (m_state == STATE_IDLE) {
                            m_modeTimer.setTimeout(m_netModeHang);
                            setState(STATE_P25);
                        }

                        // if the state is P25; write P25 data
                        if (m_state == STATE_P25) {
                            m_modem->writeP25Data(data, len);

                            INTERRUPT_DMR_BEACON;

                            // if there is a NXDN CC running; halt the CC
                            if (nxdn != nullptr) {
                                if (nxdn->getCCRunning() && !nxdn->getCCHalted()) {
                                    INTERRUPT_NXDN_CONTROL;
                                }
                            }

                            m_modeTimer.start();
                        }
                    }
                    else {
                        nextLen = 0U;
                    }
                }
            }

            if (nextLen == 0U) {
                // if we have no P25 data, and we're either idle or P25 state, check if we
                // need to be starting the CC running flag or writing end of voice call data
                if (m_state == STATE_IDLE || m_state == STATE_P25) {
                    if (p25->getCCHalted()) {
                        p25->setCCHalted(false);
                    }

                    // write end of voice if necessary
                    ret = p25->writeRF_VoiceEnd();
                    if (ret) {
                        if (m_state == STATE_IDLE) {
                            m_modeTimer.setTimeout(m_netModeHang);
                            setState(STATE_P25);
                        }

                        if (m_state == STATE_P25) {
                            m_modeTimer.start();
                        }
                    }
                }
            }

            // if the modem is in duplex -- handle P25 CC burst control
            if (m_duplex) {
                if (p25BcastDurationTimer.isPaused() && !p25->getCCHalted()) {
                    p25BcastDurationTimer.resume();
                }

                if (p25->getCCHalted()) {
                    p25->setCCHalted(false);
                }

                if (g_fireP25Control) {
                    m_modeTimer.stop();
                }
            }
        }
#endif // defined(ENABLE_P25)

        /** Next Generation Digital Narrowband */
#if defined(ENABLE_NXDN)
        // check if there is space on the modem for NXDN frames,
        // if there is read frames from the NXDN controller and write it
        // to the modem
        if (nxdn != nullptr) {
            ret = m_modem->hasNXDNSpace();
            if (ret) {
                len = nxdn->getFrame(data);
                if (len > 0U) {
                    // if the state is idle; set to NXDN and start mode timer
                    if (m_state == STATE_IDLE) {
                        m_modeTimer.setTimeout(m_netModeHang);
                        setState(STATE_NXDN);
                    }

                    // if the state is NXDN; write NXDN data
                    if (m_state == STATE_NXDN) {
                        m_modem->writeNXDNData(data, len);

                        INTERRUPT_DMR_BEACON;

                        // if there is a P25 CC running; halt the CC
                        if (p25 != nullptr) {
                            if (p25->getCCRunning() && !p25->getCCHalted()) {
                                INTERRUPT_P25_CONTROL;
                            }
                        }

                        m_modeTimer.start();
                    }
                }
            }
        }
#endif // defined(ENABLE_NXDN)

        // ------------------------------------------------------
        //  -- Modem Clocking                                 --
        // ------------------------------------------------------

        ms = stopWatch.elapsed();
        stopWatch.start();

        m_modem->clock(ms);

        // ------------------------------------------------------
        //  -- Read from Modem Processing                     --
        // ------------------------------------------------------

        /** Digital Mobile Radio */
#if defined(ENABLE_DMR)
        if (dmr != nullptr) {
            // read DMR slot 1 frames from the modem, and if there is any
            // write those frames to the DMR controller
            len = m_modem->readDMRData1(data);
            if (len > 0U) {
                if (m_state == STATE_IDLE) {
                    // if the modem is in duplex -- process wakeup CSBKs
                    if (m_duplex) {
                        bool ret = dmr->processWakeup(data);
                        if (ret) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            setState(STATE_DMR);

                            START_DMR_DUPLEX_IDLE(true);

                            INTERRUPT_DMR_BEACON;
                            INTERRUPT_P25_CONTROL;
                            INTERRUPT_NXDN_CONTROL;
                        }
                    }
                    else {
                        // in simplex directly process slot 1 frames
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setState(STATE_DMR);
                        START_DMR_DUPLEX_IDLE(true);

                        dmr->processFrame(1U, data, len);

                        INTERRUPT_DMR_BEACON;
                        INTERRUPT_P25_CONTROL;
                        INTERRUPT_NXDN_CONTROL;
                    }
                }
                else if (m_state == STATE_DMR) {
                    // if the modem is in duplex, and hasn't started transmitting
                    // process wakeup CSBKs
                    if (m_duplex && !m_modem->hasTX()) {
                        bool ret = dmr->processWakeup(data);
                        if (ret) {
                            m_modem->writeDMRStart(true);
                            m_dmrTXTimer.start();
                        }
                    }
                    else {
                        // process slot 1 frames
                        bool ret = dmr->processFrame(1U, data, len);
                        if (ret) {
                            INTERRUPT_DMR_BEACON;
                            INTERRUPT_P25_CONTROL;
                            INTERRUPT_NXDN_CONTROL;

                            m_modeTimer.start();
                            if (m_duplex)
                                m_dmrTXTimer.start();
                        }
                    }
                }
                else if (m_state != HOST_STATE_LOCKOUT) {
                    LogWarning(LOG_HOST, "DMR modem data received, state = %u", m_state);
                }
            }

            // read DMR slot 2 frames from the modem, and if there is any
            // write those frames to the DMR controller
            len = m_modem->readDMRData2(data);
            if (len > 0U) {
                if (m_state == STATE_IDLE) {
                    // if the modem is in duplex -- process wakeup CSBKs
                    if (m_duplex) {
                        bool ret = dmr->processWakeup(data);
                        if (ret) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            setState(STATE_DMR);
                            START_DMR_DUPLEX_IDLE(true);

                            INTERRUPT_DMR_BEACON;
                            INTERRUPT_P25_CONTROL;
                            INTERRUPT_NXDN_CONTROL;
                        }
                    }
                    else {
                        // in simplex -- directly process slot 2 frames
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setState(STATE_DMR);
                        START_DMR_DUPLEX_IDLE(true);

                        dmr->processFrame(2U, data, len);

                        INTERRUPT_DMR_BEACON;
                        INTERRUPT_P25_CONTROL;
                        INTERRUPT_NXDN_CONTROL;
                    }
                }
                else if (m_state == STATE_DMR) {
                    // if the modem is in duplex, and hasn't started transmitting
                    // process wakeup CSBKs
                    if (m_duplex && !m_modem->hasTX()) {
                        bool ret = dmr->processWakeup(data);
                        if (ret) {
                            m_modem->writeDMRStart(true);
                            m_dmrTXTimer.start();
                        }
                    }
                    else {
                        // process slot 2 frames
                        bool ret = dmr->processFrame(2U, data, len);
                        if (ret) {
                            INTERRUPT_DMR_BEACON;
                            INTERRUPT_P25_CONTROL;
                            INTERRUPT_NXDN_CONTROL;

                            m_modeTimer.start();
                            if (m_duplex)
                                m_dmrTXTimer.start();
                        }
                    }
                }
                else if (m_state != HOST_STATE_LOCKOUT) {
                    LogWarning(LOG_HOST, "DMR modem data received, state = %u", m_state);
                }
            }
        }
#endif // defined(ENABLE_DMR)

        /** Project 25 */
#if defined(ENABLE_P25)
        // read P25 frames from modem, and if there are frames
        // write those frames to the P25 controller
        if (p25 != nullptr) {
            len = m_modem->readP25Data(data);
            if (len > 0U) {
                if (m_state == STATE_IDLE) {
                    // process P25 frames
                    bool ret = p25->processFrame(data, len);
                    if (ret) {
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setState(STATE_P25);

                        INTERRUPT_DMR_BEACON;
                        //INTERRUPT_P25_CONTROL;
                        INTERRUPT_NXDN_CONTROL;
                    }
                    else {
                        ret = p25->writeRF_VoiceEnd();
                        if (ret) {
                            INTERRUPT_DMR_BEACON;
                            INTERRUPT_NXDN_CONTROL;

                            if (m_state == STATE_IDLE) {
                                m_modeTimer.setTimeout(m_rfModeHang);
                                setState(STATE_P25);
                            }

                            if (m_state == STATE_P25) {
                                m_modeTimer.start();
                            }

                            // if the modem is in duplex -- handle P25 CC burst control
                            if (m_duplex) {
                                if (p25BcastDurationTimer.isPaused() && !p25->getCCHalted()) {
                                    p25BcastDurationTimer.resume();
                                }

                                if (p25->getCCHalted()) {
                                    p25->setCCHalted(false);
                                }

                                if (g_fireP25Control) {
                                    m_modeTimer.stop();
                                }
                            }
                            else {
                                p25BcastDurationTimer.stop();
                            }
                        }
                    }
                }
                else if (m_state == STATE_P25) {
                    // process P25 frames
                    bool ret = p25->processFrame(data, len);
                    if (ret) {
                        m_modeTimer.start();
                        //INTERRUPT_P25_CONTROL;
                    }
                    else {
                        ret = p25->writeRF_VoiceEnd();
                        if (ret) {
                            m_modeTimer.start();
                        }
                    }
                }
                else if (m_state != HOST_STATE_LOCKOUT) {
                    LogWarning(LOG_HOST, "P25 modem data received, state = %u", m_state);
                }
            }
        }
#endif // defined(ENABLE_P25)

        /** Next Generation Digital Narrowband */
        // read NXDN frames from modem, and if there are frames
        // write those frames to the NXDN controller
#if defined(ENABLE_NXDN)
        if (nxdn != nullptr) {
            len = m_modem->readNXDNData(data);
            if (len > 0U) {
                if (m_state == STATE_IDLE) {
                    // process NXDN frames
                    bool ret = nxdn->processFrame(data, len);
                    if (ret) {
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setState(STATE_NXDN);

                        INTERRUPT_DMR_BEACON;
                        INTERRUPT_P25_CONTROL;
                    }
                }
                else if (m_state == STATE_NXDN) {
                    // process NXDN frames
                    bool ret = nxdn->processFrame(data, len);
                    if (ret) {
                        m_modeTimer.start();
                        //INTERRUPT_NXDN_CONTROL;
                    }
                }
                else if (m_state != HOST_STATE_LOCKOUT) {
                    LogWarning(LOG_HOST, "NXDN modem data received, state = %u", m_state);
                }
            }
        }
#endif // defined(ENABLE_NXDN)

        // ------------------------------------------------------
        //  -- Network, DMR, and P25 Clocking                 --
        // ------------------------------------------------------

        if (m_network != nullptr)
            m_network->clock(ms);

#if defined(ENABLE_DMR)
        if (dmr != nullptr)
            dmr->clock(ms);
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
        if (p25 != nullptr)
            p25->clock(ms);
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
        if (nxdn != nullptr)
            nxdn->clock(ms);
#endif // defined(ENABLE_NXDN)

        // ------------------------------------------------------
        //  -- Timer Clocking                                 --
        // ------------------------------------------------------

        // clock and check CW timer
        m_cwIdTimer.clock(ms);
        if (m_cwIdTimer.isRunning() && m_cwIdTimer.hasExpired()) {
            if (!m_modem->hasTX() && !m_p25CtrlChannel && !m_dmrCtrlChannel) {
                if (dmrBeaconDurationTimer.isRunning() || p25BcastDurationTimer.isRunning()) {
                    LogDebug(LOG_HOST, "CW, beacon or CC timer running, ceasing");

                    dmrBeaconDurationTimer.stop();
                    p25BcastDurationTimer.stop();
                }

                LogDebug(LOG_HOST, "CW, start transmitting");

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

                    m_modem->clock(ms);

                    if (!first && !m_modem->hasTX()) {
                        LogDebug(LOG_HOST, "CW, finished transmitting");
                        break;
                    }

                    if (first) {
                        first = false;
                        Thread::sleep(200U + CW_IDLE_SLEEP_MS); // ~250ms; poll time of the modem
                    }
                    else
                        Thread::sleep(CW_IDLE_SLEEP_MS);
                } while (true);

                m_cwIdTimer.setTimeout(m_cwIdTime);
                m_cwIdTimer.start();
            }
        }

        /** Digial Mobile Radio */
#if defined(ENABLE_DMR)
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
                        LogDebug(LOG_HOST, "DMR, start CC broadcast");
                    }
                    else {
                        LogDebug(LOG_HOST, "DMR, roaming beacon burst");
                    }
                    dmrBeaconIntervalTimer.start();
                    dmrBeaconDurationTimer.start();
                }
            }

            // clock and check DMR roaming beacon duration timer
            dmrBeaconDurationTimer.clock(ms);
            if (dmrBeaconDurationTimer.isRunning() && dmrBeaconDurationTimer.hasExpired()) {
                dmrBeaconDurationTimer.stop();

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
            m_dmrTXTimer.clock(ms);
            if (m_dmrTXTimer.isRunning() && m_dmrTXTimer.hasExpired()) {
                m_modem->writeDMRStart(false);
                m_dmrTXTimer.stop();
            }
        }
#endif // defined(ENABLE_DMR)

        /** Project 25 */
#if defined(ENABLE_P25)
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

                            p25->writeAdjSSNetwork();
                            p25->setCCRunning(true);

                            // hide this message for continuous CC -- otherwise display every time we process
                            if (!m_p25CtrlChannel) {
                                LogMessage(LOG_HOST, "P25, start CC broadcast");
                            }

                            g_fireP25Control = false;
                            p25BcastIntervalTimer.start();
                            p25BcastDurationTimer.start();

                            // if the CC is continuous -- clock one cycle into the duration timer
                            if (m_p25CtrlChannel) {
                                p25BcastDurationTimer.clock(ms);
                            }
                        }
                    }

                    if (p25BcastDurationTimer.isPaused()) {
                        p25BcastDurationTimer.resume();
                    }

                    // clock and check P25 CC broadcast duration timer
                    p25BcastDurationTimer.clock(ms);
                    if (p25BcastDurationTimer.isRunning() && p25BcastDurationTimer.hasExpired()) {
                        p25BcastDurationTimer.stop();

                        p25->setCCRunning(false);

                        if (m_state == STATE_P25 && !m_modeTimer.isRunning()) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            m_modeTimer.start();
                        }
                    }
                }
                else {
                    // simply use the P25 CC interval timer in a non-broadcast state to transmit adjacent site data over
                    // the network
                    if (p25BcastIntervalTimer.isRunning() && p25BcastIntervalTimer.hasExpired()) {
                        if ((m_state == STATE_IDLE || m_state == STATE_P25) && !m_modem->hasTX()) {
                            p25->writeAdjSSNetwork();
                            p25BcastIntervalTimer.start();
                        }
                    }
                }
            }
        }
#endif // defined(ENABLE_P25)

        /** Next Generation Digital Narrowband */
#if defined(ENABLE_NXDN)
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

                            //nxdn->writeAdjSSNetwork();
                            nxdn->setCCRunning(true);

                            // hide this message for continuous CC -- otherwise display every time we process
                            if (!m_nxdnCtrlChannel) {
                                LogMessage(LOG_HOST, "NXDN, start CC broadcast");
                            }

                            g_fireNXDNControl = false;
                            nxdnBcastIntervalTimer.start();
                            nxdnBcastDurationTimer.start();

                            // if the CC is continuous -- clock one cycle into the duration timer
                            if (m_nxdnCtrlChannel) {
                                nxdnBcastDurationTimer.clock(ms);
                            }
                        }
                    }

                    if (nxdnBcastDurationTimer.isPaused()) {
                        nxdnBcastDurationTimer.resume();
                    }

                    // clock and check NXDN CC broadcast duration timer
                    nxdnBcastDurationTimer.clock(ms);
                    if (nxdnBcastDurationTimer.isRunning() && nxdnBcastDurationTimer.hasExpired()) {
                        nxdnBcastDurationTimer.stop();

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
                            //nxdn->writeAdjSSNetwork();
                            nxdnBcastIntervalTimer.start();
                        }
                    }
                }
            }
        }
#endif // defined(ENABLE_NXDN)

        if (g_killed) {
#if defined(ENABLE_DMR)
            if (dmr != nullptr) {
                if (m_dmrCtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearDMRData1();
                        m_modem->clearDMRData2();
                    }

                    dmr->setCCRunning(false);
                    dmr->setCCHalted(true);

                    dmrBeaconDurationTimer.stop();
                    dmrBeaconIntervalTimer.stop();
                }
            }
#endif // defined(ENABLE_DMR)

#if defined(ENABLE_P25)
            if (p25 != nullptr) {
                if (m_p25CtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearP25Data();
                        p25->reset();
                    }

                    p25->setCCRunning(false);

                    p25BcastDurationTimer.stop();
                    p25BcastIntervalTimer.stop();
                }
            }
#endif // defined(ENABLE_P25)

#if defined(ENABLE_NXDN)
            if (nxdn != nullptr) {
                if (m_nxdnCtrlChannel) {
                    if (!hasTxShutdown) {
                        m_modem->clearNXDNData();
                        nxdn->reset();
                    }

                    nxdn->setCCRunning(false);

                    nxdnBcastDurationTimer.stop();
                    nxdnBcastIntervalTimer.stop();
                }
            }
#endif // defined(ENABLE_NXDN)

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
/// Reads basic configuration parameters from the YAML configuration file.
/// </summary>
bool Host::readParams()
{
    yaml::Node modemConf = m_conf["system"]["modem"];

    yaml::Node modemProtocol = modemConf["protocol"];
    std::string portType = modemProtocol["type"].as<std::string>("null");

    yaml::Node udpProtocol = modemProtocol["udp"];
    std::string udpMode = udpProtocol["mode"].as<std::string>("master");

    bool udpMasterMode = false;
    std::transform(portType.begin(), portType.end(), portType.begin(), ::tolower);
    if ((portType == UART_PORT || portType == PTY_PORT) && g_remoteModemMode) {
        udpMasterMode = true;
    }

    yaml::Node protocolConf = m_conf["protocols"];
#if defined(ENABLE_DMR)
    m_dmrEnabled = protocolConf["dmr"]["enable"].as<bool>(false);
#else
    m_dmrEnabled = false; // hardcode to false when no DMR support is compiled in
#endif // defined(ENABLE_DMR)
#if defined(ENABLE_P25)
    m_p25Enabled = protocolConf["p25"]["enable"].as<bool>(false);
#else
    m_p25Enabled = false; // hardcode to false when no P25 support is compiled in
#endif // defined(ENABLE_P25)
#if defined(ENABLE_NXDN)
    m_nxdnEnabled = protocolConf["nxdn"]["enable"].as<bool>(false);
#else
    m_nxdnEnabled = false; // hardcode to false when no NXDN support is compiled in
#endif // defined(ENABLE_NXDN)

    yaml::Node systemConf = m_conf["system"];
    m_duplex = systemConf["duplex"].as<bool>(true);
    bool simplexSameFreq = systemConf["simplexSameFrequency"].as<bool>(false);

    m_timeout = systemConf["timeout"].as<uint32_t>(120U);
    m_rfModeHang = systemConf["rfModeHang"].as<uint32_t>(10U);
    m_rfTalkgroupHang = systemConf["rfTalkgroupHang"].as<uint32_t>(10U);
    m_netModeHang = systemConf["netModeHang"].as<uint32_t>(3U);
    if (!systemConf["modeHang"].isNone()) {
        m_rfModeHang = m_netModeHang = systemConf["modeHang"].as<uint32_t>();
    }

    m_activeTickDelay = (uint8_t)systemConf["activeTickDelay"].as<uint32_t>(5U);
    if (m_activeTickDelay < 1U)
        m_activeTickDelay = 1U;
    m_idleTickDelay = (uint8_t)systemConf["idleTickDelay"].as<uint32_t>(5U);
    if (m_idleTickDelay < 1U)
        m_idleTickDelay = 1U;

    m_identity = systemConf["identity"].as<std::string>();
    m_fixedMode = systemConf["fixedMode"].as<bool>(false);

    if (m_identity.length() > 8) {
        std::string identity = m_identity;
        m_identity = identity.substr(0, 8);

        ::LogWarning(LOG_HOST, "System Identity \"%s\" is too long; truncating to 8 characters, \"%s\".", identity.c_str(), m_identity.c_str());
    }

    int8_t lto = (int8_t)systemConf["localTimeOffset"].as<int32_t>(0);

    removeLockFile();

    LogInfo("General Parameters");
    if (!udpMasterMode) {
        LogInfo("    DMR: %s", m_dmrEnabled ? "enabled" : "disabled");
        LogInfo("    P25: %s", m_p25Enabled ? "enabled" : "disabled");
        LogInfo("    NXDN: %s", m_nxdnEnabled ? "enabled" : "disabled");
        LogInfo("    Duplex: %s", m_duplex ? "yes" : "no");
        if (!m_duplex) {
            LogInfo("    Simplex Same Frequency: %s", simplexSameFreq ? "yes" : "no");
        }
        LogInfo("    Active Tick Delay: %ums", m_activeTickDelay);
        LogInfo("    Idle Tick Delay: %ums", m_idleTickDelay);
        LogInfo("    Timeout: %us", m_timeout);
        LogInfo("    RF Mode Hang: %us", m_rfModeHang);
        LogInfo("    RF Talkgroup Hang: %us", m_rfTalkgroupHang);
        LogInfo("    Net Mode Hang: %us", m_netModeHang);
        LogInfo("    Identity: %s", m_identity.c_str());
        LogInfo("    Fixed Mode: %s", m_fixedMode ? "yes" : "no");
        LogInfo("    Lock Filename: %s", g_lockFile.c_str());
        LogInfo("    Local Time Offset: %dh", lto);

        yaml::Node systemInfo = systemConf["info"];
        m_latitude = systemInfo["latitude"].as<float>(0.0F);
        m_longitude = systemInfo["longitude"].as<float>(0.0F);
        m_height = systemInfo["height"].as<int>(0);
        m_power = systemInfo["power"].as<uint32_t>(0U);
        m_location = systemInfo["location"].as<std::string>();

        LogInfo("System Info Parameters");
        LogInfo("    Latitude: %fdeg N", m_latitude);
        LogInfo("    Longitude: %fdeg E", m_longitude);
        LogInfo("    Height: %um", m_height);
        LogInfo("    Power: %uW", m_power);
        LogInfo("    Location: \"%s\"", m_location.c_str());

        // try to load bandplan identity table
        std::string idenLookupFile = systemConf["iden_table"]["file"].as<std::string>();
        uint32_t idenReloadTime = systemConf["iden_table"]["time"].as<uint32_t>(0U);

        if (idenLookupFile.length() <= 0U) {
            ::LogError(LOG_HOST, "No bandplan identity table? This must be defined!");
            return false;
        }

        LogInfo("Iden Table Lookups");
        LogInfo("    File: %s", idenLookupFile.length() > 0U ? idenLookupFile.c_str() : "None");
        if (idenReloadTime > 0U)
            LogInfo("    Reload: %u mins", idenReloadTime);

        m_idenTable = new IdenTableLookup(idenLookupFile, idenReloadTime);
        m_idenTable->read();

        yaml::Node rfssConfig = systemConf["config"];
        m_channelId = (uint8_t)rfssConfig["channelId"].as<uint32_t>(0U);
        if (m_channelId > 15U) { // clamp to 15
            m_channelId = 15U;
        }

        IdenTable entry = m_idenTable->find(m_channelId);
        if (entry.baseFrequency() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid base frequency.", m_channelId);
            return false;
        }

        m_channelNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);
        if (m_channelNo == 0U) { // clamp to 1
            m_channelNo = 1U;
        }
        if (m_channelNo > 4095U) { // clamp to 4095
            m_channelNo = 4095U;
        }

        if (entry.txOffsetMhz() == 0U) {
            ::LogError(LOG_HOST, "Channel Id %u has an invalid Tx offset.", m_channelId);
            return false;
        }

        uint32_t calcSpace = (uint32_t)(entry.chSpaceKhz() / 0.125);
        float calcTxOffset = entry.txOffsetMhz() * 1000000;

        m_txFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)));
        m_rxFrequency = (uint32_t)(m_txFrequency + calcTxOffset);

        if (calcTxOffset < 0.0f && m_rxFrequency < entry.baseFrequency()) {
            ::LogError(LOG_HOST, "Channel Id %u Channel No $%04X has an invalid frequency. Rx Frequency (%u) is less then the base frequency (%u), all frequencies must be higher then the base frequency.", m_channelId, m_channelNo,
                m_rxFrequency, entry.baseFrequency());
            return false;
        }

        if (!m_duplex && simplexSameFreq) {
            m_rxFrequency = m_txFrequency;
        }

        yaml::Node& voiceChList = rfssConfig["voiceChNo"];

        if (voiceChList.size() == 0U) {
            ::LogError(LOG_HOST, "No voice channel list defined!");
            return false;
        }

        for (size_t i = 0; i < voiceChList.size(); i++) {
            yaml::Node& channel = voiceChList[i];

            uint32_t chNo = (uint32_t)::strtoul(channel["channelNo"].as<std::string>("1").c_str(), NULL, 16);
            if (chNo == 0U) { // clamp to 1
                chNo = 1U;
            }
            if (chNo > 4095U) { // clamp to 4095
                chNo = 4095U;
            }

            std::string restApiAddress = channel["restAddress"].as<std::string>("127.0.0.1");
            uint16_t restApiPort = (uint16_t)channel["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
            std::string restApiPassword = channel["restPassword"].as<std::string>();

            ::LogInfoEx(LOG_HOST, "Voice Channel Id %u Channel No $%04X REST API Adddress %s:%u", m_channelId, chNo, restApiAddress.c_str(), restApiPort);

            VoiceChData data = VoiceChData(chNo, restApiAddress, restApiPort, restApiPassword);
            m_voiceChData[chNo] = data;
            m_voiceChNo.push_back(chNo);
        }

        std::string strVoiceChNo = "";
        for (auto it = m_voiceChNo.begin(); it != m_voiceChNo.end(); ++it) {
            int decVal = ::atoi(std::to_string(*it).c_str());
            char hexStr[23];

            ::sprintf(hexStr, "$%04X (%u)", decVal, decVal);

            strVoiceChNo.append(std::string(hexStr));
            strVoiceChNo.append(",");
        }
        strVoiceChNo.erase(strVoiceChNo.find_last_of(","));

        m_siteId = (uint8_t)::strtoul(rfssConfig["siteId"].as<std::string>("1").c_str(), NULL, 16);
        m_siteId = p25::P25Utils::siteId(m_siteId);

        m_dmrColorCode = rfssConfig["colorCode"].as<uint32_t>(2U);
        m_dmrColorCode = dmr::DMRUtils::colorCode(m_dmrColorCode);

        m_dmrNetId = (uint32_t)::strtoul(rfssConfig["dmrNetId"].as<std::string>("1").c_str(), NULL, 16);
        m_dmrNetId = dmr::DMRUtils::netId(m_dmrNetId, dmr::SITE_MODEL_TINY);

        m_p25NAC = (uint32_t)::strtoul(rfssConfig["nac"].as<std::string>("293").c_str(), NULL, 16);
        m_p25NAC = p25::P25Utils::nac(m_p25NAC);

        uint32_t p25TxNAC = (uint32_t)::strtoul(rfssConfig["txNAC"].as<std::string>("F7E").c_str(), NULL, 16);
        if (p25TxNAC == m_p25NAC) {
            LogWarning(LOG_HOST, "Only use txNAC when split NAC operations are needed. nac and txNAC should not be the same!");
        }

        m_p25PatchSuperGroup = (uint32_t)::strtoul(rfssConfig["pSuperGroup"].as<std::string>("FFFF").c_str(), NULL, 16);
        m_p25NetId = (uint32_t)::strtoul(rfssConfig["netId"].as<std::string>("BB800").c_str(), NULL, 16);
        m_p25NetId = p25::P25Utils::netId(m_p25NetId);

        m_sysId = (uint32_t)::strtoul(rfssConfig["sysId"].as<std::string>("001").c_str(), NULL, 16);
        m_sysId = p25::P25Utils::sysId(m_sysId);

        m_p25RfssId = (uint8_t)::strtoul(rfssConfig["rfssId"].as<std::string>("1").c_str(), NULL, 16);
        m_p25RfssId = p25::P25Utils::rfssId(m_p25RfssId);

        m_nxdnRAN = rfssConfig["ran"].as<uint32_t>(1U);

        m_authoritative = rfssConfig["authoritative"].as<bool>(true);

        LogInfo("System Config Parameters");
        LogInfo("    Authoritative: %s", m_authoritative ? "yes" : "no");
        if (m_authoritative) {
            m_supervisor = rfssConfig["supervisor"].as<bool>(false);
            LogInfo("    Supervisor: %s", m_supervisor ? "yes" : "no");
        }
        LogInfo("    RX Frequency: %uHz", m_rxFrequency);
        LogInfo("    TX Frequency: %uHz", m_txFrequency);
        LogInfo("    Base Frequency: %uHz", entry.baseFrequency());
        LogInfo("    TX Offset: %fMHz", entry.txOffsetMhz());
        LogInfo("    Bandwidth: %fKHz", entry.chBandwidthKhz());
        LogInfo("    Channel Spacing: %fKHz", entry.chSpaceKhz());
        LogInfo("    Channel Id: %u", m_channelId);
        LogInfo("    Channel No.: $%04X (%u)", m_channelNo, m_channelNo);
        LogInfo("    Voice Channel No(s).: %s", strVoiceChNo.c_str());
        LogInfo("    Site Id: $%02X", m_siteId);
        LogInfo("    System Id: $%03X", m_sysId);
        LogInfo("    DMR Color Code: %u", m_dmrColorCode);
        LogInfo("    DMR Network Id: $%05X", m_dmrNetId);
        LogInfo("    P25 NAC: $%03X", m_p25NAC);

        if (p25TxNAC != 0xF7EU && p25TxNAC != m_p25NAC) {
            LogInfo("    P25 Tx NAC: $%03X", p25TxNAC);
        }

        LogInfo("    P25 Patch Super Group: $%04X", m_p25PatchSuperGroup);
        LogInfo("    P25 Network Id: $%05X", m_p25NetId);
        LogInfo("    P25 RFSS Id: $%02X", m_p25RfssId);
        LogInfo("    NXDN RAN: %u", m_nxdnRAN);

        if (!m_authoritative) {
            m_supervisor = false;
            LogWarning(LOG_HOST, "Host is non-authoritative! This requires REST API to handle permit TG for VCs and grant TG for CCs!");
        }
    }
    else {
        LogInfo("    Modem Remote Control: yes");
    }

    return true;
}

/// <summary>
/// Initializes the modem DSP.
/// </summary>
bool Host::createModem()
{
    yaml::Node protocolConf = m_conf["protocols"];

    yaml::Node dmrProtocol = protocolConf["dmr"];
    uint32_t dmrQueueSize = dmrProtocol["queueSize"].as<uint32_t>(24U);

    // clamp queue size to no less then 24 and no greater the 100
    if (dmrQueueSize < 24U) {
        LogWarning(LOG_HOST, "DMR queue size must be greater then 24 frames, defaulting to 24 frames!");
        dmrQueueSize = 24U;
    }
    if (dmrQueueSize > 100U) {
        LogWarning(LOG_HOST, "DMR queue size must be less then 100 frames, defaulting to 100 frames!");
        dmrQueueSize = 100U;
    }
    if (dmrQueueSize > 60U) {
        LogWarning(LOG_HOST, "DMR queue size is excessive, >60 frames!");
    }

    m_dmrQueueSizeBytes = dmrQueueSize * (dmr::DMR_FRAME_LENGTH_BYTES * 5U);

    yaml::Node p25Protocol = protocolConf["p25"];
    uint32_t p25QueueSize = p25Protocol["queueSize"].as<uint16_t>(12U);

    // clamp queue size to no less then 12 and no greater the 100 frames
    if (p25QueueSize < 12U) {
        LogWarning(LOG_HOST, "P25 queue size must be greater then 12 frames, defaulting to 12 frames!");
        p25QueueSize = 12U;
    }
    if (p25QueueSize > 50U) {
        LogWarning(LOG_HOST, "P25 queue size must be less then 50 frames, defaulting to 50 frames!");
        p25QueueSize = 50U;
    }
    if (p25QueueSize > 30U) {
        LogWarning(LOG_HOST, "P25 queue size is excessive, >30 frames!");
    }

    m_p25QueueSizeBytes = p25QueueSize * p25::P25_LDU_FRAME_LENGTH_BYTES;

    yaml::Node nxdnProtocol = protocolConf["nxdn"];
    uint32_t nxdnQueueSize = nxdnProtocol["queueSize"].as<uint32_t>(31U);

    // clamp queue size to no less then 31 and no greater the 50 frames
    if (nxdnQueueSize < 31U) {
        LogWarning(LOG_HOST, "NXDN queue size must be greater then 31 frames, defaulting to 31 frames!");
        nxdnQueueSize = 31U;
    }
    if (nxdnQueueSize > 50U) {
        LogWarning(LOG_HOST, "NXDN queue size must be less then 50 frames, defaulting to 50 frames!");
        nxdnQueueSize = 50U;
    }

    m_nxdnQueueSizeBytes = nxdnQueueSize * nxdn::NXDN_FRAME_LENGTH_BYTES;

    yaml::Node modemConf = m_conf["system"]["modem"];

    yaml::Node modemProtocol = modemConf["protocol"];
    std::string portType = modemProtocol["type"].as<std::string>("null");
#if defined(ENABLE_P25) && defined(ENABLE_DFSI)
    m_useDFSI = modemProtocol["dfsi"].as<bool>(false);
#else
    m_useDFSI = false;
#endif // defined(ENABLE_P25) && defined(ENABLE_DFSI)

    yaml::Node uartProtocol = modemProtocol["uart"];
    std::string uartPort = uartProtocol["port"].as<std::string>();
    uint32_t uartSpeed = uartProtocol["speed"].as<uint32_t>(115200);

    bool rxInvert = modemConf["rxInvert"].as<bool>(false);
    bool txInvert = modemConf["txInvert"].as<bool>(false);
    bool pttInvert = modemConf["pttInvert"].as<bool>(false);
    bool dcBlocker = modemConf["dcBlocker"].as<bool>(true);
    bool cosLockout = modemConf["cosLockout"].as<bool>(false);
    uint8_t fdmaPreamble = (uint8_t)modemConf["fdmaPreamble"].as<uint32_t>(80U);
    uint8_t dmrRxDelay = (uint8_t)modemConf["dmrRxDelay"].as<uint32_t>(7U);
    uint8_t p25CorrCount = (uint8_t)modemConf["p25CorrCount"].as<uint32_t>(4U);
    int rxDCOffset = modemConf["rxDCOffset"].as<int>(0);
    int txDCOffset = modemConf["txDCOffset"].as<int>(0);

    yaml::Node hotspotParams = modemConf["hotspot"];

    int dmrDiscBWAdj = hotspotParams["dmrDiscBWAdj"].as<int>(0);
    int p25DiscBWAdj = hotspotParams["p25DiscBWAdj"].as<int>(0);
    int nxdnDiscBWAdj = hotspotParams["nxdnDiscBWAdj"].as<int>(0);
    int dmrPostBWAdj = hotspotParams["dmrPostBWAdj"].as<int>(0);
    int p25PostBWAdj = hotspotParams["p25PostBWAdj"].as<int>(0);
    int nxdnPostBWAdj = hotspotParams["nxdnPostBWAdj"].as<int>(0);
    ADF_GAIN_MODE adfGainMode = (ADF_GAIN_MODE)hotspotParams["adfGainMode"].as<uint32_t>(0U);
    bool afcEnable = hotspotParams["afcEnable"].as<bool>(false);
    uint8_t afcKI = (uint8_t)hotspotParams["afcKI"].as<uint32_t>(11U);
    uint8_t afcKP = (uint8_t)hotspotParams["afcKP"].as<uint32_t>(4U);
    uint8_t afcRange = (uint8_t)hotspotParams["afcRange"].as<uint32_t>(1U);
    int rxTuning = hotspotParams["rxTuning"].as<int>(0);
    int txTuning = hotspotParams["txTuning"].as<int>(0);
    uint8_t rfPower = (uint8_t)hotspotParams["rfPower"].as<uint32_t>(100U);

    yaml::Node repeaterParams = modemConf["repeater"];

    int dmrSymLevel3Adj = repeaterParams["dmrSymLvl3Adj"].as<int>(0);
    int dmrSymLevel1Adj = repeaterParams["dmrSymLvl1Adj"].as<int>(0);
    int p25SymLevel3Adj = repeaterParams["p25SymLvl3Adj"].as<int>(0);
    int p25SymLevel1Adj = repeaterParams["p25SymLvl1Adj"].as<int>(0);
    int nxdnSymLevel3Adj = repeaterParams["nxdnSymLvl3Adj"].as<int>(0);
    int nxdnSymLevel1Adj = repeaterParams["nxdnSymLvl1Adj"].as<int>(0);

    yaml::Node softpotParams = modemConf["softpot"];

    uint8_t rxCoarse = (uint8_t)softpotParams["rxCoarse"].as<uint32_t>(127U);
    uint8_t rxFine = (uint8_t)softpotParams["rxFine"].as<uint32_t>(127U);
    uint8_t txCoarse = (uint8_t)softpotParams["txCoarse"].as<uint32_t>(127U);
    uint8_t txFine = (uint8_t)softpotParams["txFine"].as<uint32_t>(127U);
    uint8_t rssiCoarse = (uint8_t)softpotParams["rssiCoarse"].as<uint32_t>(127U);
    uint8_t rssiFine = (uint8_t)softpotParams["rssiFine"].as<uint32_t>(127U);

    float rxLevel = modemConf["rxLevel"].as<float>(50.0F);
    float cwIdTXLevel = modemConf["cwIdTxLevel"].as<float>(50.0F);
    float dmrTXLevel = modemConf["dmrTxLevel"].as<float>(50.0F);
    float p25TXLevel = modemConf["p25TxLevel"].as<float>(50.0F);
    float nxdnTXLevel = modemConf["nxdnTxLevel"].as<float>(50.0F);
    if (!modemConf["txLevel"].isNone()) {
        cwIdTXLevel = dmrTXLevel = p25TXLevel = nxdnTXLevel = modemConf["txLevel"].as<float>(50.0F);
    }
    bool disableOFlowReset = modemConf["disableOFlowReset"].as<bool>(false);
    bool ignoreModemConfigArea = modemConf["ignoreModemConfigArea"].as<bool>(false);
    bool dumpModemStatus = modemConf["dumpModemStatus"].as<bool>(false);
    bool trace = modemConf["trace"].as<bool>(false);
    bool debug = modemConf["debug"].as<bool>(false);

    if (rfPower == 0U) { // clamp to 1
        rfPower = 1U;
    }
    if (rfPower > 100U) { // clamp to 100
        rfPower = 100U;
    }

    LogInfo("Modem Parameters");
    LogInfo("    Port Type: %s", portType.c_str());

    port::IModemPort* modemPort = nullptr;
    std::transform(portType.begin(), portType.end(), portType.begin(), ::tolower);
    if (portType == NULL_PORT) {
        modemPort = new port::ModemNullPort();
    }
    else if (portType == UART_PORT || portType == PTY_PORT) {
        port::SERIAL_SPEED serialSpeed = port::SERIAL_115200;
        switch (uartSpeed) {
        case 1200:
            serialSpeed = port::SERIAL_1200;
            break;
        case 2400:
            serialSpeed = port::SERIAL_2400;
            break;
        case 4800:
            serialSpeed = port::SERIAL_4800;
            break;
        case 9600:
            serialSpeed = port::SERIAL_9600;
            break;
        case 19200:
            serialSpeed = port::SERIAL_19200;
            break;
        case 38400:
            serialSpeed = port::SERIAL_38400;
            break;
        case 76800:
            serialSpeed = port::SERIAL_76800;
            break;
        case 230400:
            serialSpeed = port::SERIAL_230400;
            break;
        case 460800:
            serialSpeed = port::SERIAL_460800;
            break;
        default:
            LogWarning(LOG_HOST, "Unsupported serial speed %u, defaulting to %u", uartSpeed, port::SERIAL_115200);
            uartSpeed = 115200;
        case 115200:
            break;
        }

        if (portType == PTY_PORT) {
#if !defined(_WIN32) && !defined(_WIN64)
            modemPort = new port::UARTPort(uartPort, serialSpeed, false);
            LogInfo("    PTY Port: %s", uartPort.c_str());
            LogInfo("    PTY Speed: %u", uartSpeed);
#else
            LogError(LOG_HOST, "Pseudo PTY is not supported on Windows!");
            return false;
#endif // !defined(_WIN32) && !defined(_WIN64)
        }
        else {
            modemPort = new port::UARTPort(uartPort, serialSpeed, true);
            LogInfo("    UART Port: %s", uartPort.c_str());
            LogInfo("    UART Speed: %u", uartSpeed);
        }
    }
    else {
        LogError(LOG_HOST, "Invalid protocol port type, %s!", portType.c_str());
        return false;
    }

    if (g_remoteModemMode) {
        if (portType == UART_PORT || portType == PTY_PORT) {
            m_modemRemotePort = new port::UDPPort(g_remoteAddress, g_remotePort);
            m_modemRemote = true;
            ignoreModemConfigArea = true;

        }
        else {
            delete modemPort;
            modemPort = new port::UDPPort(g_remoteAddress, g_remotePort);
            m_modemRemote = false;
        }

        LogInfo("    UDP Mode: %s", m_modemRemote ? "master" : "peer");
        LogInfo("    UDP Address: %s", g_remoteAddress.c_str());
        LogInfo("    UDP Port: %u", g_remotePort);
    }

    if (!m_modemRemote) {
        LogInfo("    RX Invert: %s", rxInvert ? "yes" : "no");
        LogInfo("    TX Invert: %s", txInvert ? "yes" : "no");
        LogInfo("    PTT Invert: %s", pttInvert ? "yes" : "no");
        LogInfo("    DC Blocker: %s", dcBlocker ? "yes" : "no");
        LogInfo("    COS Lockout: %s", cosLockout ? "yes" : "no");
        LogInfo("    FDMA Preambles: %u (%.1fms)", fdmaPreamble, float(fdmaPreamble) * 0.2222F);
        LogInfo("    DMR RX Delay: %u (%.1fms)", dmrRxDelay, float(dmrRxDelay) * 0.0416666F);
        LogInfo("    P25 Corr. Count: %u (%.1fms)", p25CorrCount, float(p25CorrCount) * 0.667F);
        LogInfo("    RX DC Offset: %d", rxDCOffset);
        LogInfo("    TX DC Offset: %d", txDCOffset);
        LogInfo("    RX Tuning Offset: %dhz", rxTuning);
        LogInfo("    TX Tuning Offset: %dhz", txTuning);
        LogInfo("    RX Effective Frequency: %uhz", m_rxFrequency + rxTuning);
        LogInfo("    TX Effective Frequency: %uhz", m_txFrequency + txTuning);
        LogInfo("    RX Coarse: %u, Fine: %u", rxCoarse, rxFine);
        LogInfo("    TX Coarse: %u, Fine: %u", txCoarse, txFine);
        LogInfo("    RSSI Coarse: %u, Fine: %u", rssiCoarse, rssiFine);
        LogInfo("    RF Power Level: %u", rfPower);
        LogInfo("    RX Level: %.1f%%", rxLevel);
        LogInfo("    CW Id TX Level: %.1f%%", cwIdTXLevel);
        LogInfo("    DMR TX Level: %.1f%%", dmrTXLevel);
        LogInfo("    P25 TX Level: %.1f%%", p25TXLevel);
        LogInfo("    NXDN TX Level: %.1f%%", nxdnTXLevel);
        LogInfo("    Disable Overflow Reset: %s", disableOFlowReset ? "yes" : "no");
        LogInfo("    DMR Queue Size: %u (%u bytes)", dmrQueueSize, m_dmrQueueSizeBytes);
        LogInfo("    P25 Queue Size: %u (%u bytes)", p25QueueSize, m_p25QueueSizeBytes);
        LogInfo("    NXDN Queue Size: %u (%u bytes)", nxdnQueueSize, m_nxdnQueueSizeBytes);

        if (m_useDFSI) {
            LogInfo("    Digital Fixed Station Interface: yes");
        }

        if (ignoreModemConfigArea) {
            LogInfo("    Ignore Modem Configuration Area: yes");
        }

        if (dumpModemStatus) {
            LogInfo("    Dump Modem Status: yes");
        }
    }

    if (debug) {
        LogInfo("    Debug: yes");
    }

    m_modem = new Modem(modemPort, m_duplex, rxInvert, txInvert, pttInvert, dcBlocker, cosLockout, fdmaPreamble, dmrRxDelay, p25CorrCount,
        m_dmrQueueSizeBytes, m_p25QueueSizeBytes, m_nxdnQueueSizeBytes, disableOFlowReset, ignoreModemConfigArea, dumpModemStatus, trace, debug);
    if (!m_modemRemote) {
        m_modem->setModeParams(m_dmrEnabled, m_p25Enabled, m_nxdnEnabled);
        m_modem->setLevels(rxLevel, cwIdTXLevel, dmrTXLevel, p25TXLevel, nxdnTXLevel);
        m_modem->setSymbolAdjust(dmrSymLevel3Adj, dmrSymLevel1Adj, p25SymLevel3Adj, p25SymLevel1Adj, nxdnSymLevel3Adj, nxdnSymLevel1Adj);
        m_modem->setDCOffsetParams(txDCOffset, rxDCOffset);
        m_modem->setRFParams(m_rxFrequency, m_txFrequency, rxTuning, txTuning, rfPower, dmrDiscBWAdj, p25DiscBWAdj, nxdnDiscBWAdj, dmrPostBWAdj,
            p25PostBWAdj, nxdnPostBWAdj, adfGainMode, afcEnable, afcKI, afcKP, afcRange);
        m_modem->setSoftPot(rxCoarse, rxFine, txCoarse, txFine, rssiCoarse, rssiFine);
        m_modem->setDMRColorCode(m_dmrColorCode);
        m_modem->setP25NAC(m_p25NAC);
#if defined(ENABLE_P25) && defined(ENABLE_DFSI)
        m_modem->setP25DFSI(m_useDFSI);
#endif // defined(ENABLE_P25) && defined(ENABLE_DFSI)
    }

    if (m_modemRemote) {
        m_modem->setOpenHandler(MODEM_OC_PORT_HANDLER_BIND(Host::rmtPortModemOpen, this));
        m_modem->setCloseHandler(MODEM_OC_PORT_HANDLER_BIND(Host::rmtPortModemClose, this));
        m_modem->setResponseHandler(MODEM_RESP_HANDLER_BIND(Host::rmtPortModemHandler, this));
    }

    bool ret = m_modem->open();
    if (!ret) {
        delete m_modem;
        m_modem = nullptr;
        return false;
    }

    // are we on a protocol version older then 3?
    if (m_modem->getVersion() < 3U) {
        if (m_nxdnEnabled) {
            ::LogError(LOG_HOST, "NXDN is not supported on legacy firmware.");
            return false;
        }
    }

    return true;
}

/// <summary>
/// Initializes network connectivity.
/// </summary>
bool Host::createNetwork()
{
    yaml::Node networkConf = m_conf["network"];
    bool netEnable = networkConf["enable"].as<bool>(false);
    bool restApiEnable = networkConf["restEnable"].as<bool>(false);

    // dump out if both networking and REST API are disabled
    if (!netEnable && !restApiEnable) {
        return true;
    }

    std::string address = networkConf["address"].as<std::string>();
    uint16_t port = (uint16_t)networkConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
    uint16_t local = (uint16_t)networkConf["local"].as<uint32_t>(0U);
    std::string restApiAddress = networkConf["restAddress"].as<std::string>("127.0.0.1");
    uint16_t restApiPort = (uint16_t)networkConf["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
    std::string restApiPassword = networkConf["restPassword"].as<std::string>();
    bool restApiDebug = networkConf["restDebug"].as<bool>(false);
    uint32_t id = networkConf["id"].as<uint32_t>(0U);
    uint32_t jitter = networkConf["talkgroupHang"].as<uint32_t>(360U);
    std::string password = networkConf["password"].as<std::string>();
    bool slot1 = networkConf["slot1"].as<bool>(true);
    bool slot2 = networkConf["slot2"].as<bool>(true);
    bool allowActivityTransfer = networkConf["allowActivityTransfer"].as<bool>(false);
    bool allowDiagnosticTransfer = networkConf["allowDiagnosticTransfer"].as<bool>(false);
    bool updateLookup = networkConf["updateLookups"].as<bool>(false);
    bool debug = networkConf["debug"].as<bool>(false);

    if (restApiPassword.length() > 64) {
        std::string password = restApiPassword;
        restApiPassword = password.substr(0, 64);

        ::LogWarning(LOG_HOST, "REST API password is too long; truncating to the first 64 characters.");
    }

    if (restApiPassword.empty() && restApiEnable) {
        ::LogWarning(LOG_HOST, "REST API password not provided; REST API disabled.");
        restApiEnable = false;
    }

    IdenTable entry = m_idenTable->find(m_channelId);

    LogInfo("Network Parameters");
    LogInfo("    Enabled: %s", netEnable ? "yes" : "no");
    if (netEnable) {
        LogInfo("    Peer Id: %u", id);
        LogInfo("    Address: %s", address.c_str());
        LogInfo("    Port: %u", port);
        if (local > 0U)
            LogInfo("    Local: %u", local);
        else
            LogInfo("    Local: random");
        LogInfo("    DMR Jitter: %ums", jitter);
        LogInfo("    Slot 1: %s", slot1 ? "enabled" : "disabled");
        LogInfo("    Slot 2: %s", slot2 ? "enabled" : "disabled");
        LogInfo("    Allow Activity Log Transfer: %s", allowActivityTransfer ? "yes" : "no");
        LogInfo("    Allow Diagnostic Log Transfer: %s", allowDiagnosticTransfer ? "yes" : "no");
        LogInfo("    Update Lookups: %s", updateLookup ? "yes" : "no");

        if (debug) {
            LogInfo("    Debug: yes");
        }
    }
    LogInfo("    REST API Enabled: %s", restApiEnable ? "yes" : "no");
    if (restApiEnable) {
        LogInfo("    REST API Address: %s", restApiAddress.c_str());
        LogInfo("    REST API Port: %u", restApiPort);

        if (restApiDebug) {
            LogInfo("    REST API Debug: yes");
        }
    }

    // initialize networking
    if (netEnable) {
        m_network = new Network(address, port, local, id, password, m_duplex, debug, m_dmrEnabled, m_p25Enabled, m_nxdnEnabled, slot1, slot2, allowActivityTransfer, allowDiagnosticTransfer, updateLookup);

        m_network->setLookups(m_ridLookup, m_tidLookup);
        m_network->setMetadata(m_identity, m_rxFrequency, m_txFrequency, entry.txOffsetMhz(), entry.chBandwidthKhz(), m_channelId, m_channelNo,
            m_power, m_latitude, m_longitude, m_height, m_location);
        if (restApiEnable) {
            m_network->setRESTAPIData(restApiPassword, restApiPort);
        }

        bool ret = m_network->open();
        if (!ret) {
            delete m_network;
            m_network = nullptr;
            LogError(LOG_HOST, "failed to initialize traffic networking!");
            return false;
        }

        m_network->enable(true);
        ::LogSetNetwork(m_network);
    }

    // initialize network remote command
    if (restApiEnable) {
        m_RESTAPI = new RESTAPI(restApiAddress, restApiPort, restApiPassword, this, restApiDebug);
        m_RESTAPI->setLookups(m_ridLookup, m_tidLookup);
        bool ret = m_RESTAPI->open();
        if (!ret) {
            delete m_RESTAPI;
            m_RESTAPI = nullptr;
            LogError(LOG_HOST, "failed to initialize REST API networking! REST API will be unavailable!");
            // REST API failing isn't fatal -- we'll allow this to return normally
        }
    }
    else {
        m_RESTAPI = nullptr;
    }

    return true;
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

            m_modem->clearDMRData1();
            m_modem->clearDMRData2();
            m_modem->clearP25Data();

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

                if (m_tidLookup != nullptr) {
                    m_tidLookup->stop();
                    delete m_tidLookup;
                }
                if (m_ridLookup != nullptr) {
                    m_ridLookup->stop();
                    delete m_ridLookup;
                }

                if (m_network != nullptr) {
                    m_network->close();
                    delete m_network;
                }

                if (m_RESTAPI != nullptr) {
                    m_RESTAPI->close();
                    delete m_RESTAPI;
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
