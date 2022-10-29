/**
* Digital Voice Modem - Remote Command Client
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Remote Command Client
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2019 by Jonathan Naylor G4KLX
*   Copyright (C) 2019 by Bryan Biedenkapp <gatekeep@gmail.com>
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
#include "RemoteCommand.h"
#include "edac/SHA256.h"
#include "network/UDPSocket.h"
#include "Thread.h"
#include "Log.h"
#include "Utils.h"

using namespace network;

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#undef __PROG_NAME__
#define __PROG_NAME__ "Digital Voice Modem (DVM) RCON Tool"
#undef __EXE_NAME__
#define __EXE_NAME__ "dvmcmd"

#define ERRNO_REMOTE_CMD 99

// ---------------------------------------------------------------------------
//	Macros
// ---------------------------------------------------------------------------

#define IS(s) (::strcmp(argv[i], s) == 0)

// ---------------------------------------------------------------------------
//  Global Variables
// ---------------------------------------------------------------------------

static std::string g_progExe = std::string(__EXE_NAME__);
static std::string g_remoteAddress = std::string("127.0.0.1");
static uint32_t g_remotePort = RCON_DEFAULT_PORT;
static std::string g_remotePassword = std::string();
static bool g_debug = false;

// ---------------------------------------------------------------------------
//	Global Functions
// ---------------------------------------------------------------------------

void fatal(const char* message)
{
    ::fprintf(stderr, "%s: %s\n", g_progExe.c_str(), message);
    exit(EXIT_FAILURE);
}

void usage(const char* message, const char* arg)
{
    ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
    ::fprintf(stdout, "Copyright (c) 2017-2022 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\n");
    ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\n\n");
    if (message != nullptr) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, "usage: %s [-v] [-a <address>] [-p <port>] [-P <password>] <command>\n\n"
        "  -a       remote modem command address\n"
        "  -p       remote modem command port\n"
        "  -P       remote modem authentication password\n"
        "\n"
        "  -d       enable debug\n"
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
            g_remotePort = (uint32_t)::atoi(argv[++i]);

            if (g_remotePort == 0)
                usage("error: %s", "remote port number cannot be blank or 0!");

            p += 2;
        }
        else if (IS("-P")) {
            if ((argc - 1) <= 0)
                usage("error: %s", "must specify the auth password");
            g_remotePassword = std::string(argv[++i]);

            if (g_remotePassword == "")
                usage("error: %s", "remote auth password cannot be blank!");

            p += 2;
        }
        else if (IS("-d")) {
            ++p;
            g_debug = true;
        }
        else if (IS("-v")) {
            ::fprintf(stdout, __PROG_NAME__ " %s (built %s)\r\n", __VER__, __BUILD__);
            ::fprintf(stdout, "Copyright (c) 2017-2022 Bryan Biedenkapp, N2PLL and DVMProject (https://github.com/dvmproject) Authors.\r\n");
            ::fprintf(stdout, "Portions Copyright (c) 2015-2021 by Jonathan Naylor, G4KLX and others\r\n");
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

int main(int argc, char** argv)
{
    if (argv[0] != nullptr && *argv[0] != 0)
        g_progExe = std::string(argv[0]);

    if (argc < 2) {
        usage("error: %s", "must specify the remote command!");
        return ERRNO_REMOTE_CMD;
    }

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

    // process command
    std::string cmd = std::string(argv[0]);
    for (int i = 1; i < argc; i++) {
        cmd += " ";
        cmd += std::string(argv[i]);
    }

    // initialize system logging
    bool ret = ::LogInitialise("", "", 0U, 1U, true);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    RemoteCommand* command = new RemoteCommand(g_remoteAddress, g_remotePort, g_remotePassword, g_debug);
    int retCode = command->send(cmd);

    ::LogFinalise();
    return retCode;
}
