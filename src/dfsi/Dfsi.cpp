// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/p25/P25Utils.h"
#include "common/network/udp/Socket.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "common/Utils.h"
#include "host/ActivityLog.h"
#include "Dfsi.h"
#include "DfsiMain.h"

using namespace network;
using namespace lookups;

#include <cstdio>
#include <algorithm>
#include <functional>
#include <random>

#include <sys/utsname.h>
#include <unistd.h>
#include <pwd.h>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define IDLE_WARMUP_MS 5U

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the HostTest class. */

Dfsi::Dfsi(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_network(nullptr),
    m_ridLookup(nullptr),
    m_tidLookup(nullptr),
    m_pingTime(5U),
    m_maxMissedPings(5U),
    m_updateLookupTime(10U),
    m_debug(false),
    m_repeatTraffic(true),
    m_serial(nullptr)
{
    /* stub */
}

/* Finalizes a instance of the HostTest class. */

Dfsi::~Dfsi() = default;

/* Executes the main FNE processing loop. */

int Dfsi::run()
{
    bool ret = false;

    // Try and parse the config yaml
    try {
        ret = yaml::Parse(m_conf, m_confFile.c_str());
        if (!ret) {
            ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
        }
    }
    catch (yaml::OperationException const& e) {
        ::fatal("cannot read the configuration file - %s (%s)", m_confFile.c_str(), e.message());
    }

    // Check if we should run as a daemn or not
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

    // Init activity logging
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

    ::LogInfo(__BANNER__ "\r\n" __PROG_NAME__ " " __VER__ " (built " __BUILD__ ")\r\n" \
        "Copyright (c) 2024 Patrick McDonnell, W3AXL and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
        ">> DFSI Network Peer\r\n");

    // read base parameters from configuration
    ret = readParams();
    if (!ret)
        return EXIT_FAILURE;

    // Read DFSI config
    yaml::Node dfsi_conf = m_conf["dfsi"];
    uint16_t dfsiMode = dfsi_conf["mode"].as<uint16_t>();
    uint32_t p25BufferSize = dfsi_conf["p25BufferSize"].as<uint32_t>();
    uint16_t callTimeout = dfsi_conf["callTimeout"].as<uint16_t>();

    // initialize peer networking
    ret = createPeerNetwork();
    if (!ret)
        return EXIT_FAILURE;

    std::string dfsiModeStr = "Unknown";

    switch (dfsiMode) {
        case DFSI_MODE_UDP_FNE:
        {
            dfsiModeStr = "UDP DFSI to FNE";
            LogError(LOG_HOST, "UDP DFSI mode not yet supported, sorry!");
            return EXIT_FAILURE;
        }
        break;
        case DFSI_MODE_V24_FNE:
        {
            dfsiModeStr = "V24 DFSI to FNE";
            ret = createSerialNetwork(p25BufferSize, callTimeout);
            if (!ret)
                return EXIT_FAILURE;
        }
        break;
        case DFSI_MODE_UDP_V24:
        {
            dfsiModeStr = "UDP DFSI to V24 DFSI";
            LogError(LOG_HOST, "UDP to V24 mode not yet supported, sorry!");
            return EXIT_FAILURE;
        }
        break;
        default:
        {
            LogError(LOG_HOST, "Invalid DFSI mode specified: %d", dfsiMode);
            return EXIT_FAILURE;
        }
        break;
    }

    LogInfo("DFSI Parameters");
    LogInfo("    Mode: %u (%s)", dfsiMode, dfsiModeStr.c_str());
    LogInfo("    P25 Buffer Size: %u bytes", p25BufferSize);
    LogInfo("    Call Timeout:    %u ms", callTimeout);

    StopWatch stopWatch;
    stopWatch.start();

    /*
    ** Main execution loop
    */

    struct utsname utsinfo;
    ::memset(&utsinfo, 0, sizeof(utsinfo));
    ::uname(&utsinfo);

    ::LogInfoEx(LOG_HOST, "[ OK ] DFSI is up and running on %s %s %s", utsinfo.sysname, utsinfo.release, utsinfo.machine);
    
    // Get serial board info if connected
    if (m_serial)
    {
        m_serial->getBoardInfo();
    }

    // main execution loop
    while (!g_killed) {
        uint32_t ms = stopWatch.elapsed();

        ms = stopWatch.elapsed();
        stopWatch.start();

        // ------------------------------------------------------
        //  -- Network RX Clocking                             --
        // ------------------------------------------------------

        if (m_network != nullptr)
            m_network->clock(ms);

        uint32_t length = 0U;
        bool netReadRet = false;

        UInt8Array p25Buffer = m_network->readP25(netReadRet, length);
        if (netReadRet) {
            // Send the data to the serial handler if serial is up
            if (m_serial != nullptr)
                m_serial->processP25FromNet(std::move(p25Buffer), length);
        }

        // ------------------------------------------------------
        //  -- Network TX Clocking                             --
        // ------------------------------------------------------

        // Processes data in the serial rx P25 buffer and sends it to the network buffer for sending, if serial is up
        if (m_serial != nullptr) {
            m_serial->processP25ToNet();
        }

        // ------------------------------------------------------
        //  -- Serial Clocking                                 --
        // ------------------------------------------------------

        if (m_serial != nullptr) {
            m_serial->clock(ms);
        }

        // Timekeeping
        if (ms < 2U)
            Thread::sleep(1U);
    }

    ::LogSetNetwork(nullptr);
    if (m_network != nullptr) {
        m_network->close();
        delete m_network;
    }

    if (m_serial != nullptr) {
        m_serial->close();
        delete m_serial;
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

/* Reads basic configuration parameters from the YAML configuration file. */

bool Dfsi::readParams()
{
    // No basic config params right now

    return true;
}

/* Initializes peer network connectivity. */

bool Dfsi::createPeerNetwork()
{
    yaml::Node networkConf = m_conf["network"];
    std::string password = networkConf["password"].as<std::string>();

    std::string address = networkConf["address"].as<std::string>();
    uint16_t port = networkConf["port"].as<uint16_t>();
    uint32_t id = networkConf["peerId"].as<uint32_t>();

    bool encrypted = networkConf["encrypted"].as<bool>(false);
    std::string key = networkConf["presharedKey"].as<std::string>();
    uint8_t presharedKey[AES_WRAPPED_PCKT_KEY_LEN];
    if (!key.empty()) {
        if (key.size() == 32) {
            // bryanb: shhhhhhh....dirty nasty hacks
            key = key.append(key); // since the key is 32 characters (16 hex pairs), double it on itself for 64 characters (32 hex pairs)
            LogWarning(LOG_HOST, "Half-length network preshared encryption key detected, doubling key on itself.");
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
                LogWarning(LOG_HOST, "Invalid characters in the network preshared encryption key. Encryption disabled.");
                encrypted = false;
            }
        }
        else {
            LogWarning(LOG_HOST, "Invalid  network preshared encryption key length, key should be 32 hex pairs, or 64 characters. Encryption disabled.");
            encrypted = false;
        }
    }

    std::string identity = networkConf["identity"].as<std::string>();

    bool netDebug = networkConf["debug"].as<bool>();

    LogInfo("Network Parameters");
    LogInfo("    Identity:  %s", identity.c_str());
    LogInfo("    Peer ID:   %u", id);
    LogInfo("    Address:   %s", address.c_str());
    LogInfo("    Port:      %u", port);
    LogInfo("    Encrypted: %s", encrypted ? "yes" : "no");

    if (id > 999999999U) {
        ::LogError(LOG_HOST, "Network Peer ID cannot be greater then 999999999.");
        return false;
    }

    // initialize networking
    m_network = new DfsiPeerNetwork(address, port, 0U, id, password, true, netDebug, false, true, false, true, true, true, true, true, false);
    m_network->setMetadata(identity, 0U, 0U, 0.0F, 0.0F, 0, 0, 0, 0.0F, 0.0F, 0, "");
    m_network->setLookups(m_ridLookup, m_tidLookup);

    ::LogSetNetwork(m_network);

    if (encrypted) {
        m_network->setPresharedKey(presharedKey);
    }

    m_network->enable(true);
    bool ret = m_network->open();
    if (!ret) {
        delete m_network;
        m_network = nullptr;
        LogError(LOG_HOST, "failed to initialize traffic networking for PEER %u", id);
        return false;
    }

    ::LogSetNetwork(m_network);

    return true;
}

/* Initializes serial V.24 network. */

bool Dfsi::createSerialNetwork(uint32_t p25BufferSize, uint16_t callTimeout)
{
    // Read serial config
    yaml::Node dfsi_conf = m_conf["dfsi"];
    yaml::Node serial_conf = dfsi_conf["serial"];
    std::string portType = serial_conf["portType"].as<std::string>("null");
    std::string port = serial_conf["port"].as<std::string>();
    uint32_t baudrate = serial_conf["baudrate"].as<uint32_t>();
    bool rtrt = serial_conf["rtrt"].as<bool>();
    bool diu = serial_conf["diu"].as<bool>();
    uint16_t jitter = serial_conf["jitter"].as<uint16_t>();
    bool serial_debug = serial_conf["debug"].as<bool>();
    bool serial_trace = serial_conf["trace"].as<bool>();

    LogInfo("Serial Parameters");
    LogInfo("    Port Type:   %s", portType.c_str());
    LogInfo("    Port:        %s", port.c_str());
    LogInfo("    Baudrate:    %u", baudrate);
    LogInfo("    RT/RT:       %s", rtrt ? "Enabled" : "Disabled");
    LogInfo("    DIU Flag:    %s", diu ? "Enabled" : "Disabled");
    LogInfo("    Jitter Size: %u ms", jitter);
    LogInfo("    Debug:       %s", serial_debug ? "Enabled" : "Disabled");
    LogInfo("    Trace:       %s", serial_trace ? "Enabled" : "Disabled");

    // Create serial service
    m_serial = new SerialService(portType, port, baudrate, rtrt, diu, jitter, m_network, p25BufferSize, p25BufferSize, callTimeout, serial_debug, serial_trace);

    // Open serial
    bool ret = m_serial->open();
    if (!ret) {
        delete m_serial;
        m_serial = nullptr;
        LogError(LOG_HOST, "failed to initialied serial V24 interface");
        return false;
    }

    return true;
}