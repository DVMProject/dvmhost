// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2020-2023 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "HostMain.h"
#include "Host.h"
#include "calibrate/HostCal.h"
#include "setup/HostSetup.h"
#include "ActivityLog.h"

using namespace network;
using namespace modem;
using namespace lookups;

#include <cstdio>
#include <cstdarg>
#include <vector>

#include <signal.h>

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define IS(s) (::strcmp(argv[i], s) == 0)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

int g_signal = 0;
bool g_calibrate = false;
bool g_setup = false;
std::string g_progExe = std::string(__EXE_NAME__);
std::string g_iniFile = std::string(DEFAULT_CONF_FILE);
std::string g_lockFile = std::string(DEFAULT_LOCK_FILE);

bool g_foreground = false;
bool g_killed = false;

bool g_remoteModemMode = false;
std::string g_remoteAddress = std::string("127.0.0.1");
uint16_t g_remotePort = REMOTE_MODEM_PORT;

bool g_fireDMRBeacon = false;
bool g_fireP25Control = false;
bool g_fireNXDNControl = false;

bool g_fireCCVCNotification = false;

uint8_t* g_gitHashBytes = nullptr;

bool g_disableNonAuthoritativeLogging = false;

bool g_modemDebug = false;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

#if !defined(CATCH2_TEST_COMPILATION)
/* Internal signal handler. */

static void sigHandler(int signum)
{
    g_signal = signum;
    g_killed = true;
}
#endif

/* Helper to print a fatal error message and exit. */

void fatal(const char* msg, ...)
{
    char buffer[400U];
    ::memset(buffer, 0x20U, 400U);

    va_list vl;
    va_start(vl, msg);

    ::vsprintf(buffer, msg, vl);

    va_end(vl);

    ::fprintf(stderr, "%s: FATAL PANIC; %s\n", g_progExe.c_str(), buffer);
    exit(EXIT_FAILURE);
}

/* Helper to pring usage the command line arguments. (And optionally an error.) */

void usage(const char* message, const char* arg)
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2017-2025 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, 
        "usage: %s [-vhdf]"
        "[--syslog]"
#if defined(ENABLE_SETUP_TUI)
        "[--setup]"
#else
        "[--cal]"
#endif
        "[-c <configuration file>]"
        "[--remote [-a <address>] [-p <port>]]"
        "\n\n"
        "  -v        show version information\n"
        "  -h        show this screen\n"
        "  -d        force modem debug\n"
        "  -f        foreground mode\n"
        "\n"
        "  --syslog  force logging to syslog\n"
        "\n"
#if defined(ENABLE_SETUP_TUI)
        "  --setup   setup and calibration mode\n"
#else
        "  --cal     old calibration mode\n"
#endif
        "\n"
        "  -c <file> specifies the configuration file to use\n"
        "\n"
        "  --remote  remote modem mode\n"
        "  -a        remote modem command address\n"
        "  -p        remote modem command port\n"
        "\n"
        "  --        stop handling options\n",
        g_progExe.c_str());
    exit(EXIT_FAILURE);
}

/* Helper to validate the command line arguments. */

int checkArgs(int argc, char* argv[])
{
    int i, p = 0;

    // iterate through arguments
    for (i = 1; i <= argc; i++)
    {
        if (argv[i] == nullptr) {
            break;
        }

        if (*argv[i] != '-') {
            continue;
        }
        else if (IS("--")) {
            ++p;
            break;
        }
        else if (IS("-d")) {
            g_modemDebug = true;
        }
        else if (IS("-f")) {
            g_foreground = true;
        }
        else if (IS("--syslog")) {
            g_useSyslog = true;
        }
        else if (IS("--cal")) {
            g_calibrate = true;
        }
        else if (IS("--setup")) {
#if defined(ENABLE_SETUP_TUI)
            g_setup = true;
#else
            g_calibrate = true;
#endif // defined(ENABLE_SETUP_TUI)
        }
        else if (IS("-c")) {
            if (argc-- <= 0)
                usage("error: %s", "must specify the configuration file to use");
            g_iniFile = std::string(argv[++i]);

            if (g_iniFile.empty())
                usage("error: %s", "configuration file cannot be blank!");

            p += 2;
        }
        else if (IS("--remote")) {
            g_remoteModemMode = true;
        }
        else if (IS("-a")) {
            if ((argc - 1) <= 0)
                usage("error: %s", "must specify the address to connect to");
            g_remoteAddress = std::string(argv[++i]);

            if (g_remoteAddress.empty())
                usage("error: %s", "remote address cannot be blank!");

            p += 2;
        }
        else if (IS("-p")) {
            if ((argc - 1) <= 0)
                usage("error: %s", "must specify the port to connect to");
            g_remotePort = (uint16_t)::atoi(argv[++i]);

            if (g_remotePort == 0)
                usage("error: %s", "remote port number cannot be blank or 0!");

            p += 2;
        }
        else if (IS("-v")) {
            ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
            ::fprintf(stdout, "Copyright (c) 2017-2025 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
            ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n");
            if (argc == 2)
                exit(EXIT_SUCCESS);
        }
        else if (IS("-h")) {
            usage(nullptr, nullptr);
            if (argc == 2)
                exit(EXIT_SUCCESS);
        }
        else {
            usage("unrecognized option `%s'", argv[i]);
        }
    }

    if (p < 0 || p > argc) {
        p = 0;
    }

    return ++p;
}

// ---------------------------------------------------------------------------
//  Program Entry Point
// ---------------------------------------------------------------------------
#if !defined(CATCH2_TEST_COMPILATION)
int main(int argc, char** argv)
{
    g_gitHashBytes = new uint8_t[4U];
    ::memset(g_gitHashBytes, 0x00U, 4U);

    uint32_t hash = ::strtoul(__GIT_VER_HASH__, 0, 16);
    __SET_UINT32(hash, g_gitHashBytes, 0U);

    if (argv[0] != nullptr && *argv[0] != 0)
        g_progExe = std::string(argv[0]);

    if (argc > 1) {
        // check arguments
        int i = checkArgs(argc, argv);
        if (i < argc) {
            argc -= i;
            argv += i;
        }
        else {
            argc--;
            argv++;
        }
    }

    ::signal(SIGINT, sigHandler);
    ::signal(SIGTERM, sigHandler);
#if !defined(_WIN32)
    ::signal(SIGHUP, sigHandler);
#endif // !defined(_WIN32)

    int ret = 0;

    do {
        g_signal = 0;
        g_killed = false;

        if (g_calibrate || g_setup) {
#if defined(ENABLE_SETUP_TUI)
            if (g_setup) {
                HostSetup* setup = new HostSetup(g_iniFile);
                ret = setup->run(argc, argv);
                delete setup;
            }
            else {
                HostCal* cal = new HostCal(g_iniFile);
                ret = cal->run(argc, argv);
                delete cal;
            }
#else
            HostCal* cal = new HostCal(g_iniFile);
            ret = cal->run(argc, argv);
            delete cal;
#endif // defined(ENABLE_SETUP_TUI)
        }
        else {
            Host* host = new Host(g_iniFile);
            ret = host->run();
            delete host;
        }

        if (g_signal == 2)
            ::LogInfoEx(LOG_HOST, "[STOP] dvmhost:main SIGINT");

        if (g_signal == 15)
            ::LogInfoEx(LOG_HOST, "[STOP] dvmhost:main SIGTERM");

        if (g_signal == 1)
            ::LogInfoEx(LOG_HOST, "[RSTR] dvmhost:main SIGHUP");
    } while (g_signal == 1);

    ::LogFinalise();
    ::ActivityLogFinalise();

    return ret;
}
#endif