/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#include "edac/CRC.h"
#include "network/FrameQueue.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::frame;

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the FrameQueue class.
/// </FrameQueue>
/// <param name="socket">Local port used to listen for incoming data.</param>
/// <param name="id">Unique ID of this modem on the network.</param>
FrameQueue::FrameQueue(UDPSocket socket, uint32_t id) :
    m_id(id),
    m_socket(socket),
    m_buffers()
{
    assert(id > 1000U);
}

/// <summary>
/// Finalizes a instance of the FrameQueue class.
/// </summary>
FrameQueue::~FrameQueue()
{
    /* stub */
}

/// <summary>
/// Read message from the UDP socket.
/// </summary>
/// <param name="message">Buffer to read message into.</param>
/// <param name="address">IP address to read message from.</param>
/// <param name="addrLen"></param>
/// <param name="rtpHeader">RTP Header.</param>
/// <param name="fneHeader">FNE Header.</param>
/// <returns>Actual length of message read from remote UDP socket.</returns>
int FrameQueue::read(uint8_t* message, sockaddr_storage& address, uint32_t& addrLen,
    RTPHeader* rtpHeader, RTPFNEHeader* fneHeader)
{
    assert(message != nullptr);

    uint8_t buffer[DATA_PACKET_LENGTH];
    ::memset(buffer, 0x00U, DATA_PACKET_LENGTH);

    RTPHeader _rtpHeader = RTPHeader(true);
    RTPFNEHeader _fneHeader = RTPFNEHeader();

    int length = m_socket.read(buffer, DATA_PACKET_LENGTH, address, addrLen);
    if (length > 0) {
        if (length < RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES) {
            LogError(LOG_NET, "FrameQueue::read(), message received from network is malformed! %u bytes != %u bytes", 
                RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES, length);
            return -1;
        }

        // decode RTP header
        if (!_rtpHeader.decode(buffer)) {
            LogError(LOG_NET, "FrameQueue::read(), invalid RTP packet received from network");
            return -1;
        }

        // ensure the RTP header has extension header (otherwise abort)
        if (!_rtpHeader.getExtension()) {
            LogError(LOG_NET, "FrameQueue::read(), invalid RTP header received from network");
            return -1;
        }

        // ensure payload type is correct
        if (_rtpHeader.getPayloadType() != DVM_RTP_PAYLOAD_TYPE) {
            LogError(LOG_NET, "FrameQueue::read(), invalid RTP payload type received from network");
            return -1;
        }

        if (rtpHeader != nullptr) {
            *rtpHeader = _rtpHeader;
        }

        // decode FNE RTP header
        if (!_fneHeader.decode(buffer + RTP_HEADER_LENGTH_BYTES)) {
            LogError(LOG_NET, "FrameQueue::read(), invalid RTP packet received from network");
            return -1;
        }

        if (fneHeader != nullptr) {
            *fneHeader = _fneHeader;
        }

        // copy message
        uint32_t messageLen = _fneHeader.getMessageLength();
        message = new uint8_t[messageLen];
        ::memset(message, 0x00U, messageLen);
        ::memcpy(message, buffer + (RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES), messageLen);

        uint16_t calc = edac::CRC::createCRC16(message, messageLen * 8U);
        if (calc != _fneHeader.getCRC()) {
            LogError(LOG_NET, "FrameQueue::read(), failed CRC CCITT-162 check");
            delete[] message;
            return -1;
        }

        return messageLen;
    }

    return -1;
}

/// <summary>
/// Cache "message" to frame queue.
/// </summary>
/// <param name="message">Message buffer to frame and queue.</param>
/// <param name="length">Length of message.</param>
/// <param name="streamId"></param>
/// <param name="peerId"></param>
/// <returns></returns>
void FrameQueue::enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId)
{
    assert(message != nullptr);
    assert(length > 0U);

    uint32_t bufferLen = RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES + length;
    uint8_t buffer[bufferLen];
    ::memset(buffer, 0x00U, bufferLen);

    RTPHeader header = RTPHeader();
    header.setExtension(true);
    header.setPayloadType(DVM_RTP_PAYLOAD_TYPE);
    header.setSSRC(streamId);

    header.encode(buffer);

    RTPFNEHeader fneHeader = RTPFNEHeader();
    fneHeader.setCRC(edac::CRC::createCRC16(message, length * 8U));
    fneHeader.setStreamId(streamId);
    fneHeader.setPeerId(peerId);
    fneHeader.setMessageLength(length);

    fneHeader.encode(buffer + RTP_HEADER_LENGTH_BYTES);

    ::memcpy(buffer + RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES, message, length);

    m_buffers.push_back({ bufferLen, buffer });
}

/// <summary>
/// Cache all messages in "buffers" in order to frame queue.
/// </summary>
/// <param name="buffers">List of buffers to enqueue.</param>
/// <param name="streamId"></param>
/// <param name="peerId"></param>
/// <returns></returns>
bool FrameQueue::enqueueMessage(BufferVector& buffers, uint32_t streamId, uint32_t peerId)
{
    if (!buffers.size()) {
        return false;
    }

    for (auto& buffer : buffers) {
        uint32_t length = buffer.first;
        uint32_t bufferLen = RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES + length;
        uint8_t _buffer[bufferLen];
        ::memset(_buffer, 0x00U, bufferLen);

        RTPHeader header = RTPHeader();
        header.setExtension(true);
        header.setPayloadType(DVM_RTP_PAYLOAD_TYPE);
        header.setSSRC(streamId);

        header.encode(_buffer);

        RTPFNEHeader fneHeader = RTPFNEHeader();
        fneHeader.setCRC(edac::CRC::createCRC16(buffer.second, length * 8U));
        fneHeader.setStreamId(streamId);
        fneHeader.setPeerId(peerId);
        fneHeader.setMessageLength(length);

        fneHeader.encode(_buffer + RTP_HEADER_LENGTH_BYTES);

        ::memcpy(_buffer + RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES, buffer.second, length);

        m_buffers.push_back({ bufferLen, _buffer });
    }

    return true;
}

/// <summary>
/// Flush the message queue.
/// </summary>
/// <returns></returns>
bool FrameQueue::flushQueue(sockaddr_storage& addr, uint32_t addrLen)
{
    if (m_buffers.empty()) {
        return false;
    }

    if (m_socket.write(m_buffers, addr, addrLen)) {
        LogError(LOG_NET, "Socket has failed when writing data to the network!");
        return false;
    }

    for (auto& buffer : m_buffers) {
        if (buffer.second != nullptr) {
            delete buffer.second;
        }
    }
    m_buffers.clear();

    return true;
}
