// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "edac/CRC.h"
#include "network/BaseNetwork.h"
#include "network/FrameQueue.h"
#include "network/RTPHeader.h"
#include "network/RTPExtensionHeader.h"
#include "network/RTPFNEHeader.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::frame;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the FrameQueue class.
/// </FrameQueue>
/// <param name="socket">Local port used to listen for incoming data.</param>
/// <param name="peerId">Unique ID of this modem on the network.</param>
FrameQueue::FrameQueue(UDPSocket* socket, uint32_t peerId, bool debug) : RawFrameQueue(socket, debug),
    m_peerId(peerId),
    m_streamTimestamps()
{
    assert(peerId < 999999999U);
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
    RTPHeader _rtpHeader = RTPHeader();
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
        if ((_rtpHeader.getPayloadType() != DVM_RTP_PAYLOAD_TYPE) &&
            (_rtpHeader.getPayloadType() != DVM_CTRL_RTP_PAYLOAD_TYPE)) {
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

        // copy message
        messageLength = _fneHeader.getMessageLength();
        UInt8Array message = std::unique_ptr<uint8_t[]>(new uint8_t[messageLength]);
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
/// <param name="rtpSeq">RTP Sequence.</param>
/// <param name="addr">IP address to write data to.</param>
/// <param name="addrLen"></param>
/// <returns></returns>
void FrameQueue::enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
    OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen)
{
    enqueueMessage(message, length, streamId, peerId, peerId, opcode, rtpSeq, addr, addrLen);
}

/// <summary>
/// Cache "message" to frame queue.
/// </summary>
/// <param name="message">Message buffer to frame and queue.</param>
/// <param name="length">Length of message.</param>
/// <param name="streamId">Message stream ID.</param>
/// <param name="peerId">Peer ID.</param>
/// <param name="ssrc">RTP SSRC ID.</param>
/// <param name="opcode">Opcode.</param>
/// <param name="rtpSeq">RTP Sequence.</param>
/// <param name="addr">IP address to write data to.</param>
/// <param name="addrLen"></param>
/// <returns></returns>
void FrameQueue::enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
    uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen)
{
    assert(message != nullptr);
    assert(length > 0U);

    uint32_t timestamp = INVALID_TS;
    if (streamId != 0U) {
        auto entry = m_streamTimestamps.find(streamId);
        if (entry != m_streamTimestamps.end()) {
            timestamp = entry->second;
        }

        if (timestamp != INVALID_TS) {
            timestamp += (RTP_GENERIC_CLOCK_RATE / 133);
            if (m_debug)
                LogDebug(LOG_NET, "FrameQueue::enqueueMessage() RTP streamId = %u, previous TS = %u, TS = %u, rtpSeq = %u", streamId, m_streamTimestamps[streamId], timestamp, rtpSeq);
            m_streamTimestamps[streamId] = timestamp;
        }
    }

    uint32_t bufferLen = RTP_HEADER_LENGTH_BYTES + RTP_EXTENSION_HEADER_LENGTH_BYTES + RTP_FNE_HEADER_LENGTH_BYTES + length;
    uint8_t* buffer = new uint8_t[bufferLen];
    ::memset(buffer, 0x00U, bufferLen);

    RTPHeader header = RTPHeader();
    header.setExtension(true);

    header.setPayloadType(DVM_RTP_PAYLOAD_TYPE);
    header.setTimestamp(timestamp);
    header.setSequence(rtpSeq);
    header.setSSRC(ssrc);

    // properly flag control opcodes
    if ((opcode.first == NET_FUNC_TRANSFER) || (opcode.first == NET_FUNC_GRANT_REQ)) {
        header.setPayloadType(DVM_CTRL_RTP_PAYLOAD_TYPE);
        header.setSequence(0U);
    }

    header.encode(buffer);

    if (streamId != 0U && timestamp == INVALID_TS && rtpSeq != RTP_END_OF_CALL_SEQ) {
        if (m_debug)
            LogDebug(LOG_NET, "FrameQueue::enqueueMessage() RTP streamId = %u, initial TS = %u, rtpSeq = %u", streamId, header.getTimestamp(), rtpSeq);
        m_streamTimestamps[streamId] = header.getTimestamp();
    }

    if (streamId != 0U && rtpSeq == RTP_END_OF_CALL_SEQ) {
        auto entry = m_streamTimestamps.find(streamId);
        if (entry != m_streamTimestamps.end()) {
            if (m_debug)
                LogDebug(LOG_NET, "FrameQueue::enqueueMessage() RTP streamId = %u, rtpSeq = %u", streamId, rtpSeq);
            m_streamTimestamps.erase(streamId);
        }
    }

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

    UDPDatagram *dgram = new UDPDatagram;
    dgram->buffer = buffer;
    dgram->length = bufferLen;
    dgram->address = addr;
    dgram->addrLen = addrLen;

    m_buffers.push_back(dgram);
}

/// <summary>
/// Helper method to clear any tracked stream timestamps.
/// </summary>
void FrameQueue::clearTimestamps()
{
    m_streamTimestamps.clear();
}
