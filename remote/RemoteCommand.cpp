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
#include "Defines.h"
#include "edac/SHA256.h"
#include "network/UDPSocket.h"
#include "RemoteCommand.h"
#include "Log.h"

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

const uint32_t START_OF_TEXT = 0x02;
const uint32_t REC_SEPARATOR = 0x1E;

const uint32_t RC_BUFFER_LENGTH = 140U;

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
    if (message != NULL) {
        ::fprintf(stderr, "%s: ", g_progExe.c_str());
        ::fprintf(stderr, message, arg);
        ::fprintf(stderr, "\n\n");
    }

    ::fprintf(stdout, "usage: %s [-v] [-a <address>] [-p <port>] [-P <password>] <command>\n\n"
        "  -a       remote modem command address\n"
        "  -p       remote modem command port\n"
        "  -P       remote modem authentication password\n"
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

    if (argc < 2) {
        usage("error: %s", "must specify the remote command!");
        return EXIT_FAILURE;
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
    bool ret = ::LogInitialise("", "", 0U, 1U);
    if (!ret) {
        ::fprintf(stderr, "unable to open the log file\n");
        return 1;
    }

    CRemoteCommand* command = new CRemoteCommand(g_remoteAddress, g_remotePort, g_remotePassword);
    int retCode = command->send(cmd);

    ::LogFinalise();
    return retCode;
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the CRemoteCommand class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="password">Authentication password.</param>
CRemoteCommand::CRemoteCommand(const std::string& address, uint32_t port, const std::string& password) :
    m_address(address),
    m_port(port),
    m_password(password)
{
    assert(!address.empty());
    assert(port > 0U);
}

/// <summary>
/// Finalizes a instance of the CRemoteCommand class.
/// </summary>
CRemoteCommand::~CRemoteCommand()
{
    /* stub */
}

/// <summary>
/// Sends remote control command to the specified modem.
/// </summary>
/// <param name="command">Command string to send to remote modem.</param>
/// <returns>EXIT_SUCCESS, if command was sent, otherwise EXIT_FAILURE.</returns>
int CRemoteCommand::send(const std::string& command)
{
    UDPSocket socket(0U);

    bool ret = socket.open();
    if (!ret)
        return EXIT_FAILURE;

    uint8_t buffer[RC_BUFFER_LENGTH];
    ::memset(buffer, 0x00U, RC_BUFFER_LENGTH);

    sockaddr_storage addr;
    uint32_t addrLen;

    if (UDPSocket::lookup(m_address, m_port, addr, addrLen) != 0)
        addrLen = 0U;

    if (addrLen > 0U) {
        return EXIT_FAILURE;
    }

    ::LogInfoEx(LOG_HOST, "%s: sending command \"%s\" to %s:%u\r\n", g_progExe.c_str(), command.c_str(),
        m_address.c_str(), m_port);

    buffer[0U] = START_OF_TEXT;

    if (!m_password.empty()) {
        size_t size = m_password.size();

        uint8_t* in = new uint8_t[size];
        for (size_t i = 0U; i < size; i++)
            in[i] = m_password.at(i);

        uint8_t out[32U];
        ::memset(out, 0x00U, 32U);

        edac::SHA256 sha256;
        sha256.buffer(in, (uint32_t)(size), out);

        ::memcpy(buffer + 1U, out, 32U);
    }

    buffer[33U] = REC_SEPARATOR;
    ::memcpy(buffer + 34U, command.c_str(), command.size());

    ret = socket.write((uint8_t *)buffer, 34U + command.size(), addr, m_port);
    if (!ret) {
        socket.close();
        ::LogError(LOG_HOST, "Failed to send command: \"%s\"\r\n", command.c_str());
        return EXIT_FAILURE;
    }

    socket.close();
    return EXIT_SUCCESS;
}
