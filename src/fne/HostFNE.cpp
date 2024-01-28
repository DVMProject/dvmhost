// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Converged FNE Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Converged FNE Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "common/network/UDPSocket.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "network/fne/TagDMRData.h"
#include "network/fne/TagP25Data.h"
#include "network/fne/TagNXDNData.h"
#include "ActivityLog.h"
#include "HostFNE.h"
#include "FNEMain.h"

using namespace network;
using namespace lookups;

#include <cstdio>
#include <algorithm>
#include <functional>

#include <unistd.h>
#include <pwd.h>

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
    m_allowDiagnosticTransfer(false),
    m_RESTAPI(nullptr)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the HostFNE class.
/// </summary>
HostFNE::~HostFNE() = default;

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
        ::fatal("cannot read the configuration file - %s (%s)", m_confFile.c_str(), e.message());
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

    // handle POSIX process forking
    if (m_daemon) {
        // create new process
        pid_t pid = ::fork();
        if (pid == -1) {
            ::fprintf(stderr, "%s: Couldn't fork() , exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }
        else if (pid != 0) {
            ::LogFinalise();
            exit(EXIT_SUCCESS);
        }

        // create new session and process group
        if (::setsid() == -1) {
            ::fprintf(stderr, "%s: Couldn't setsid(), exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }

        // set the working directory to the root directory
        if (::chdir("/") == -1) {
            ::fprintf(stderr, "%s: Couldn't cd /, exiting\n", g_progExe.c_str());
            ::LogFinalise();
            return EXIT_FAILURE;
        }

        ::close(STDIN_FILENO);
        ::close(STDOUT_FILENO);
        ::close(STDERR_FILENO);
    }

    ::LogInfo(__PROG_NAME__ " %s (built %s)", __VER__, __BUILD__);
    ::LogInfo("Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.");
    ::LogInfo("Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others");
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

    // initialize REST API
    initializeRESTAPI();

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
            network::PeerNetwork* peerNetwork = network.second;
            if (peerNetwork != nullptr) {
                peerNetwork->clock(ms);

                // process peer network traffic
                processPeer(peerNetwork);
            }
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

    if (m_RESTAPI != nullptr) {
        m_RESTAPI->close();
        delete m_RESTAPI;
    }

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
/// <returns></returns>
bool HostFNE::readParams()
{
    yaml::Node systemConf = m_conf["system"];
    m_pingTime = systemConf["pingTime"].as<uint32_t>(5U);
    m_maxMissedPings = systemConf["maxMissedPings"].as<uint32_t>(5U);
    m_updateLookupTime = systemConf["tgRuleUpdateTime"].as<uint32_t>(10U);
    bool sendTalkgroups = systemConf["sendTalkgroups"].as<bool>(true);

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

    LogInfo("    Send Talkgroups: %s", sendTalkgroups ? "yes" : "no");

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
    m_tidLookup->sendTalkgroups(sendTalkgroups);
    m_tidLookup->read();

    return true;
}

/// <summary>
/// Initializes REST API serivces.
/// </summary>
/// <returns></returns>
bool HostFNE::initializeRESTAPI()
{
    yaml::Node systemConf = m_conf["system"];
    bool restApiEnable = systemConf["restEnable"].as<bool>(false);

    // dump out if both networking and REST API are disabled
    if (!restApiEnable) {
        return true;
    }

    std::string restApiAddress = systemConf["restAddress"].as<std::string>("127.0.0.1");
    uint16_t restApiPort = (uint16_t)systemConf["restPort"].as<uint32_t>(REST_API_DEFAULT_PORT);
    std::string restApiPassword = systemConf["restPassword"].as<std::string>();
    bool restApiDebug = systemConf["restDebug"].as<bool>(false);

    if (restApiPassword.length() > 64) {
        std::string password = restApiPassword;
        restApiPassword = password.substr(0, 64);

        ::LogWarning(LOG_HOST, "REST API password is too long; truncating to the first 64 characters.");
    }

    if (restApiPassword.empty() && restApiEnable) {
        ::LogWarning(LOG_HOST, "REST API password not provided; REST API disabled.");
        restApiEnable = false;
    }

    LogInfo("REST API Parameters");
    LogInfo("    REST API Enabled: %s", restApiEnable ? "yes" : "no");
    if (restApiEnable) {
        LogInfo("    REST API Address: %s", restApiAddress.c_str());
        LogInfo("    REST API Port: %u", restApiPort);

        if (restApiDebug) {
            LogInfo("    REST API Debug: yes");
        }
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
/// Initializes master FNE network connectivity.
/// </summary>
/// <returns></returns>
bool HostFNE::createMasterNetwork()
{
    yaml::Node masterConf = m_conf["master"];
    std::string address = masterConf["address"].as<std::string>();
    uint16_t port = (uint16_t)masterConf["port"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
    uint32_t id = masterConf["peerId"].as<uint32_t>(1001U);
    std::string password = masterConf["password"].as<std::string>();
    bool verbose = masterConf["verbose"].as<bool>(false);
    bool debug = masterConf["debug"].as<bool>(false);

    bool encrypted = masterConf["encrypted"].as<bool>(false);
    std::string key = masterConf["presharedKey"].as<std::string>();
    uint8_t presharedKey[AES_WRAPPED_PCKT_KEY_LEN];
    if (!key.empty()) {
        if (key.size() == 32) {
            // bryanb: shhhhhhh....dirty nasty hacks
            key = key.append(key); // since the key is 32 characters (16 hex pairs), double it on itself for 64 characters (32 hex pairs)
            LogWarning(LOG_HOST, "Half-length master network preshared encryption key detected, doubling key on itself.");
        }

        if (key.size() == 64) {
            if ((key.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
                const char* keyPtr = key.c_str();
                ::memset(presharedKey, 0x00U, AES_WRAPPED_PCKT_KEY_LEN);

                for (uint8_t i = 0; i < AES_WRAPPED_PCKT_KEY_LEN; i++) {
                    char t[4] = {keyPtr[0], keyPtr[1], 0};
                    presharedKey[i] = (uint8_t)::strtoul(t, NULL, 16);
                    keyPtr += 2 * sizeof(char);
                }
            }
            else {
                LogWarning(LOG_HOST, "Invalid characters in the master network preshared encryption key. Encryption disabled.");
                encrypted = false;
            }
        }
        else {
            LogWarning(LOG_HOST, "Invalid master  network preshared encryption key length, key should be 32 hex pairs, or 64 characters. Encryption disabled.");
            encrypted = false;
        }
    }

    if (id > 999999999U) {
        ::LogError(LOG_HOST, "Network Peer ID cannot be greater then 999999999.");
        return false;
    }

    m_dmrEnabled = masterConf["allowDMRTraffic"].as<bool>(true);
    m_p25Enabled = masterConf["allowP25Traffic"].as<bool>(true);
    m_nxdnEnabled = masterConf["allowNXDNTraffic"].as<bool>(true);

    uint32_t parrotDelay = masterConf["parrotDelay"].as<uint32_t>(2500U);
    if (m_pingTime * 1000U < parrotDelay) {
        LogWarning(LOG_HOST, "Parrot delay cannot be longer then the ping time of a peer. Reducing parrot delay to half the ping time.");
        parrotDelay = (m_pingTime * 1000U) / 2U;
    }
    bool parrotGrantDemand = masterConf["parrotGrantDemand"].as<bool>(true);

    LogInfo("Network Parameters");
    LogInfo("    Peer ID: %u", id);
    LogInfo("    Address: %s", address.c_str());
    LogInfo("    Port: %u", port);
    LogInfo("    Allow DMR Traffic: %s", m_dmrEnabled ? "yes" : "no");
    LogInfo("    Allow P25 Traffic: %s", m_p25Enabled ? "yes" : "no");
    LogInfo("    Allow NXDN Traffic: %s", m_nxdnEnabled ? "yes" : "no");
    LogInfo("    Parrot Repeat Delay: %u ms", parrotDelay);
    LogInfo("    Parrot Grant Demand: %s", parrotGrantDemand ? "yes" : "no");

    LogInfo("    Encrypted: %s", encrypted ? "yes" : "no");

    if (verbose) {
        LogInfo("    Verbose: yes");
    }

    if (debug) {
        LogInfo("    Debug: yes");
    }

    // initialize networking
    m_network = new FNENetwork(this, address, port, id, password, debug, verbose, m_dmrEnabled, m_p25Enabled, m_nxdnEnabled, 
        parrotDelay, parrotGrantDemand, m_allowActivityTransfer, m_allowDiagnosticTransfer, m_pingTime, m_updateLookupTime);

    m_network->setLookups(m_ridLookup, m_tidLookup);

    if (m_RESTAPI != nullptr) {
        m_RESTAPI->setNetwork(m_network);
    }

    bool ret = m_network->open();
    if (!ret) {
        delete m_network;
        m_network = nullptr;
        LogError(LOG_HOST, "failed to initialize traffic networking!");
        return false;
    }

    if (encrypted) {
        m_network->setPresharedKey(presharedKey);
    }

    return true;
}

/// <summary>
/// Initializes peer FNE network connectivity.
/// </summary>
/// <returns></returns>
bool HostFNE::createPeerNetworks()
{
    yaml::Node& peerList = m_conf["peers"];
    if (peerList.size() > 0U) {
        for (size_t i = 0; i < peerList.size(); i++) {
            yaml::Node& peerConf = peerList[i];

            bool enabled = peerConf["enabled"].as<bool>(false);
            std::string masterAddress = peerConf["masterAddress"].as<std::string>();
            uint16_t masterPort = (uint16_t)peerConf["masterPort"].as<uint32_t>(TRAFFIC_DEFAULT_PORT);
            std::string password = peerConf["password"].as<std::string>();
            uint32_t id = peerConf["peerId"].as<uint32_t>(1001U);
            bool debug = peerConf["debug"].as<bool>(false);

            bool encrypted = peerConf["encrypted"].as<bool>(false);
            std::string key = peerConf["presharedKey"].as<std::string>();
            uint8_t presharedKey[AES_WRAPPED_PCKT_KEY_LEN];
            if (!key.empty()) {
                if (key.size() == 32) {
                    // bryanb: shhhhhhh....dirty nasty hacks
                    key = key.append(key); // since the key is 32 characters (16 hex pairs), double it on itself for 64 characters (32 hex pairs)
                    LogWarning(LOG_HOST, "Half-length peer  network preshared encryption key detected, doubling key on itself.");
                }

                if (key.size() == 64) {
                    if ((key.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
                        const char* keyPtr = key.c_str();
                        ::memset(presharedKey, 0x00U, AES_WRAPPED_PCKT_KEY_LEN);

                        for (uint8_t i = 0; i < AES_WRAPPED_PCKT_KEY_LEN; i++) {
                            char t[4] = {keyPtr[0], keyPtr[1], 0};
                            presharedKey[i] = (uint8_t)::strtoul(t, NULL, 16);
                            keyPtr += 2 * sizeof(char);
                        }
                    }
                    else {
                        LogWarning(LOG_HOST, "Invalid characters in the peer network preshared encryption key. Encryption disabled.");
                        encrypted = false;
                    }
                }
                else {
                    LogWarning(LOG_HOST, "Invalid peer network preshared encryption key length, key should be 32 hex pairs, or 64 characters. Encryption disabled.");
                    encrypted = false;
                }
            }

            std::string identity = peerConf["identity"].as<std::string>();
            uint32_t rxFrequency = peerConf["rxFrequency"].as<uint32_t>(0U);
            uint32_t txFrequency = peerConf["txFrequency"].as<uint32_t>(0U);
            float latitude = peerConf["latitude"].as<float>(0.0F);
            float longitude = peerConf["longitude"].as<float>(0.0F);
            std::string location = peerConf["location"].as<std::string>();

            ::LogInfoEx(LOG_HOST, "Peer ID %u Master Address %s Master Port %u Identity %s Enabled %u Encrypted %u", id, masterAddress.c_str(), masterPort, identity.c_str(), enabled, encrypted);

            if (id > 999999999U) {
                ::LogError(LOG_HOST, "Network Peer ID cannot be greater then 999999999.");
                continue;
            }

            // initialize networking
            network::PeerNetwork* network = new PeerNetwork(masterAddress, masterPort, 0U, id, password, true, debug, m_dmrEnabled, m_p25Enabled, m_nxdnEnabled, true, true, m_allowActivityTransfer, m_allowDiagnosticTransfer, false);
            network->setMetadata(identity, rxFrequency, txFrequency, 0.0F, 0.0F, 0, 0, 0, latitude, longitude, 0, location);
            if (encrypted) {
                m_network->setPresharedKey(presharedKey);
            }

            network->enable(enabled);
            if (enabled) {
                bool ret = network->open();
                if (!ret) {
                    delete m_network;
                    m_network = nullptr;
                    LogError(LOG_HOST, "failed to initialize traffic networking for PEER %u", id);
                }
            }

            if (network != nullptr)
                m_peerNetworks[identity] = network;
        }
    }

    return true;
}

/// <summary>
/// Processes any peer network traffic.
/// </summary>
/// <param name="peerNetwork"></param>
void HostFNE::processPeer(network::PeerNetwork* peerNetwork)
{
    if (peerNetwork == nullptr)
        return; // this shouldn't happen...
    if (peerNetwork->getStatus() != NET_STAT_RUNNING)
        return;

    // process DMR data
    if (peerNetwork->hasDMRData()) {
        uint32_t length = 100U;
        bool ret = false;
        UInt8Array data = peerNetwork->readDMR(ret, length);
        if (ret) {
            uint32_t peerId = peerNetwork->getPeerId();
            uint32_t slotNo = (data[15U] & 0x80U) == 0x80U ? 2U : 1U;
            uint32_t streamId = peerNetwork->getDMRStreamId(slotNo);

            m_network->dmrTrafficHandler()->processFrame(data.get(), length, peerId, peerNetwork->pktLastSeq(), streamId, true);
        }
    }

    // process P25 data
    if (peerNetwork->hasP25Data()) {
        uint32_t length = 100U;
        bool ret = false;
        UInt8Array data = peerNetwork->readP25(ret, length);
        if (ret) {
            uint32_t peerId = peerNetwork->getPeerId();
            uint32_t streamId = peerNetwork->getP25StreamId();

            m_network->p25TrafficHandler()->processFrame(data.get(), length, peerId, peerNetwork->pktLastSeq(), streamId, true);
        }
    }

    // process NXDN data
    if (peerNetwork->hasNXDNData()) {
        uint32_t length = 100U;
        bool ret = false;
        UInt8Array data = peerNetwork->readNXDN(ret, length);
        if (ret) {
            uint32_t peerId = peerNetwork->getPeerId();
            uint32_t streamId = peerNetwork->getNXDNStreamId();

            m_network->nxdnTrafficHandler()->processFrame(data.get(), length, peerId, peerNetwork->pktLastSeq(), streamId, true);
        }
    }
}