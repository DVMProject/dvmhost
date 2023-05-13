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
#include "network/UDPSocket.h"
#include "host/fne/HostFNE.h"
#include "HostMain.h"
#include "Log.h"
#include "StopWatch.h"
#include "Thread.h"
#include "Utils.h"

using namespace network;
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

#define IDLE_WARMUP_MS 5U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the HostFNE class.
/// </summary>
/// <param name="confFile">Full-path to the configuration file.</param>
HostFNE::HostFNE(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_network(nullptr),
    m_dmrEnabled(false),
    m_p25Enabled(false),
    m_nxdnEnabled(false),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_peerNetworks(),
    m_pingTime(5U),
    m_maxMissedPings(5U),
    m_updateLookupTime(10U),
    m_allowActivityTransfer(false),
    m_allowDiagnosticTransfer(false)
{
    UDPSocket::startup();
}

/// <summary>
/// Finalizes a instance of the HostFNE class.
/// </summary>
HostFNE::~HostFNE()
{
    UDPSocket::shutdown();
}

/// <summary>
/// Executes the main FNE processing loop.
/// </summary>
/// <returns>Zero if successful, otherwise error occurred.</returns>
int HostFNE::run()
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
    ::LogInfo(">> Fixed Network Equipment");

    // read base parameters from configuration
    ret = readParams();
    if (!ret)
        return EXIT_FAILURE;

    yaml::Node systemConf = m_conf["system"];

    // try to load radio IDs table
    std::string ridLookupFile = systemConf["radio_id"]["file"].as<std::string>();
    uint32_t ridReloadTime = systemConf["radio_id"]["time"].as<uint32_t>(0U);

    LogInfo("Radio Id Lookups");
    LogInfo("    File: %s", ridLookupFile.length() > 0U ? ridLookupFile.c_str() : "None");
    if (ridReloadTime > 0U)
        LogInfo("    Reload: %u mins", ridReloadTime);
    
    m_ridLookup = new RadioIdLookup(ridLookupFile, ridReloadTime, true);
    m_ridLookup->read();

    // initialize master networking
    ret = createMasterNetwork();
    if (!ret)
        return EXIT_FAILURE;

    // initialize peer networking
    ret = createPeerNetworks();
    if (!ret)
        return EXIT_FAILURE;

    ::LogInfoEx(LOG_HOST, "FNE is up and running");

    StopWatch stopWatch;
    stopWatch.start();

    // main execution loop
    while (!g_killed) {
        uint32_t ms = stopWatch.elapsed();

        ms = stopWatch.elapsed();
        stopWatch.start();

        // ------------------------------------------------------
        //  -- Network Clocking                               --
        // ------------------------------------------------------

        // clock master
        if (m_network != nullptr)
            m_network->clock(ms);

        // clock peers
        for (auto network : m_peerNetworks) {
            network::Network* peerNetwork = network.second;
            if (peerNetwork != nullptr)
                peerNetwork->clock(ms);
        }

        if (ms < 2U)
            Thread::sleep(1U);
    }

    if (m_network != nullptr) {
        m_network->close();
        delete m_network;
    }

    for (auto network : m_peerNetworks) {
        network::Network* peerNetwork = network.second;
        if (peerNetwork != nullptr)
            peerNetwork->close();
    }
    m_peerNetworks.clear();

    if (m_tidLookup != nullptr) {
        m_tidLookup->stop();
        delete m_tidLookup;
    }
    if (m_ridLookup != nullptr) {
        m_ridLookup->stop();
        delete m_ridLookup;
    }

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Reads basic configuration parameters from the YAML configuration file.
/// </summary>
bool HostFNE::readParams()
{
    yaml::Node systemConf = m_conf["system"];
    m_pingTime = systemConf["pingTime"].as<uint32_t>(5U);
    m_maxMissedPings = systemConf["maxMissedPings"].as<uint32_t>(5U);
    m_updateLookupTime = systemConf["tgRuleUpdateTime"].as<uint32_t>(10U);

    if (m_pingTime == 0U) {
        m_pingTime = 5U;
    }

    if (m_maxMissedPings == 0U) {
        m_maxMissedPings = 5U;
    }

    if (m_updateLookupTime == 0U) {
        m_updateLookupTime = 10U;
    }

    m_allowActivityTransfer = systemConf["allowActivityTransfer"].as<bool>(true);
    m_allowDiagnosticTransfer = systemConf["allowDiagnosticTransfer"].as<bool>(true);

    LogInfo("General Parameters");
    LogInfo("    Peer Ping Time: %us", m_pingTime);
    LogInfo("    Maximum Missed Pings: %u", m_maxMissedPings);
    LogInfo("    Talkgroup Rule Update Time: %u mins", m_updateLookupTime);

    LogInfo("    Allow Activity Log Transfer: %s", m_allowActivityTransfer ? "yes" : "no");
    LogInfo("    Allow Diagnostic Log Transfer: %s", m_allowDiagnosticTransfer ? "yes" : "no");

    // attempt to load and populate routing rules
    yaml::Node masterConf = m_conf["master"];
    yaml::Node talkgroupRules = masterConf["talkgroup_rules"];
    std::string talkgroupConfig = talkgroupRules["file"].as<std::string>();
    uint32_t talkgroupConfigReload = talkgroupRules["time"].as<uint32_t>(30U);

    LogInfo("Talkgroup Rule Lookups");
    LogInfo("    File: %s", talkgroupConfig.length() > 0U ? talkgroupConfig.c_str() : "None");
    if (talkgroupConfigReload > 0U)
        LogInfo("    Reload: %u mins", talkgroupConfigReload);

    m_tidLookup = new TalkgroupRulesLookup(talkgroupConfig, talkgroupConfigReload, true);
    m_tidLookup->read();

    return true;
}

/// <summary>
/// Initializes master FNE network connectivity.
/// </summary>
bool HostFNE::createMasterNetwork()
{
    yaml::Node masterConf = m_conf["master"];
    bool netEnable = masterConf["enable"].as<bool>(true);

    // dump out if networking is disabled
    if (!netEnable) {
        return true;
    }

    std::string address = masterConf["address"].as<std::string>();
    uint16_t port = (uint16_t)masterConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
    uint32_t id = masterConf["peerId"].as<uint32_t>(1001U);
    std::string password = masterConf["password"].as<std::string>();
    bool debug = masterConf["debug"].as<bool>(false);

    m_dmrEnabled = masterConf["allowDMRTraffic"].as<bool>(true);
    m_p25Enabled = masterConf["allowP25Traffic"].as<bool>(true);
    m_nxdnEnabled = masterConf["allowNXDNTraffic"].as<bool>(true);

    LogInfo("Network Parameters");
    LogInfo("    Enabled: %s", netEnable ? "yes" : "no");
    if (netEnable) {
        LogInfo("    Peer ID: %u", id);
        LogInfo("    Address: %s", address.c_str());
        LogInfo("    Port: %u", port);
        LogInfo("    Allow DMR Traffic: %s", m_dmrEnabled ? "yes" : "no");
        LogInfo("    Allow P25 Traffic: %s", m_p25Enabled ? "yes" : "no");
        LogInfo("    Allow NXDN Traffic: %s", m_nxdnEnabled ? "yes" : "no");

        if (debug) {
            LogInfo("    Debug: yes");
        }
    }

    // initialize networking
    if (netEnable) {
        m_network = new FNENetwork(this, address, port, id, password, debug, m_dmrEnabled, m_p25Enabled, m_nxdnEnabled, m_allowActivityTransfer, m_allowDiagnosticTransfer, 
            m_pingTime, m_updateLookupTime);

        m_network->setLookups(m_ridLookup, m_tidLookup);

        bool ret = m_network->open();
        if (!ret) {
            delete m_network;
            m_network = nullptr;
            LogError(LOG_HOST, "failed to initialize traffic networking!");
            return false;
        }

        m_network->enable(true);
    }

    return true;
}

/// <summary>
/// Initializes peer FNE network connectivity.
/// </summary>
bool HostFNE::createPeerNetworks()
{
    yaml::Node& peerList = m_conf["peers"];
    if (peerList.size() > 0U) {
        for (size_t i = 0; i < peerList.size(); i++) {
            yaml::Node& peerConf = peerList[i];

            bool enabled = peerConf["enabled"].as<bool>(false);
            std::string address = peerConf["address"].as<std::string>();
            uint16_t port = (uint16_t)peerConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
            std::string masterAddress = peerConf["masterAddress"].as<std::string>();
            uint16_t masterPort = (uint16_t)peerConf["masterPort"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
            std::string password = peerConf["password"].as<std::string>();
            uint32_t id = peerConf["peerId"].as<uint32_t>(1001U);
            bool debug = peerConf["debug"].as<bool>(false);

            std::string identity = peerConf["identity"].as<std::string>();
            uint32_t rxFrequency = peerConf["rxFrequency"].as<uint32_t>(0U);
            uint32_t txFrequency = peerConf["txFrequency"].as<uint32_t>(0U);
            float latitude = peerConf["latitude"].as<float>(0.0F);
            float longitude = peerConf["longitude"].as<float>(0.0F);
            std::string location = peerConf["location"].as<std::string>();

            ::LogInfoEx(LOG_HOST, "Peer ID %u Master Address %s Master Port %u Identity %s Enabled %u", id, masterAddress.c_str(), masterPort, identity.c_str(), enabled);

            // initialize networking
            network::Network* network = new Network(address, port, 0U, id, password, true, debug, m_dmrEnabled, m_p25Enabled, m_nxdnEnabled, true, true, m_allowActivityTransfer, m_allowDiagnosticTransfer, false);
            network->setMetadata(identity, rxFrequency, txFrequency, 0.0F, 0.0F, 0, 0, 0, latitude, longitude, 0, location);

            network->enable(enabled);
            if (enabled) {
                bool ret = network->open();
                if (!ret) {
                    LogError(LOG_HOST, "failed to initialize traffic networking for PEER %u", id);
                    network->enable(false);
                    network->close();
                }
            }

            m_peerNetworks[identity] = network;
        }
    }

    return true;
}