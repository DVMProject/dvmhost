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
*   Copyright (C) 2020,2021 by Bryan Biedenkapp N2PLL
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
#include "HostMain.h"
#include "host/Host.h"
#include "host/calibrate/HostCal.h"
#include "host/setup/HostSetup.h"
#include "Log.h"

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

bool g_fireDMRBeacon = false;
bool g_fireP25Control = false;
bool g_interruptP25Control = false;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

#if !defined(_WIN32) && !defined(_WIN64)
static void sigHandler(int signum)
{
    g_killed = true;
    g_signal = signum;
}
#endif

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

void usage(const char* message, const char* arg)
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    if (message != NULL) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, "usage: %s [-v] [-f] [--cal] [--setup] [-c <configuration file>]\n\n"
        "  -f       foreground mode\n"
        "  --cal    calibration mode\n"
        "  --setup  setup mode\n"
        "\n"
        "  -v       show version information\n"
        "  -h       show this screen\n"
        "  --       stop handling options\n",
        g_progExe.c_str());
    exit(EXIT_FAILURE);
}

int checkArgs(int argc, char* argv[])
{
    int i, p = 0;

    // iterate through arguments
    for (i = 1; i <= argc; i++)
    {
        if (argv[i] == NULL) {
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
        else if (IS("--cal")) {
            g_calibrate = true;
        }
        else if (IS("--setup")) {
            g_setup = true;
        }
        else if (IS("-c")) {
            if (argc-- <= 0)
                usage("error: %s", "must specify the configuration file to use");
            g_iniFile = std::string(argv[++i]);

            if (g_iniFile == "")
                usage("error: %s", "configuration file cannot be blank!");

            p += 2;
        }
        else if (IS("-v")) {
            ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
            if (argc == 2)
                exit(EXIT_SUCCESS);
        }
        else if (IS("-h")) {
            usage(NULL, NULL);
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

int main(int argc, char** argv)
{
    if (argv[0] != NULL && *argv[0] != 0)
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

#if !defined(_WIN32) && !defined(_WIN64)
    ::signal(SIGINT, sigHandler);
    ::signal(SIGTERM, sigHandler);
    ::signal(SIGHUP, sigHandler);
#endif

    int ret = 0;

    do {
        g_signal = 0;

        if (g_calibrate || g_setup) {
            if (g_setup) {
                HostSetup* setup = new HostSetup(g_iniFile);
                ret = setup->run();
                delete setup;
            }
            else {
                HostCal* cal = new HostCal(g_iniFile);
                ret = cal->run();
                delete cal;
            }
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
