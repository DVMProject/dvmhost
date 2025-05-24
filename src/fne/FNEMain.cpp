// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2020-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/Log.h"
#include "ActivityLog.h"
#include "FNEMain.h"
#include "HostFNE.h"

using namespace network;
using namespace lookups;

#include <cstdio>
#include <cstdarg>
#include <vector>

#include <signal.h>

#if defined(_WIN32)
#include <ws2tcpip.h>
#include <Winsock2.h>
#endif // !defined(_WIN32)

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

#define IS(s) (::strcmp(argv[i], s) == 0)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

#ifndef SIGHUP
#define SIGHUP 1
#endif

int g_signal = 0;
std::string g_progExe = std::string(__EXE_NAME__);
std::string g_iniFile = std::string(DEFAULT_CONF_FILE);
std::string g_lockFile = std::string(DEFAULT_LOCK_FILE);

bool g_foreground = false;
bool g_killed = false;

uint8_t* g_gitHashBytes = nullptr;

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
        "usage: %s [-vhf]"
        "[--syslog]"
        "[-c <configuration file>]"
        "\n\n"
        "  -v        show version information\n"
        "  -h        show this screen\n"
        "  -f        foreground mode\n"
        "\n"
        "  --syslog  force logging to syslog\n"
        "\n"
        "  -c <file> specifies the configuration file to use\n"
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
        else if (IS("-f")) {
            g_foreground = true;
        }
        else if (IS("--syslog")) {
            g_useSyslog = true;
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
#if defined(_WIN32)
    WSAData data;
    int wsaRet = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (wsaRet != 0) {
        ::LogError(LOG_NET, "Error from WSAStartup, err: %d", wsaRet);
    }
#endif // defined(_WIN32)

    g_gitHashBytes = new uint8_t[4U];
    ::memset(g_gitHashBytes, 0x00U, 4U);

    uint32_t hash = ::strtoul(__GIT_VER_HASH__, 0, 16);
    SET_UINT32(hash, g_gitHashBytes, 0U);

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

        HostFNE *fne = new HostFNE(g_iniFile);
        ret = fne->run();
        delete fne;

        if (g_signal == SIGINT)
            ::LogInfoEx(LOG_HOST, "[STOP] dvmfne:main SIGINT");

        if (g_signal == SIGTERM)
            ::LogInfoEx(LOG_HOST, "[STOP] dvmfne:main SIGTERM");

        if (g_signal == SIGHUP)
            ::LogInfoEx(LOG_HOST, "[RSTR] dvmfne:main SIGHUP");
    } while (g_signal == SIGHUP);

    ::LogFinalise();
    ::ActivityLogFinalise();

#if defined(_WIN32)
    ::WSACleanup();
#endif // defined(_WIN32)

    return ret;
}
#endif