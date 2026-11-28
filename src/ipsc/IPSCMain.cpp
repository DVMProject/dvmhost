// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "HostIPSC.h"
#include "IPSCMain.h"
#include "common/Log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <signal.h>

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
    ::memset(buffer, 0x20U, sizeof(buffer));

    va_list vl;
    va_start(vl, msg);
    ::vsprintf(buffer, msg, vl);
    va_end(vl);

    ::fprintf(stderr, "%s: FATAL PANIC; %s\n", g_progExe.c_str(), buffer);
    exit(EXIT_FAILURE);
}

/* Helper to print command line usage. (And optionally an error.) */

void usage(const char* message, const char* arg)
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2026 Bryan Biedenkapp, N2PLL and DVMProject "
                      "(https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, "
                      "G4KLX and others\n");
    ::fprintf(stdout, HIGHLY_UNNECESSARY_DISCLAIMER_FOR_THE_MENTAL "\n\n");
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout,
              "usage: %s [-vhf]"
              "[-c <configuration file>]"
              "\n\n"
              "  -v        show version information\n"
              "  -h        show this screen\n"
              "  -f        foreground mode\n"
              "\n"
              "  -c <file> specifies the configuration file to use\n"
              "\n"
              "  --        stop handling options\n",
              g_progExe.c_str());

    exit(message == nullptr ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* Helper to validate the command line arguments. */

int checkArgs(int argc, char* argv[])
{
    int i, p = 0;
    for (i = 1; i <= argc; i++) {
        if (argv[i] == nullptr)
            break;
        if (*argv[i] != '-')
            continue;
        else if (IS("--")) {
            ++p;
            break;
        } else if (IS("-f")) {
            g_foreground = true;
        } else if (IS("-c")) {
            if (i + 1 >= argc)
                usage("error: %s", "must specify the configuration file to use");
            g_iniFile = std::string(argv[++i]);
            if (g_iniFile.empty())
                usage("error: %s", "configuration file cannot be blank!");
            p += 2;
        } else if (IS("-v")) {
            ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
            ::fprintf(stdout, "Copyright (c) 2026 Bryan Biedenkapp, N2PLL and DVMProject "
                              "(https://github.com/dvmproject) Authors.\n");
            ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, "
                              "G4KLX and others\n");
            ::fprintf(stdout, HIGHLY_UNNECESSARY_DISCLAIMER_FOR_THE_MENTAL "\n");
            if (argc == 2)
                exit(EXIT_SUCCESS);
        } else if (IS("-h")) {
            usage(nullptr, nullptr);
        } else {
            usage("unrecognized option `%s'", argv[i]);
        }
    }

    if (p < 0 || p > argc)
        p = 0;
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

    uint32_t hash = ::strtoul(__GIT_VER_HASH__, nullptr, 16);
    SET_UINT32(hash, g_gitHashBytes, 0U);

    if (argv[0] != nullptr && *argv[0] != 0)
        g_progExe = std::string(argv[0]);

    if (argc > 1) {
        int i = checkArgs(argc, argv);
        if (i < argc) {
            argc -= i;
            argv += i;
        } else {
            argc--;
            argv++;
        }
    }

    log_stacktrace::SignalHandling sh(g_foreground);

    ::signal(SIGINT, sigHandler);
    ::signal(SIGTERM, sigHandler);
#if !defined(_WIN32)
    ::signal(SIGHUP, sigHandler);
#endif

    int ret = EXIT_SUCCESS;
    do {
        g_signal = 0;
        g_killed = false;

        ipsc::HostIPSC* host = new ipsc::HostIPSC(g_iniFile);
        ret = host->run();
        delete host;

        if (g_signal == SIGINT)
            ::LogInfoEx(LOG_HOST, "Exited on receipt of SIGINT");
        if (g_signal == SIGTERM)
            ::LogInfoEx(LOG_HOST, "Exited on receipt of SIGTERM");
        if (g_signal == SIGHUP)
            ::LogInfoEx(LOG_HOST, "Restarting on receipt of SIGHUP");
    } while (g_signal == SIGHUP);

    ::LogFinalise();
    delete[] g_gitHashBytes;
    g_gitHashBytes = nullptr;
    return ret;
}
#endif
