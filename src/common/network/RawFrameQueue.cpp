/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
/*
*   Copyright (C) 2024 by Bryan Biedenkapp N2PLL
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
#include "network/RawFrameQueue.h"
#include "network/UDPSocket.h"
#include "Log.h"
#include "Utils.h"

using namespace network;

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the RawFrameQueue class.
/// </summary>
/// <param name="socket">Local port used to listen for incoming data.</param>
/// <param name="debug"></param>
RawFrameQueue::RawFrameQueue(UDPSocket* socket, bool debug) :
    m_socket(socket),
    m_buffers(),
    m_debug(debug)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the RawFrameQueue class.
/// </summary>
RawFrameQueue::~RawFrameQueue()
{
    /* stub */
}

/// <summary>
/// Read message from the received UDP packet.
/// </summary>
/// <param name="messageLength">Actual length of message read from packet.</param>
/// <param name="address">IP address data read from.</param>
/// <param name="addrLen"></param>
/// <returns>Buffer containing message read.</returns>
UInt8Array RawFrameQueue::read(int& messageLength, sockaddr_storage& address, uint32_t& addrLen)
{
    messageLength = -1;

    // read message from socket
    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);
    int length = m_socket->read(buffer, DATA_PACKET_LENGTH, address, addrLen);
    if (length < 0) {
        LogError(LOG_NET, "Failed reading data from the network");
        return nullptr;
    }

    if (length > 0) {
        if (m_debug)
            Utils::dump(1U, "Network Packet", buffer, length);

        // copy message
        messageLength = length;
        UInt8Array message = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
        ::memcpy(message.get(), buffer, length);

        return message;
    }

    return nullptr;
}

/// <summary>
/// Cache "message" to frame queue.
/// </summary>
/// <param name="message">Message buffer to frame and queue.</param>
/// <param name="length">Length of message.</param>
/// <param name="addr">IP address to write data to.</param>
/// <param name="addrLen"></param>
/// <returns></returns>
void RawFrameQueue::enqueueMessage(const uint8_t* message, uint32_t length, sockaddr_storage& addr, uint32_t addrLen)
{
    assert(message != nullptr);
    assert(length > 0U);

    uint8_t* buffer = new uint8_t[length];
    ::memset(buffer, 0x00U, length);
    ::memcpy(buffer, message, length);

    if (m_debug)
        Utils::dump(1U, "RawFrameQueue::enqueueMessage() Buffered Message", buffer, length);

    UDPDatagram* dgram = new UDPDatagram;
    dgram->buffer = buffer;
    dgram->length = length;
    dgram->address = addr;
    dgram->addrLen = addrLen;

    m_buffers.push_back(dgram);
}

/// <summary>
/// Flush the message queue.
/// </summary>
/// <returns></returns>
bool RawFrameQueue::flushQueue()
{
    if (m_buffers.empty()) {
        return false;
    }

    // bryanb: this is the same as above -- but for some assinine reason prevents
    // weirdness
    if (m_buffers.size() == 0U) {
        return false;
    }

    // LogDebug(LOG_NET, "m_buffers len = %u", m_buffers.size());

    bool ret = true;
    if (!m_socket->write(m_buffers)) {
        LogError(LOG_NET, "Failed writing data to the network");
        ret = false;
    }

    for (auto& buffer : m_buffers) {
        if (buffer != nullptr) {
            // LogDebug(LOG_NET, "deleting buffer, addr %p len %u", buffer->buffer, buffer->length);
            if (buffer->buffer != nullptr) {
                delete buffer->buffer;
                buffer->length = 0;
                buffer->buffer = nullptr;
            }

            delete buffer;
            buffer = nullptr;
        }
    }
    m_buffers.clear();

    return ret;
}
