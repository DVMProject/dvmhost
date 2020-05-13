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
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
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
#include "p25/Control.h"
#include "modem/SerialController.h"
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
#include <vector>

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

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
    m_modem(NULL),
    m_network(NULL),
    m_mode(STATE_IDLE),
    m_modeTimer(1000U),
    m_dmrTXTimer(1000U),
    m_cwIdTimer(1000U),
    m_dmrEnabled(false),
    m_p25Enabled(false),
    m_p25CtrlBcstContinuous(false),
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
    m_idenTable(NULL),
    m_ridLookup(NULL),
    m_tidLookup(NULL),
    m_remoteControl(NULL)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Host class.
/// </summary>
Host::~Host()
{
    /* stub */
}

/// <summary>
/// Executes the main modem host processing loop.
/// </summary>
/// <returns>Zero if successful, otherwise error occurred.</returns>
int Host::run()
{
    bool ret = yaml::Parse(m_conf, m_confFile.c_str());
    if (!ret) {
        ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
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
#endif

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
    if (m_conf["network"]["enable"].as<bool>(false)) {
        ret = createNetwork();
        if (!ret)
            return EXIT_FAILURE;
    }

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

    dmr::Control* dmr = NULL;
    LogInfo("DMR Parameters");
    LogInfo("    Enabled: %s", m_dmrEnabled ? "yes" : "no");
    if (m_dmrEnabled) {
        yaml::Node dmrProtocol = protocolConf["dmr"];
        m_dmrBeacons = dmrProtocol["beacons"]["enable"].as<bool>(false);
        bool embeddedLCOnly = dmrProtocol["embeddedLCOnly"].as<bool>(false);
        bool dmrDumpDataPacket = dmrProtocol["dumpDataPacket"].as<bool>(false);
        bool dmrRepeatDataPacket = dmrProtocol["repeatDataPacket"].as<bool>(true);
        bool dumpTAData = dmrProtocol["dumpTAData"].as<bool>(true);
        uint32_t callHang = dmrProtocol["callHang"].as<uint32_t>(3U);
        uint32_t txHang = dmrProtocol["txHang"].as<uint32_t>(4U);
        uint32_t dmrQueueSize = dmrProtocol["queueSize"].as<uint32_t>(5120U);
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
        LogInfo("    Call Hang: %us", callHang);
        LogInfo("    TX Hang: %us", txHang);
        LogInfo("    Queue Size: %u", dmrQueueSize);

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

        dmr = new dmr::Control(m_dmrColorCode, callHang, dmrQueueSize, embeddedLCOnly, dumpTAData, m_timeout, m_rfTalkgroupHang,
            m_modem, m_network, m_duplex, m_ridLookup, m_tidLookup, rssi, jitter, dmrDumpDataPacket, dmrRepeatDataPacket, dmrDebug, dmrVerbose);
        m_dmrTXTimer.setTimeout(txHang);

        if (dmrVerbose) {
            LogInfo("    Verbose: yes");
        }
        if (dmrDebug) {
            LogInfo("    Debug: yes");
        }
    }

    // initialize P25
    Timer p25CCIntervalTimer(1000U);
    Timer p25CCDurationTimer(1000U);

    p25::Control* p25 = NULL;
    LogInfo("P25 Parameters");
    LogInfo("    Enabled: %s", m_p25Enabled ? "yes" : "no");
    if (m_p25Enabled) {
        yaml::Node p25Protocol = protocolConf["p25"];
        uint32_t tduPreambleCount = p25Protocol["tduPreambleCount"].as<uint32_t>(8U);
        m_controlData = p25Protocol["control"]["enable"].as<bool>(false);
        bool controlBcstContinuous = p25Protocol["control"]["continuous"].as<bool>(false);
        bool p25DumpDataPacket = p25Protocol["dumpDataPacket"].as<bool>(false);
        bool p25RepeatDataPacket = p25Protocol["repeatDataPacket"].as<bool>(true);
        uint32_t callHang = p25Protocol["callHang"].as<uint32_t>(3U);
        uint32_t p25QueueSize = p25Protocol["queueSize"].as<uint32_t>(8192U);
        bool p25Verbose = p25Protocol["verbose"].as<bool>(true);
        bool p25Debug = p25Protocol["debug"].as<bool>(false);

        LogInfo("    TDU Preamble before Voice: %u", tduPreambleCount);
        LogInfo("    Dump Packet Data: %s", p25DumpDataPacket ? "yes" : "no");
        LogInfo("    Repeat Packet Data: %s", p25RepeatDataPacket ? "yes" : "no");
        LogInfo("    Call Hang: %us", callHang);
        LogInfo("    Queue Size: %u", p25QueueSize);

        LogInfo("    Control: %s", m_controlData ? "yes" : "no");

        uint32_t p25ControlBcstInterval = p25Protocol["control"]["interval"].as<uint32_t>(60U);
        uint32_t p25ControlBcstDuration = p25Protocol["control"]["duration"].as<uint32_t>(1U);
        if (m_controlData) {
            LogInfo("    Control Broadcast Continuous: %s", controlBcstContinuous ? "yes" : "no");
            if (controlBcstContinuous) {
                p25ControlBcstInterval = 30U;
                p25ControlBcstDuration = 120U;
                m_p25CtrlBcstContinuous = controlBcstContinuous;
            }
            else {
                LogInfo("    Control Broadcast Interval: %us", p25ControlBcstInterval);
                LogInfo("    Control Broadcast Duration: %us", p25ControlBcstDuration);
            }

            p25CCIntervalTimer.setTimeout(p25ControlBcstInterval);
            p25CCIntervalTimer.start();

            p25CCDurationTimer.setTimeout(p25ControlBcstDuration);

            g_fireP25Control = true;
            g_interruptP25Control = false;
        }

        p25 = new p25::Control(m_p25NAC, callHang, p25QueueSize, m_modem, m_network, m_timeout, m_rfTalkgroupHang,
            p25ControlBcstInterval, m_duplex, m_ridLookup, m_tidLookup, m_idenTable, rssi, p25DumpDataPacket, p25RepeatDataPacket, p25Debug, p25Verbose);
        p25->setOptions(m_conf, m_cwCallsign, m_voiceChNo, m_p25PatchSuperGroup, m_p25NetId, m_p25SysId, m_p25RfssId,
            m_p25SiteId, m_channelId, m_channelNo, true);

        if (p25Verbose) {
            LogInfo("    Verbose: yes");
        }
        if (p25Debug) {
            LogInfo("    Debug: yes");
        }
    }

    if (!m_dmrEnabled && !m_p25Enabled) {
        ::LogError(LOG_HOST, "No modes enabled? DMR and/or P25 must be enabled!");
        g_killed = true;
    }

    if (m_dmrEnabled && m_p25CtrlBcstContinuous) {
        ::LogError(LOG_HOST, "Cannot have DMR enabled when using dedicated P25 control!");
        g_killed = true;
    }

    if (m_fixedMode && m_dmrEnabled && m_p25Enabled) {
        ::LogError(LOG_HOST, "Cannot have DMR enabled and P25 enabled when using fixed mode! Choose one protocol for fixed mode operation.");
        g_killed = true;
    }

    if (m_dmrBeacons && m_controlData) {
        ::LogError(LOG_HOST, "Cannot have DMR roaming becaons and P25 control at the same time.");
        g_killed = true;
    }

    if (!m_duplex && m_controlData) {
        ::LogError(LOG_HOST, "Cannot have P25 control and simplex mode at the same time.");
        g_killed = true;
    }

    if (!g_killed) {
        setMode(STATE_IDLE);
        ::LogInfoEx(LOG_HOST, "Host is performing late initialization and warmup");

        // perform early pumping of the modem clock (this is so the DSP has time to setup its buffers),
        // and clock the network (so it may perform early connect)
        uint32_t elapsedMs = 0U;
        while (!g_killed) {
            uint32_t ms = stopWatch.elapsed();
            stopWatch.start();

            elapsedMs += ms;
            m_modem->clock(ms);

            if (m_network != NULL)
                m_network->clock(ms);

            if (ms < 2U)
                Thread::sleep(1U);

            if (elapsedMs > 15000U)
                break;
        }

        ::LogInfoEx(LOG_HOST, "Host is up and running");
        stopWatch.start();
    }

    bool killed = false;
    bool hasTxShutdown = false;

    #define INTERRUPT_P25_CONTROL                                                                                   \
        if (g_interruptP25Control) {                                                                                \
            p25CCDurationTimer.stop();                                                                              \
            if (p25CCDurationTimer.isRunning() && !p25CCDurationTimer.hasExpired()) {                               \
                LogDebug(LOG_HOST, "traffic interrupts P25 CC, g_interruptP25Control = %u", g_interruptP25Control); \
                m_modem->clearP25Data();                                                                            \
                p25->reset();                                                                                       \
            }                                                                                                       \
        }

    // main execution loop
    while (!killed) {
        if (m_modem->hasLockout() && m_mode != HOST_STATE_LOCKOUT)
            setMode(HOST_STATE_LOCKOUT);
        else if (!m_modem->hasLockout() && m_mode == HOST_STATE_LOCKOUT)
            setMode(STATE_IDLE);

        if (m_modem->hasError() && m_mode != HOST_STATE_ERROR)
            setMode(HOST_STATE_ERROR);
        else if (!m_modem->hasError() && m_mode == HOST_STATE_ERROR)
            setMode(STATE_IDLE);

        uint32_t ms = stopWatch.elapsed();
        if (ms > 1U)
            m_modem->clock(ms);

        uint8_t data[220U];
        uint32_t len;
        bool ret;
        bool hasCw = false;

        // ------------------------------------------------------
        //  -- Read from Modem Processing                     --
        // ------------------------------------------------------

        /** DMR */
        if (dmr != NULL) {
            // read DMR slot 1 frames from the modem, and if there is any
            // write those frames to the DMR controller
            len = m_modem->readDMRData1(data);
            if (len > 0U) {
                if (m_mode == STATE_IDLE) {
                    // if the modem is in duplex -- process wakeup CSBKs
                    if (m_duplex) {
                        bool ret = dmr->processWakeup(data);
                        if (ret) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            setMode(STATE_DMR);

                            dmrBeaconDurationTimer.stop();
                            INTERRUPT_P25_CONTROL;
                        }
                    }
                    else {
                        // in simplex directly process slot 1 frames
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setMode(STATE_DMR);
                        dmr->processFrame1(data, len);

                        dmrBeaconDurationTimer.stop();
                        p25CCDurationTimer.stop();
                    }
                }
                else if (m_mode == STATE_DMR) {
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
                        bool ret = dmr->processFrame1(data, len);
                        if (ret) {
                            dmrBeaconDurationTimer.stop();
                            INTERRUPT_P25_CONTROL;

                            m_modeTimer.start();
                            if (m_duplex)
                                m_dmrTXTimer.start();
                        }
                    }
                }
                else if (m_mode != HOST_STATE_LOCKOUT) {
                    LogWarning(LOG_HOST, "DMR modem data received, mode = %u", m_mode);
                }
            }

            // read DMR slot 2 frames from the modem, and if there is any
            // write those frames to the DMR controller
            len = m_modem->readDMRData2(data);
            if (len > 0U) {
                if (m_mode == STATE_IDLE) {
                    // if the modem is in duplex -- process wakeup CSBKs
                    if (m_duplex) {
                        bool ret = dmr->processWakeup(data);
                        if (ret) {
                            m_modeTimer.setTimeout(m_rfModeHang);
                            setMode(STATE_DMR);

                            dmrBeaconDurationTimer.stop();
                            INTERRUPT_P25_CONTROL;
                        }
                    }
                    else {
                        // in simplex -- directly process slot 2 frames
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setMode(STATE_DMR);
                        dmr->processFrame2(data, len);

                        dmrBeaconDurationTimer.stop();
                        INTERRUPT_P25_CONTROL;
                    }
                }
                else if (m_mode == STATE_DMR) {
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
                        bool ret = dmr->processFrame2(data, len);
                        if (ret) {
                            dmrBeaconDurationTimer.stop();
                            INTERRUPT_P25_CONTROL;

                            m_modeTimer.start();
                            if (m_duplex)
                                m_dmrTXTimer.start();
                        }
                    }
                }
                else if (m_mode != HOST_STATE_LOCKOUT) {
                    LogWarning(LOG_HOST, "DMR modem data received, mode = %u", m_mode);
                }
            }
        }

        /** P25 */
        // read P25 frames from modem, and if there are frames
        // write those frames to the P25 controller
        if (p25 != NULL) {
            len = m_modem->readP25Data(data);
            if (len > 0U) {
                if (m_mode == STATE_IDLE) {
                    bool ret = p25->processFrame(data, len);
                    if (ret) {
                        m_modeTimer.setTimeout(m_rfModeHang);
                        setMode(STATE_P25);

                        dmrBeaconDurationTimer.stop();
                        INTERRUPT_P25_CONTROL;
                    }
                    else {
                        ret = p25->writeEndRF();
                        if (ret) {
                            dmrBeaconDurationTimer.stop();

                            if (m_mode == STATE_IDLE) {
                                m_modeTimer.setTimeout(m_rfModeHang);
                                setMode(STATE_P25);
                            }

                            if (m_mode == STATE_P25) {
                                m_modeTimer.start();
                            }

                            // if the modem is in duplex -- handle P25 CC burst control
                            if (m_duplex) {
                                if (p25CCDurationTimer.isPaused() && !g_interruptP25Control) {
                                    LogDebug(LOG_HOST, "traffic complete, resume P25 CC, g_interruptP25Control = %u", g_interruptP25Control);
                                    p25CCDurationTimer.resume();
                                }

                                if (g_interruptP25Control) {
                                    g_fireP25Control = true;
                                }

                                if (g_fireP25Control) {
                                    m_modeTimer.stop();
                                }
                            }
                            else {
                                p25CCDurationTimer.stop();
                                g_interruptP25Control = false;
                            }
                        }
                    }
                }
                else if (m_mode == STATE_P25) {
                    bool ret = p25->processFrame(data, len);
                    if (ret) {
                        m_modeTimer.start();
                        INTERRUPT_P25_CONTROL;
                    }
                    else {
                        ret = p25->writeEndRF();
                        if (ret) {
                            if (m_mode == STATE_IDLE) {
                                m_modeTimer.setTimeout(m_rfModeHang);
                                setMode(STATE_P25);
                            }

                            if (m_mode == STATE_P25) {
                                m_modeTimer.start();
                            }
                        }
                    }
                }
                else if (m_mode != HOST_STATE_LOCKOUT) {
                    LogWarning(LOG_HOST, "P25 modem data received, mode = %u", m_mode);
                }
            }
        }

        // ------------------------------------------------------
        //  -- Write to Modem Processing                      --
        // ------------------------------------------------------

        if (m_modeTimer.isRunning() && m_modeTimer.hasExpired()) {
            if (!m_fixedMode) {
                setMode(STATE_IDLE);
            } else {
                if (dmr != NULL)
                    setMode(STATE_DMR);
                if (p25 != NULL)
                    setMode(STATE_P25);
            }
        }

        /** DMR */
        if (dmr != NULL) {
            // check if there is space on the modem for DMR slot 1 frames,
            // if there is read frames from the DMR controller and write it
            // to the modem
            ret = m_modem->hasDMRSpace1();
            if (ret) {
                len = dmr->getFrame1(data);
                if (len > 0U) {
                    if (m_mode == STATE_IDLE) {
                        m_modeTimer.setTimeout(m_netModeHang);
                        setMode(STATE_DMR);
                    }
                    if (m_mode == STATE_DMR) {
                        // if the modem is in duplex -- write DMR sync start
                        if (m_duplex) {
                            m_modem->writeDMRStart(true);
                            m_dmrTXTimer.start();
                        }

                        m_modem->writeDMRData1(data, len);

                        dmrBeaconDurationTimer.stop();
                        if (g_interruptP25Control && p25CCDurationTimer.isRunning()) {
                            p25CCDurationTimer.pause();
                        }
                        m_modeTimer.start();
                    }
                    else if (m_mode != HOST_STATE_LOCKOUT) {
                        LogWarning(LOG_HOST, "DMR data received, mode = %u", m_mode);
                    }
                }
            }

            // check if there is space on the modem for DMR slot 2 frames,
            // if there is read frames from the DMR controller and write it
            // to the modem
            ret = m_modem->hasDMRSpace2();
            if (ret) {
                len = dmr->getFrame2(data);
                if (len > 0U) {
                    if (m_mode == STATE_IDLE) {
                        m_modeTimer.setTimeout(m_netModeHang);
                        setMode(STATE_DMR);
                    }
                    if (m_mode == STATE_DMR) {
                        // if the modem is in duplex -- write DMR sync start
                        if (m_duplex) {
                            m_modem->writeDMRStart(true);
                            m_dmrTXTimer.start();
                        }

                        m_modem->writeDMRData2(data, len);

                        dmrBeaconDurationTimer.stop();
                        if (g_interruptP25Control && p25CCDurationTimer.isRunning()) {
                            p25CCDurationTimer.pause();
                        }
                        m_modeTimer.start();
                    }
                    else if (m_mode != HOST_STATE_LOCKOUT) {
                        LogWarning(LOG_HOST, "DMR data received, mode = %u", m_mode);
                    }
                }
            }
        }

        /** P25 */
        // check if there is space on the modem for P25 frames,
        // if there is read frames from the P25 controller and write it
        // to the modem
        if (p25 != NULL) {
            ret = m_modem->hasP25Space();
            if (ret) {
                len = p25->getFrame(data);
                if (len > 0U) {
                    if (m_mode == STATE_IDLE) {
                        m_modeTimer.setTimeout(m_netModeHang);
                        setMode(STATE_P25);
                    }
                    
                    if (m_mode == STATE_P25) {
                        m_modem->writeP25Data(data, len);

                        dmrBeaconDurationTimer.stop();
                        if (g_interruptP25Control && p25CCDurationTimer.isRunning()) {
                            p25CCDurationTimer.pause();
                        }

                        m_modeTimer.start();
                    }
                    else if (m_mode != HOST_STATE_LOCKOUT) {
                        LogWarning(LOG_HOST, "P25 data received, mode = %u", m_mode);
                    }
                }
                else {
                    if (m_mode == STATE_IDLE || m_mode == STATE_P25) {
                        // P25 control data, if control data is being transmitted
                        if (p25CCDurationTimer.isRunning() && !p25CCDurationTimer.hasExpired()) {
                            p25->setCCRunning(true);
                            p25->writeControlRF();
                        }

                        // P25 status data, tail on idle
                        ret = p25->writeEndRF();
                        if (ret) {
                            if (m_mode == STATE_IDLE) {
                                m_modeTimer.setTimeout(m_netModeHang);
                                setMode(STATE_P25);
                            }

                            if (m_mode == STATE_P25) {
                                m_modeTimer.start();
                            }
                        }
                    }
                }


                // if the modem is in duplex -- handle P25 CC burst control
                if (m_duplex) {
                    if (p25CCDurationTimer.isPaused() && !g_interruptP25Control) {
                        LogDebug(LOG_HOST, "traffic complete, resume P25 CC, g_interruptP25Control = %u", g_interruptP25Control);
                        p25CCDurationTimer.resume();
                    }

                    if (g_interruptP25Control) {
                        g_fireP25Control = true;
                    }

                    if (g_fireP25Control) {
                        m_modeTimer.stop();
                    }
                }
            }
        }

        // ------------------------------------------------------
        //  -- Remote Control Processing                      --
        // ------------------------------------------------------

        if (m_remoteControl != NULL) {
            m_remoteControl->process(this, dmr, p25);
        }

        // ------------------------------------------------------
        //  -- Modem, DMR, P25 and Network Clocking           --
        // ------------------------------------------------------

        ms = stopWatch.elapsed();
        stopWatch.start();

        m_modem->clock(ms);

        if (dmr != NULL)
            dmr->clock();
        if (p25 != NULL)
            p25->clock(ms);

        if (m_network != NULL)
            m_network->clock(ms);

        // ------------------------------------------------------
        //  -- Timer Clocking                                 --
        // ------------------------------------------------------

        // clock and check CW timer
        m_cwIdTimer.clock(ms);
        if (m_cwIdTimer.isRunning() && m_cwIdTimer.hasExpired()) {
            if (dmrBeaconDurationTimer.isRunning() || p25CCDurationTimer.isRunning()) {
                LogDebug(LOG_HOST, "CW, beacon or CC timer running, ceasing");

                setMode(STATE_IDLE);

                dmrBeaconDurationTimer.stop();
                p25CCDurationTimer.stop();
                //g_interruptP25Control = true;
            }

            if (m_mode == STATE_IDLE && !m_modem->hasTX()) {
                hasCw = true;
                m_modem->sendCWId(m_cwCallsign);

                m_cwIdTimer.setTimeout(m_cwIdTime);
                m_cwIdTimer.start();
            }
        }

        /** DMR */
        if (dmr != NULL) {
            // clock and check DMR roaming beacon interval timer
            dmrBeaconIntervalTimer.clock(ms);
            if ((dmrBeaconIntervalTimer.isRunning() && dmrBeaconIntervalTimer.hasExpired()) || g_fireDMRBeacon) {
                if (hasCw) {
                    g_fireDMRBeacon = false;
                    dmrBeaconIntervalTimer.start();
                }
                else {
                    if ((m_mode == STATE_IDLE || m_mode == STATE_DMR) && !m_modem->hasTX()) {
                        if (m_modeTimer.isRunning()) {
                            m_modeTimer.stop();
                        }

                        if (m_mode != STATE_DMR)
                            setMode(STATE_DMR);

                        g_fireDMRBeacon = false;
                        LogDebug(LOG_HOST, "DMR, roaming beacon burst");
                        dmrBeaconIntervalTimer.start();
                        dmrBeaconDurationTimer.start();
                    }
                }
            }

            // clock and check DMR roaming beacon duration timer
            dmrBeaconDurationTimer.clock(ms);
            if (dmrBeaconDurationTimer.isRunning() && dmrBeaconDurationTimer.hasExpired()) {
                dmrBeaconDurationTimer.stop();

                if (m_mode == STATE_DMR && !m_modeTimer.isRunning()) {
                    m_modeTimer.setTimeout(m_rfModeHang);
                    m_modeTimer.start();
                }
            }

            // clock and check DMR Tx timer
            m_dmrTXTimer.clock(ms);
            if (m_dmrTXTimer.isRunning() && m_dmrTXTimer.hasExpired()) {
                m_modem->writeDMRStart(false);
                m_dmrTXTimer.stop();
            }
        }

        /** P25 */
        if (p25 != NULL) {
            // clock and check P25 CC broadcast interval timer
            p25CCIntervalTimer.clock(ms);
            if ((p25CCIntervalTimer.isRunning() && p25CCIntervalTimer.hasExpired()) || g_fireP25Control) {
                if (hasCw) {
                    g_fireP25Control = false;
                    p25CCIntervalTimer.start();
                }
                else {
                    if ((m_mode == STATE_IDLE || m_mode == STATE_P25) && !m_modem->hasTX()) {
                        if (m_modeTimer.isRunning()) {
                            m_modeTimer.stop();
                        }

                        if (m_mode != STATE_P25)
                            setMode(STATE_P25);

                        if (g_interruptP25Control) {
                            g_interruptP25Control = false;
                            LogDebug(LOG_HOST, "traffic complete, restart P25 CC broadcast, g_interruptP25Control = %u", g_interruptP25Control);
                        }

                        p25->writeAdjSSNetwork();
                        p25->setCCRunning(true);

                        // hide this message for continuous CC -- otherwise display every time we process
                        if (!m_p25CtrlBcstContinuous) {
                            LogMessage(LOG_HOST, "P25, start CC broadcast");
                        }

                        g_fireP25Control = false;
                        p25CCIntervalTimer.start();
                        p25CCDurationTimer.start();

                        // if the CC is continuous -- clock one cycle into the duration timer
                        if (m_p25CtrlBcstContinuous) {
                            p25CCDurationTimer.clock(ms);
                        }
                    }
                }
            }

            // if the CC is continuous -- we don't clock the CC duration timer (which results in the CC
            // broadcast running infinitely until stopped)
            if (!m_p25CtrlBcstContinuous) {
                // clock and check P25 CC broadcast duration timer
                p25CCDurationTimer.clock(ms);
                if (p25CCDurationTimer.isRunning() && p25CCDurationTimer.hasExpired()) {
                    p25CCDurationTimer.stop();

                    p25->writeControlEndRF();
                    p25->setCCRunning(false);

                    if (m_mode == STATE_P25 && !m_modeTimer.isRunning()) {
                        m_modeTimer.setTimeout(m_rfModeHang);
                        m_modeTimer.start();
                    }
                }

                if (p25CCDurationTimer.isPaused()) {
                    p25CCDurationTimer.resume();
                }
            }
        }

        if (g_killed) {
            if (p25 != NULL) {
                if (m_p25CtrlBcstContinuous && !hasTxShutdown) {
                    m_modem->clearP25Data();
                    p25->reset();

                    p25->writeControlEndRF();
                    p25->setCCRunning(false);

                    p25CCDurationTimer.stop();
                    p25CCIntervalTimer.stop();
                }
            }

            hasTxShutdown = true;
            if (!m_modem->hasTX()) {
                killed = true;
            }
        }

        m_modeTimer.clock(ms);

        if (ms < 2U)
            Thread::sleep(1U);
    }

    ::ActivityLog("DVM", true, "Host is down and stopping");
    setMode(HOST_STATE_QUIT);

    delete dmr;
    delete p25;

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Reads basic configuration parameters from the INI.
/// </summary>
bool Host::readParams()
{
    yaml::Node protocolConf = m_conf["protocols"];
    m_dmrEnabled = protocolConf["dmr"]["enable"].as<bool>(false);
    m_p25Enabled = protocolConf["p25"]["enable"].as<bool>(false);

    yaml::Node systemConf = m_conf["system"];
    m_duplex = systemConf["duplex"].as<bool>(true);

    m_timeout = systemConf["timeout"].as<uint32_t>(120U);
    m_rfModeHang = systemConf["rfModeHang"].as<uint32_t>(10U);
    m_rfTalkgroupHang = systemConf["rfTalkgroupHang"].as<uint32_t>(10U);
    m_netModeHang = systemConf["netModeHang"].as<uint32_t>(3U);
    if (!systemConf["modeHang"].isNone()) {
        m_rfModeHang = m_netModeHang = systemConf["modeHang"].as<uint32_t>();
    }

    m_identity = systemConf["identity"].as<std::string>();
    m_fixedMode = systemConf["fixedMode"].as<bool>(false);

    removeLockFile();

    LogInfo("General Parameters");
    LogInfo("    DMR: %s", m_dmrEnabled ? "enabled" : "disabled");
    LogInfo("    P25: %s", m_p25Enabled ? "enabled" : "disabled");
    LogInfo("    Duplex: %s", m_duplex ? "yes" : "no");
    LogInfo("    Timeout: %us", m_timeout);
    LogInfo("    RF Mode Hang: %us", m_rfModeHang);
    LogInfo("    RF Talkgroup Hang: %us", m_rfTalkgroupHang);
    LogInfo("    Net Mode Hang: %us", m_netModeHang);
    LogInfo("    Identity: %s", m_identity.c_str());
    LogInfo("    Fixed Mode: %s", m_fixedMode ? "yes" : "no");
    LogInfo("    Lock Filename: %s", g_lockFile.c_str());

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

    if (entry.txOffsetMhz() == 0U) {
        ::LogError(LOG_HOST, "Channel Id %u has an invalid Tx offset.", m_channelId);
        return false;
    }

    uint32_t calcSpace = (uint32_t)(entry.chSpaceKhz() / 0.125);
    float calcTxOffset = entry.txOffsetMhz() * 1000000;

    m_channelNo = (uint32_t)::strtoul(rfssConfig["channelNo"].as<std::string>("1").c_str(), NULL, 16);
    if (m_channelNo == 0U) { // clamp to 1
        m_channelNo = 1U;
    }
    if (m_channelNo > 4095U) { // clamp to 4095
        m_channelNo = 4095U;
    }

    m_rxFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)) + calcTxOffset);
    m_txFrequency = (uint32_t)((entry.baseFrequency() + ((calcSpace * 125) * m_channelNo)));

    yaml::Node& voiceChList = rfssConfig["voiceChNo"];
    for (size_t i = 0; i < voiceChList.size(); i++) {
        uint32_t chNo = (uint32_t)::strtoul(voiceChList[i].as<std::string>("1").c_str(), NULL, 16);
        m_voiceChNo.push_back(chNo);
    }

    std::string strVoiceChNo = "";
    for (auto it = m_voiceChNo.begin(); it != m_voiceChNo.end(); ++it) {
        int decVal = ::atoi(std::to_string(*it).c_str());
        char hexStr[8];

        ::sprintf(hexStr, "$%04X", decVal);

        strVoiceChNo.append(std::string(hexStr));
        strVoiceChNo.append(",");
    }
    strVoiceChNo.erase(strVoiceChNo.find_last_of(","));

    m_dmrColorCode = rfssConfig["colorCode"].as<uint32_t>(2U);

    m_p25NAC = (uint32_t)::strtoul(rfssConfig["nac"].as<std::string>("293").c_str(), NULL, 16);
    m_p25PatchSuperGroup = (uint32_t)::strtoul(rfssConfig["pSuperGroup"].as<std::string>("FFFF").c_str(), NULL, 16);
    m_p25NetId = (uint32_t)::strtoul(rfssConfig["netId"].as<std::string>("BB800").c_str(), NULL, 16);
    if (m_p25NetId == 0U) { // clamp to 1
        m_p25NetId = 1U;
    }
    if (m_p25NetId > 0xFFFFEU) { // clamp to $FFFFE
        m_p25NetId = 0xFFFFEU;
    }
    m_p25SysId = (uint32_t)::strtoul(rfssConfig["sysId"].as<std::string>("001").c_str(), NULL, 16);
    if (m_p25SysId == 0U) { // clamp to 1
        m_p25SysId = 1U;
    }
    if (m_p25SysId > 0xFFEU) { // clamp to $FFE
        m_p25SysId = 0xFFEU;
    }
    m_p25RfssId = (uint8_t)::strtoul(rfssConfig["rfssId"].as<std::string>("1").c_str(), NULL, 16);
    if (m_p25RfssId == 0U) { // clamp to 1
        m_p25RfssId = 1U;
    }
    if (m_p25RfssId > 0xFEU) { // clamp to $FE
        m_p25RfssId = 0xFEU;
    }
    m_p25SiteId = (uint8_t)::strtoul(rfssConfig["siteId"].as<std::string>("1").c_str(), NULL, 16);
    if (m_p25SiteId == 0U) { // clamp to 1
        m_p25SiteId = 1U;
    }
    if (m_p25SiteId > 0xFEU) { // clamp to $FE
        m_p25SiteId = 0xFEU;
    }

    LogInfo("System Config Parameters");
    LogInfo("    RX Frequency: %uHz", m_rxFrequency);
    LogInfo("    TX Frequency: %uHz", m_txFrequency);
    LogInfo("    Base Frequency: %uHz", entry.baseFrequency());
    LogInfo("    TX Offset: %fMHz", entry.txOffsetMhz());
    LogInfo("    Bandwidth: %fKHz", entry.chBandwidthKhz());
    LogInfo("    Channel Spacing: %fKHz", entry.chSpaceKhz());
    LogInfo("    Channel Id: %u", m_channelId);
    LogInfo("    Channel No.: $%04X", m_channelNo);
    LogInfo("    Voice Channel No(s).: %s", strVoiceChNo.c_str());
    LogInfo("    DMR Color Code: %u", m_dmrColorCode);
    LogInfo("    P25 NAC: $%03X", m_p25NAC);
    LogInfo("    P25 Patch Super Group: $%04X", m_p25PatchSuperGroup);
    LogInfo("    P25 Network Id: $%05X", m_p25NetId);
    LogInfo("    P25 System Id: $%03X", m_p25SysId);
    LogInfo("    P25 RFSS Id: $%02X", m_p25RfssId);
    LogInfo("    P25 Site Id: $%02X", m_p25SiteId);

    return true;
}

/// <summary>
/// Initializes the modem DSP.
/// </summary>
bool Host::createModem()
{
    yaml::Node modemConf = m_conf["system"]["modem"];
    std::string port = modemConf["port"].as<std::string>();
    bool rxInvert = modemConf["rxInvert"].as<bool>(false);
    bool txInvert = modemConf["txInvert"].as<bool>(false);
    bool pttInvert = modemConf["pttInvert"].as<bool>(false);
    bool dcBlocker = modemConf["dcBlocker"].as<bool>(true);
    bool cosLockout = modemConf["cosLockout"].as<bool>(false);
    uint8_t fdmaPreamble = (uint8_t)modemConf["fdmaPreamble"].as<uint32_t>(80U);
    uint8_t dmrRxDelay = (uint8_t)modemConf["dmrRxDelay"].as<uint32_t>(7U);
    int rxDCOffset = modemConf["rxDCOffset"].as<int>(0);
    int txDCOffset = modemConf["txDCOffset"].as<int>(0);
    float rxLevel = modemConf["rxLevel"].as<float>(50.0F);
    float cwIdTXLevel = modemConf["cwIdTxLevel"].as<float>(50.0F);
    float dmrTXLevel = modemConf["dmrTxLevel"].as<float>(50.0F);
    float p25TXLevel = modemConf["p25TxLevel"].as<float>(50.0F);
    if (!modemConf["txLevel"].isNone()) {
        cwIdTXLevel = dmrTXLevel = p25TXLevel = modemConf["txLevel"].as<float>(50.0F);
    }
    bool disableOFlowReset = modemConf["disableOFlowReset"].as<bool>(false);
    bool trace = modemConf["trace"].as<bool>(false);
    bool debug = modemConf["debug"].as<bool>(false);

    LogInfo("Modem Parameters");
    LogInfo("    Port: %s", port.c_str());
    LogInfo("    RX Invert: %s", rxInvert ? "yes" : "no");
    LogInfo("    TX Invert: %s", txInvert ? "yes" : "no");
    LogInfo("    PTT Invert: %s", pttInvert ? "yes" : "no");
    LogInfo("    DC Blocker: %s", dcBlocker ? "yes" : "no");
    LogInfo("    COS Lockout: %s", cosLockout ? "yes" : "no");
    LogInfo("    FDMA Preambles: %u (%.1fms)", fdmaPreamble, float(fdmaPreamble) * 0.2083F);
    LogInfo("    DMR RX Delay: %u (%.1fms)", dmrRxDelay, float(dmrRxDelay) * 0.0416666F);
    LogInfo("    RX DC Offset: %d", rxDCOffset);
    LogInfo("    TX DC Offset: %d", txDCOffset);
    LogInfo("    RX Level: %.1f%%", rxLevel);
    LogInfo("    CW Id TX Level: %.1f%%", cwIdTXLevel);
    LogInfo("    DMR TX Level: %.1f%%", dmrTXLevel);
    LogInfo("    P25 TX Level: %.1f%%", p25TXLevel);
    LogInfo("    Disable Overflow Reset: %s", disableOFlowReset ? "yes" : "no");

    if (debug) {
        LogInfo("    Debug: yes");
    }

    m_modem = Modem::createModem(port, m_duplex, rxInvert, txInvert, pttInvert, dcBlocker, cosLockout, fdmaPreamble, dmrRxDelay, disableOFlowReset, trace, debug);
    m_modem->setModeParams(m_dmrEnabled, m_p25Enabled);
    m_modem->setLevels(rxLevel, cwIdTXLevel, dmrTXLevel, p25TXLevel);
    m_modem->setDCOffsetParams(txDCOffset, rxDCOffset);
    m_modem->setDMRParams(m_dmrColorCode);

    bool ret = m_modem->open();
    if (!ret) {
        delete m_modem;
        m_modem = NULL;
        return false;
    }

    return true;
}

/// <summary>
/// Initializes network connectivity.
/// </summary>
bool Host::createNetwork()
{
    yaml::Node networkConf = m_conf["network"];
    std::string address = networkConf["address"].as<std::string>();
    uint32_t port = networkConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
    uint32_t local = networkConf["local"].as<uint32_t>(0U);
    std::string rconAddress = networkConf["rconAddress"].as<std::string>("127.0.0.1");
    uint32_t rconPort = networkConf["rconPort"].as<uint32_t>(RCON_DEFAULT_PORT);
    uint32_t id = networkConf["id"].as<uint32_t>(0U);
    uint32_t jitter = networkConf["talkgroupHang"].as<uint32_t>(360U);
    std::string password = networkConf["password"].as<std::string>();
    bool slot1 = networkConf["slot1"].as<bool>(true);
    bool slot2 = networkConf["slot2"].as<bool>(true);
    bool transferActivityLog = networkConf["transferActivityLog"].as<bool>(false);
    bool updateLookup = networkConf["updateLookups"].as<bool>(false);
    bool debug = networkConf["debug"].as<bool>(false);

    IdenTable entry = m_idenTable->find(m_channelId);

    LogInfo("Network Parameters");
    LogInfo("    Peer Id: %u", id);
    LogInfo("    Address: %s", address.c_str());
    LogInfo("    Port: %u", port);
    if (local > 0U)
        LogInfo("    Local: %u", local);
    else
        LogInfo("    Local: random");
    LogInfo("    RCON Address: %s", rconAddress.c_str());
    LogInfo("    RCON Port: %u", rconPort);
    LogInfo("    DMR Jitter: %ums", jitter);
    LogInfo("    Slot 1: %s", slot1 ? "enabled" : "disabled");
    LogInfo("    Slot 2: %s", slot2 ? "enabled" : "disabled");
    LogInfo("    Transfer Activity Log: %s", transferActivityLog ? "enabled" : "disabled");
    LogInfo("    Update Lookups: %s", updateLookup ? "enabled" : "disabled");

    if (debug) {
        LogInfo("    Debug: yes");
    }

    m_network = new Network(address, port, local, id, password, m_duplex, debug, slot1, slot2, transferActivityLog, updateLookup);

    m_network->setLookups(m_ridLookup, m_tidLookup);
    m_network->setConfig(m_identity, m_rxFrequency, m_txFrequency, entry.txOffsetMhz(), entry.chBandwidthKhz(), m_power,
        m_latitude, m_longitude, m_height, m_location);

    bool ret = m_network->open();
    if (!ret) {
        delete m_network;
        m_network = NULL;
        LogError(LOG_HOST, "failed to initialize traffic networking!");
        return false;
    }

    m_network->enable(true);
    ::ActivityLogSetNetwork(m_network);

    // initialize network remote command
    m_remoteControl = new RemoteControl(rconAddress, rconPort);
    m_remoteControl->setLookups(m_ridLookup, m_tidLookup);
    ret = m_remoteControl->open();
    if (!ret) {
        delete m_remoteControl;
        m_remoteControl = NULL;
        LogError(LOG_HOST, "failed to initialize remote command networking! remote command control will be unavailable!");
        // remote command control failing isn't fatal -- we'll allow this to return normally
    }

    return true;
}

/// <summary>
/// Helper to set the host/modem running state.
/// </summary>
/// <param name="mode">Mode enumeration to switch the host/modem state to.</param>
void Host::setMode(uint8_t mode)
{
    assert(m_modem != NULL);

    if (m_mode != mode) {
        LogDebug(LOG_HOST, "setMode, m_mode = %u, mode = %u", m_mode, mode);
    }

    switch (mode) {
        case STATE_DMR:
            m_modem->setMode(STATE_DMR);

            // if the modem is in duplex -- write DMR start sync
            if (m_duplex) {
                m_modem->writeDMRStart(true);
                m_dmrTXTimer.start();
            }
            
            m_mode = STATE_DMR;
            m_modeTimer.start();
            //m_cwIdTimer.stop();
            createLockFile("DMR");
            break;

        case STATE_P25:
            m_modem->setMode(STATE_P25);
            m_mode = STATE_P25;
            m_modeTimer.start();
            //m_cwIdTimer.stop();
            createLockFile("P25");
            break;

        case HOST_STATE_LOCKOUT:
            LogWarning(LOG_HOST, "Mode change, MODE_LOCKOUT");
            if (m_network != NULL)
                m_network->enable(false);

            if (m_mode == STATE_DMR && m_duplex && m_modem->hasTX()) {
                m_modem->writeDMRStart(false);
                m_dmrTXTimer.stop();
            }

            m_modem->setMode(STATE_IDLE);
            m_mode = HOST_STATE_LOCKOUT;
            m_modeTimer.stop();
            //m_cwIdTimer.stop();
            removeLockFile();
            break;

        case HOST_STATE_ERROR:
            LogWarning(LOG_HOST, "Mode change, MODE_ERROR");
            if (m_network != NULL)
                m_network->enable(false);

            if (m_mode == STATE_DMR && m_duplex && m_modem->hasTX()) {
                m_modem->writeDMRStart(false);
                m_dmrTXTimer.stop();
            }
            
            m_mode = HOST_STATE_ERROR;
            m_modeTimer.stop();
            m_cwIdTimer.stop();
            removeLockFile();
            break;

        default:
            if (m_network != NULL)
                m_network->enable(true);

            if (m_mode == STATE_DMR && m_duplex && m_modem->hasTX()) {
                m_modem->writeDMRStart(false);
                m_dmrTXTimer.stop();
            }
            
            m_modem->setMode(STATE_IDLE);
            
            if (m_mode == HOST_STATE_ERROR) {
                m_modem->sendCWId(m_cwCallsign);

                m_cwIdTimer.setTimeout(m_cwIdTime);
                m_cwIdTimer.start();
            }

            removeLockFile();
            m_modeTimer.stop();

            if (m_mode == HOST_STATE_QUIT) {
                m_modem->close();
                delete m_modem;

                if (m_tidLookup != NULL) {
                    m_tidLookup->stop();
                    delete m_tidLookup;
                }
                if (m_ridLookup != NULL) {
                    m_ridLookup->stop();
                    delete m_ridLookup;
                }

                if (m_network != NULL) {
                    m_network->close();
                    delete m_network;
                }

                if (m_remoteControl != NULL) {
                    m_remoteControl->close();
                    delete m_remoteControl;
                }
            }
            else {
                m_mode = STATE_IDLE;
            }
            break;
    }
}

/// <summary>
///
/// </summary>
/// <param name="mode"></param>
void Host::createLockFile(const char* mode) const
{
    FILE* fp = ::fopen(g_lockFile.c_str(), "wt");
    if (fp != NULL) {
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
