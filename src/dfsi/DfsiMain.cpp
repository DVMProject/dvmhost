// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/
#include "Defines.h"
#include "common/Log.h"
#include "dfsi/ActivityLog.h"
#include "DfsiMain.h"
#include "Dfsi.h"

using namespace network;
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
std::string g_progExe = std::string(__EXE_NAME__);
std::string g_iniFile = std::string(DEFAULT_CONF_FILE);
std::string g_lockFile = std::string(DEFAULT_LOCK_FILE);

std::string g_masterAddress = std::string("127.0.0.1");
uint16_t g_masterPort = 63031;
uint32_t g_peerId = 9000999;

bool g_foreground = false;
bool g_killed = false;
bool g_hideMessages = false;

uint8_t* g_gitHashBytes = nullptr;

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
    ::fprintf(stdout, "Copyright (c) 2024 DVMProject (https://github.com/dvmproject) Authors.\n");
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, 
        "usage: %s [-vhf]"
        "[-c <configuration file>]"
        "[-a <address>] [-p <port>] [-P <peer id>]"
        "\n\n"
        "  -v        show version information\n"
        "  -h        show this screen\n"
        "  -f        foreground mode\n"
        "\n"
        "  -c <file> specifies the configuration file to use\n"
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
        else if (IS("-f")) {
            g_foreground = true;
        }
        else if (IS("-s")) {
            g_hideMessages = true;
        }
        else if (IS("-c")) {
            if (argc-- <= 0)
                usage("error: %s", "must specify the configuration file to use");
            g_iniFile = std::string(argv[++i]);

            if (g_iniFile.empty())
                usage("error: %s", "configuration file cannot be blank!");

            p += 2;
        }
        else if (IS("-v")) {
            ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
            ::fprintf(stdout, "Copyright (c) 2017-2024 Patrick McDonnell, W3AXL and DVMProject (https://github.com/dvmproject) Authors.\n");
            ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
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

        Dfsi *dfsi = new Dfsi(g_iniFile);
        ret = dfsi->run();
        delete dfsi;

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