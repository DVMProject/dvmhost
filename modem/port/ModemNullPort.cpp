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
*   Copyright (C) 2021 by Jonathan Naylor G4KLX
*   Copyright (C) 2021 by Bryan Biedenkapp N2PLL
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

#include "modem/port/ModemNullPort.h"
#include "modem/Modem.h"

using namespace modem::port;
using namespace modem;

#include <cstdio>
#include <cassert>

const char* HARDWARE = "Null Modem Controller";

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the ModemNullPort class.
/// </summary>
ModemNullPort::ModemNullPort() :
    m_buffer(200U, "Null Controller Buffer")
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the ModemNullPort class.
/// </summary>
ModemNullPort::~ModemNullPort()
{
}

/// <summary>
/// Opens a connection to the port.
/// </summary>
/// <returns>True, if connection is opened, otherwise false.</returns>
bool ModemNullPort::open()
{
    return true;
}

/// <summary>
/// Reads data from the port.
/// </summary>
/// <param name="buffer">Buffer to read data from the serial port to.</param>
/// <param name="length">Length of data to read from the serial port.</param>
/// <returns>Actual length of data read from serial port.</returns>
int ModemNullPort::read(uint8_t* buffer, uint32_t length)
{
    uint32_t dataSize = m_buffer.dataSize();
    if (dataSize == 0U)
        return 0;

    if (length > dataSize)
        length = dataSize;

    m_buffer.getData(buffer, length);

    return int(length);
}

/// <summary>
/// Writes data to the serial port.
/// </summary>
/// <param name="buffer">Buffer containing data to write to serial port.</param>
/// <param name="length">Length of data to write to serial port.</param>
/// <returns>Actual length of data written to the serial port.</returns>
int ModemNullPort::write(const uint8_t* buffer, uint32_t length)
{
    switch (buffer[2U]) {
    case CMD_GET_VERSION:
        getVersion();
        break;
    case CMD_GET_STATUS:
        getStatus();
        break;
    case CMD_SET_CONFIG:
    case CMD_SET_MODE:
        writeAck(buffer[2U]);
        break;
    default:
        break;
    }

    return int(length);
}

/// <summary>
/// Closes the connection to the port.
/// </summary>
void ModemNullPort::close()
{
    /* stub */
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
///
/// </summary>
void ModemNullPort::getVersion()
{
    unsigned char reply[200U];

    reply[0U] = DVM_FRAME_START;
    reply[1U] = 0U;
    reply[2U] = CMD_GET_VERSION;

    reply[3U] = PROTOCOL_VERSION;
    reply[4U] = 15U;

    // Reserve 16 bytes for the UDID
    ::memset(reply + 5U, 0x00U, 16U);

    uint8_t count = 21U;
    for (uint8_t i = 0U; HARDWARE[i] != 0x00U; i++, count++)
        reply[count] = HARDWARE[i];

    reply[1U] = count;

    m_buffer.addData(reply, count);
}

/// <summary>
///
/// </summary>
void ModemNullPort::getStatus()
{
    unsigned char reply[15U];

    // Send all sorts of interesting internal values
    reply[0U] = DVM_FRAME_START;
    reply[1U] = 11U;
    reply[2U] = CMD_GET_STATUS;

    reply[3U] = 0U;

    reply[4U] = 0x00U;

    reply[5U] = 0x00U;

    reply[6U] = 20U;

    reply[7U] = 20U;
    reply[8U] = 20U;

    reply[9U] = 0U;

    reply[10U] = 20U;

    m_buffer.addData(reply, 11U);
}

/// <summary>
///
/// </summary>
void ModemNullPort::writeAck(uint8_t type)
{
    unsigned char reply[4U];

    reply[0U] = DVM_FRAME_START;
    reply[1U] = 4U;
    reply[2U] = CMD_ACK;
    reply[3U] = type;

    m_buffer.addData(reply, 4U);
}
