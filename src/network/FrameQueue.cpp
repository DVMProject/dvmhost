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
/// <param name="peerId">Unique ID of this modem on the network.</param>
FrameQueue::FrameQueue(UDPSocket* socket, uint32_t peerId, bool debug) :
    m_peerId(peerId),
    m_socket(socket),
    m_buffers(),
    m_debug(debug)
{
    assert(peerId > 1000U);
}

/// <summary>
/// Finalizes a instance of the FrameQueue class.
/// </summary>
FrameQueue::~FrameQueue()
{
    /* stub */
}

/// <summary>
/// Read message from the received UDP packet.
/// </summary>
/// <param name="messageLength">Actual length of message read from packet.</param>
/// <param name="address">IP address data read from.</param>
/// <param name="addrLen"></param>
/// <param name="rtpHeader">RTP Header.</param>
/// <param name="fneHeader">FNE Header.</param>
/// <returns>Buffer containing message read.</returns>
UInt8Array FrameQueue::read(int& messageLength, sockaddr_storage& address, uint32_t& addrLen,
    RTPHeader* rtpHeader, RTPFNEHeader* fneHeader)
{
    RTPHeader _rtpHeader = RTPHeader(true);
    RTPFNEHeader _fneHeader = RTPFNEHeader();

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

        if (length < RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES) {
            LogError(LOG_NET, "FrameQueue::read(), message received from network is malformed! %u bytes != %u bytes", 
                RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES, length);
            return nullptr;
        }

        // decode RTP header
        if (!_rtpHeader.decode(buffer)) {
            LogError(LOG_NET, "FrameQueue::read(), invalid RTP packet received from network");
            return nullptr;
        }

        // ensure the RTP header has extension header (otherwise abort)
        if (!_rtpHeader.getExtension()) {
            LogError(LOG_NET, "FrameQueue::read(), invalid RTP header received from network");
            return nullptr;
        }

        // ensure payload type is correct
        if (_rtpHeader.getPayloadType() != DVM_RTP_PAYLOAD_TYPE) {
            LogError(LOG_NET, "FrameQueue::read(), invalid RTP payload type received from network");
            return nullptr;
        }

        if (rtpHeader != nullptr) {
            *rtpHeader = _rtpHeader;
        }

        // decode FNE RTP header
        if (!_fneHeader.decode(buffer + RTP_HEADER_LENGTH_BYTES)) {
            LogError(LOG_NET, "FrameQueue::read(), invalid RTP packet received from network");
            return nullptr;
        }

        if (fneHeader != nullptr) {
            *fneHeader = _fneHeader;
        }

        // ensure the RTP synchronization source ID matches the FNE stream ID
        if (_rtpHeader.getSSRC() != _fneHeader.getStreamId()) {
            LogWarning(LOG_NET, "FrameQueue::read(), RTP header and FNE header do not agree on stream ID? %u != %u", _rtpHeader.getSSRC(), _fneHeader.getStreamId());
        }

        // copy message
        messageLength = _fneHeader.getMessageLength();
        __UNIQUE_UINT8_ARRAY(message, messageLength);
        ::memcpy(message.get(), buffer + (RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES), messageLength);

        uint16_t calc = edac::CRC::createCRC16(message.get(), messageLength * 8U);
        if (calc != _fneHeader.getCRC()) {
            LogError(LOG_NET, "FrameQueue::read(), failed CRC CCITT-162 check");
            messageLength = -1;
            return nullptr;
        }

        // LogDebug(LOG_NET, "message buffer, addr %p len %u", message.get(), messageLength);
        return message;
    }

    return nullptr;
}

/// <summary>
/// Cache "message" to frame queue.
/// </summary>
/// <param name="message">Message buffer to frame and queue.</param>
/// <param name="length">Length of message.</param>
/// <param name="streamId">Message stream ID.</param>
/// <param name="peerId">Peer ID.</param>
/// <param name="opcode">Opcode.</param>
/// <param name="addr">IP address to write data to.</param>
/// <param name="addrLen"></param>
/// <returns></returns>
void FrameQueue::enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
    OpcodePair opcode, sockaddr_storage& addr, uint32_t addrLen)
{
    assert(message != nullptr);
    assert(length > 0U);

    uint32_t bufferLen = RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES + length;
    uint8_t* buffer = new uint8_t[bufferLen];
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

    fneHeader.setFunction(opcode.first);
    fneHeader.setSubFunction(opcode.second);

    fneHeader.encode(buffer + RTP_HEADER_LENGTH_BYTES);

    ::memcpy(buffer + RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES, message, length);

    if (m_debug)
        Utils::dump(1U, "FrameQueue::enqueueMessage() Buffered Message", buffer, bufferLen);

    UDPDatagram* dgram = new UDPDatagram;
    dgram->buffer = buffer;
    dgram->length = bufferLen;
    dgram->address = addr;
    dgram->addrLen = addrLen;

    m_buffers.push_back(dgram);
}

/// <summary>
/// Flush the message queue.
/// </summary>
/// <returns></returns>
bool FrameQueue::flushQueue()
{
    if (m_buffers.empty()) {
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
