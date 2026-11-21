// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2021 Jonathan Naylor, G4KLX
 *  Copyright (C) 2021,2024,2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "modem/port/ModemNullPort.h"
#include "modem/Modem.h"

using namespace modem::port;
using namespace modem;

const char* HARDWARE = "Null Modem Controller";

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ModemNullPort class. */

ModemNullPort::ModemNullPort() :
    m_buffer(200U, "Null Controller Buffer")
{
    /* stub */
}

/* Finalizes a instance of the ModemNullPort class. */

ModemNullPort::~ModemNullPort() = default;

/* Opens a connection to the port. */

bool ModemNullPort::open()
{
    m_buffer.clear();
    return true;
}

/* Reads data from the port. */

int ModemNullPort::read(uint8_t* buffer, uint32_t length)
{
    uint32_t dataSize = m_buffer.dataSize();
    if (dataSize == 0U)
        return 0;

    if (length > dataSize)
        length = dataSize;

    m_buffer.get(buffer, length);

    return int(length);
}

/* Writes data to the serial port. */

int ModemNullPort::write(const uint8_t* buffer, uint32_t length)
{
    if (buffer == nullptr || length < 3U)
        return 0;

    uint32_t commandOffset = 2U;
    if (buffer[0U] == DVM_SHORT_FRAME_START) {
        if (buffer[1U] != length)
            return 0;
    }
    else if (buffer[0U] == DVM_LONG_FRAME_START) {
        if (length < 4U || ((uint32_t(buffer[1U]) << 8) | buffer[2U]) != length || buffer[3U] != CMD_P25_DATA)
            return 0;
        commandOffset = 3U;
    }
    else {
        return 0;
    }
    
    switch (buffer[commandOffset]) {
    case CMD_GET_VERSION:
        if (length != 3U)
            return 0;
        getVersion();
        break;
    case CMD_GET_STATUS:
        if (length != 3U)
            return 0;
        getStatus();
        break;
    case CMD_SET_CONFIG:
        if (length != 24U)
            return 0;
        writeAck(CMD_SET_CONFIG);
        break;
    case CMD_SET_MODE:
        if (length != 4U)
            return 0;
        writeAck(CMD_SET_MODE);
        break;
    case CMD_SET_BUFFERS:
        if (length != 9U) {
            writeNAK(CMD_SET_BUFFERS, RSN_ILLEGAL_LENGTH);
            break;
        }
        writeAck(CMD_SET_BUFFERS);
        break;
    case CMD_FLSH_READ:
        writeNAK(CMD_FLSH_READ, RSN_NO_INTERNAL_FLASH);
        break;
    default:
        break;
    }

    return int(length);
}

/* Closes the connection to the port. */

void ModemNullPort::close()
{
    m_buffer.clear();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to return a faked modem version. */

void ModemNullPort::getVersion()
{
    unsigned char reply[200U];

    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 0U;
    reply[2U] = CMD_GET_VERSION;

    reply[3U] = 4U;
    reply[4U] = 15U;

    // Reserve 16 bytes for the UDID
    ::memset(reply + 5U, 0x00U, 16U);

    uint8_t count = 21U;
    for (uint8_t i = 0U; HARDWARE[i] != 0x00U; i++, count++)
        reply[count] = HARDWARE[i];

    reply[1U] = count;

    m_buffer.addData(reply, count);
}

/* Helper to return a faked modem status. */

void ModemNullPort::getStatus()
{
    unsigned char reply[15U];

    // Send all sorts of interesting internal values
    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 12U;
    reply[2U] = CMD_GET_STATUS;

    reply[3U] = 0U;

    reply[4U] = 0x00U;

    reply[5U] = 0x00U;

    reply[6U] = 20U;

    reply[7U] = 20U;
    reply[8U] = 20U;

    reply[9U] = 0U;

    reply[10U] = 20U;
    reply[11U] = 20U;

    m_buffer.addData(reply, 12U);
}

/* Helper to write a faked modem acknowledge. */

void ModemNullPort::writeAck(uint8_t type)
{
    unsigned char reply[4U];

    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 4U;
    reply[2U] = CMD_ACK;
    reply[3U] = type;

    m_buffer.addData(reply, 4U);
}

/* Helper to write a faked modem negative acknowledge. */

void ModemNullPort::writeNAK(uint8_t opcode, uint8_t err)
{
    uint8_t reply[5U];

    reply[0U] = DVM_SHORT_FRAME_START;
    reply[1U] = 5U;
    reply[2U] = CMD_NAK;
    reply[3U] = opcode;
    reply[4U] = err;

    m_buffer.addData(reply, 5U);
}