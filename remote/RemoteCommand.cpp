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

#define ERRNO_SOCK_OPEN 98
#define ERRNO_ADDR_LOOKUP 97
#define ERRNO_FAILED_TO_SEND 96

const uint8_t RCON_FRAME_START = 0xFEU;
const uint8_t START_OF_TEXT = 0x02U;
const uint8_t END_OF_TEXT = 0x03U;
const uint8_t END_OF_BLOCK = 0x17U;
const uint8_t REC_SEPARATOR = 0x1EU;

const uint32_t RC_BUFFER_LENGTH = 250U;
const uint32_t RESPONSE_BUFFER_LEN = 4095U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the RemoteCommand class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="password">Authentication password.</param>
/// <param name="debug">Flag indicating whether debug is enabled.</param>
RemoteCommand::RemoteCommand(const std::string& address, uint32_t port, const std::string& password, bool debug) :
    m_address(address),
    m_port(port),
    m_password(password),
    m_debug(debug)
{
    assert(!address.empty());
    assert(port > 0U);
}

/// <summary>
/// Finalizes a instance of the RemoteCommand class.
/// </summary>
RemoteCommand::~RemoteCommand()
{
    /* stub */
}

/// <summary>
/// Sends remote control command to the specified modem.
/// </summary>
/// <param name="command">Command string to send to remote modem.</param>
/// <returns>EXIT_SUCCESS, if command was sent, otherwise EXIT_FAILURE.</returns>
int RemoteCommand::send(const std::string& command)
{
    UDPSocket socket(0U);

    bool ret = socket.open();
    if (!ret)
        return ERRNO_SOCK_OPEN;

    uint8_t buffer[RC_BUFFER_LENGTH];
    ::memset(buffer, 0x00U, RC_BUFFER_LENGTH);

    sockaddr_storage addr;
    uint32_t addrLen;

    if (UDPSocket::lookup(m_address, m_port, addr, addrLen) != 0) {
        ::LogError(LOG_HOST, "Could not lookup the address of remote");
        return ERRNO_ADDR_LOOKUP;
    }

    ::LogInfoEx(LOG_HOST, "sending RCON command \"%s\" to %s:%u", command.c_str(), m_address.c_str(), m_port);

    buffer[0U] = RCON_FRAME_START;
    buffer[1U] = START_OF_TEXT;

    if (!m_password.empty()) {
        size_t size = m_password.size();

        uint8_t* in = new uint8_t[size];
        for (size_t i = 0U; i < size; i++)
            in[i] = m_password.at(i);

        uint8_t out[32U];
        ::memset(out, 0x00U, 32U);

        edac::SHA256 sha256;
        sha256.buffer(in, (uint32_t)(size), out);

        ::memcpy(buffer + 2U, out, 32U);
    }

    buffer[34U] = REC_SEPARATOR;
    ::memcpy(buffer + 35U, command.c_str(), command.size());

    buffer[35U + command.size()] = END_OF_TEXT;

    if (m_debug)
        Utils::dump(1U, "RCON Sent", (uint8_t*)buffer, 36U + command.size());

    ret = socket.write((uint8_t *)buffer, 36U + command.size(), addr, addrLen);
    if (!ret) {
        socket.close();
        ::LogError(LOG_HOST, "Failed to send command: \"%s\"", command.c_str());
        return ERRNO_FAILED_TO_SEND;
    }

    Thread::sleep(100);

    uint8_t response[RESPONSE_BUFFER_LEN];
    ::memset(response, 0x00U, RESPONSE_BUFFER_LEN);

    int len = 1;
    uint32_t offs = 0U;
    do
    {
        ::memset(buffer, 0x00U, RC_BUFFER_LENGTH);
        int len = socket.read((uint8_t *)buffer, RC_BUFFER_LENGTH, addr, addrLen);
        if (len <= 1)
            break;

        if (offs + len > RESPONSE_BUFFER_LEN)
            break;

        if (m_debug)
            ::LogDebug(LOG_RCON, "RemoteCommand::send() block len = %u, offs = %u", len - 3, offs);

        buffer[len] = '\0';

        if (m_debug)
            Utils::dump(1U, "RCON Received", (uint8_t*)buffer, len);

        // make sure this is an RCON response
        if (buffer[0U] != RCON_FRAME_START) {
            ::LogError(LOG_HOST, "Invalid response from host %s:%u", m_address.c_str(), m_port);
            socket.close();
            return EXIT_FAILURE;
        }

        if (buffer[1U] != START_OF_TEXT) {
            ::LogError(LOG_HOST, "Invalid response from host %s:%u", m_address.c_str(), m_port);
            socket.close();
            return EXIT_FAILURE;
        }

        ::memcpy(response + offs, buffer + 2U, len - 3U);
        offs += len - 3U;

        Thread::sleep(100);
    } while (buffer[len - 1U] != END_OF_TEXT);

    ::LogInfoEx(LOG_HOST, ">> %s", response);

    socket.close();
    return EXIT_SUCCESS;
}
