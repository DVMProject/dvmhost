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
*   Copyright (C) 2020-2023 by Bryan Biedenkapp N2PLL
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

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>

// ---------------------------------------------------------------------------
//	Macros
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

uint8_t* g_gitHashBytes = nullptr;

bool g_modemDebug = false;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

#if !defined(CATCH2_TEST_COMPILATION)
/// <summary>
/// Internal signal handler.
/// </summary>
/// <param name="signum"></param>
static void sigHandler(int signum)
{
    g_signal = signum;
    g_killed = true;
}
#endif

/// <summary>
/// Helper to print a fatal error message and exit.
/// </summary>
/// <remarks>This is a variable argument function.</remarks>
/// <param name="msg">Message.</param>
void fatal(const char* msg, ...)
{
    char buffer[400U];
    ::memset(buffer, 0x20U, 400U);

    va_list vl;
    va_start(vl, msg);

    ::vsprintf(buffer, msg, vl);

    va_end(vl);

    ::fprintf(stderr, "%s: %s\n", g_progExe.c_str(), buffer);
    exit(EXIT_FAILURE);
}

/// <summary>
/// Helper to pring usage the command line arguments. (And optionally an error.)
/// </summary>
/// <param name="message">Error message.</param>
/// <param name="arg">Error message arguments.</param>
void usage(const char* message, const char* arg)
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, 
        "usage: %s [-vhdf]"
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

/// <summary>
/// Helper to validate the command line arguments.
/// </summary>
/// <param name="argc">Argument count.</param>
/// <param name="argv">Array of argument strings.</param>
/// <returns>Count of remaining unprocessed arguments.</returns>
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

            if (g_iniFile == "")
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

            if (g_remoteAddress == "")
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
            ::fprintf(stdout, "Copyright (c) 2017-2024 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
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
    ::signal(SIGHUP, sigHandler);

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
            ::LogInfoEx(LOG_HOST, "Exited on receipt of SIGINT");

        if (g_signal == 15)
            ::LogInfoEx(LOG_HOST, "Exited on receipt of SIGTERM");

        if (g_signal == 1)
            ::LogInfoEx(LOG_HOST, "Restarting on receipt of SIGHUP");
    } while (g_signal == 1);

    ::LogFinalise();
    ::ActivityLogFinalise();

    return ret;
}
#endif