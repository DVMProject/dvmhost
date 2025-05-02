// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024,2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "edac/CRC.h"
#include "network/BaseNetwork.h"
#include "network/FrameQueue.h"
#include "network/RTPHeader.h"
#include "network/RTPExtensionHeader.h"
#include "network/RTPFNEHeader.h"
#include "Clock.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::frame;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the FrameQueue class. */

FrameQueue::FrameQueue(udp::Socket* socket, uint32_t peerId, bool debug) : RawFrameQueue(socket, debug),
    m_peerId(peerId),
    m_streamTimestamps()
{
    assert(peerId < 999999999U);
}

/* Read message from the received UDP packet. */

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
        if (m_failedReadCnt <= MAX_FAILED_READ_CNT_LOGGING)
            LogError(LOG_NET, "Failed reading data from the network, failedCnt = %u", m_failedReadCnt);
        else {
            if (m_failedReadCnt == MAX_FAILED_READ_CNT_LOGGING + 1U)
                LogError(LOG_NET, "Failed reading data from the network -- exceeded 5 read errors, probable connection issue, silencing further errors");
        }
        m_failedReadCnt++;
        return nullptr;
    }

    if (length > 0) {
        if (m_debug)
            Utils::dump(1U, "Network Packet", buffer, length);

        m_failedReadCnt = 0U;

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
            (_rtpHeader.getPayloadType() != (DVM_RTP_PAYLOAD_TYPE + 1U))) {
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

        if (_fneHeader.getMessageLength() == 0U) {
            LogError(LOG_NET, "FrameQueue::read(), invalid FNE packet length received from network");
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

/* Write message to the UDP socket. */

bool FrameQueue::write(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
    uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen)
{
    if (message == nullptr) {
        LogError(LOG_NET, "FrameQueue::write(), message is null");
        return false;
    }
    if (length == 0U) {
        LogError(LOG_NET, "FrameQueue::write(), message length is zero");
        return false;
    }

    uint32_t bufferLen = 0U;
    uint8_t* buffer = generateMessage(message, length, streamId, peerId, ssrc, opcode, rtpSeq, &bufferLen);

    if (bufferLen > (DATA_PACKET_LENGTH / 2U)) {
        LogWarning(LOG_NET, "FrameQueue::write(), packet length is possibly oversized, possible data truncation");
    }

    bool ret = true;
    if (!m_socket->write(buffer, bufferLen, addr, addrLen)) {
        // LogError(LOG_NET, "Failed writing data to the network");
        ret = false;
    }

    delete[] buffer;
    return ret;
}

/* Cache message to frame queue. */

void FrameQueue::enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
    OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen)
{
    enqueueMessage(message, length, streamId, peerId, peerId, opcode, rtpSeq, addr, addrLen);
}

/* Cache message to frame queue. */

void FrameQueue::enqueueMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
    uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, sockaddr_storage& addr, uint32_t addrLen)
{
    if (message == nullptr) {
        LogError(LOG_NET, "FrameQueue::enqueueMessage(), message is null");
        return;
    }
    if (length == 0U) {
        LogError(LOG_NET, "FrameQueue::enqueueMessage(), message length is zero");
        return;
    }

    uint32_t bufferLen = 0U;
    uint8_t* buffer = generateMessage(message, length, streamId, peerId, ssrc, opcode, rtpSeq, &bufferLen);

    if (bufferLen > (DATA_PACKET_LENGTH / 2U)) {
        LogWarning(LOG_NET, "FrameQueue::enqueueMessage(), packet length is possibly oversized, possible data truncation");
    }

    udp::UDPDatagram *dgram = new udp::UDPDatagram;
    dgram->buffer = buffer;
    dgram->length = bufferLen;
    dgram->address = addr;
    dgram->addrLen = addrLen;

    m_buffers.push_back(dgram);
}

/* Helper method to clear any tracked stream timestamps. */

void FrameQueue::clearTimestamps()
{
    m_streamTimestamps.clear();
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Generate RTP message for the frame queue. */

uint8_t* FrameQueue::generateMessage(const uint8_t* message, uint32_t length, uint32_t streamId, uint32_t peerId,
    uint32_t ssrc, OpcodePair opcode, uint16_t rtpSeq, uint32_t* outBufferLen)
{
    if (message == nullptr) {
        LogError(LOG_NET, "FrameQueue::generateMessage(), message is null");
        return nullptr;
    }
    if (length == 0U) {
        LogError(LOG_NET, "FrameQueue::generateMessage(), message length is zero");
        return nullptr;
    }

    uint32_t timestamp = INVALID_TS;
    if (streamId != 0U) {
        m_streamTimestamps.lock(false);
        auto entry = m_streamTimestamps.find(streamId);
        if (entry != m_streamTimestamps.end()) {
            timestamp = entry->second;
        }

        if (timestamp != INVALID_TS) {
            timestamp += (RTP_GENERIC_CLOCK_RATE / 133);
            if (m_debug)
                LogDebugEx(LOG_NET, "FrameQueue::generateMessage()", "RTP streamId = %u, previous TS = %u, TS = %u, rtpSeq = %u", streamId, m_streamTimestamps[streamId], timestamp, rtpSeq);
            m_streamTimestamps[streamId] = timestamp;
        }
        m_streamTimestamps.unlock();
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

    if (streamId != 0U && timestamp == INVALID_TS && rtpSeq != RTP_END_OF_CALL_SEQ) {
        if (m_debug)
            LogDebugEx(LOG_NET, "FrameQueue::generateMessage()", "RTP streamId = %u, initial TS = %u, rtpSeq = %u", streamId, header.getTimestamp(), rtpSeq);

        timestamp = (uint32_t)system_clock::ntp::now();
        header.setTimestamp(timestamp);

        m_streamTimestamps.insert(streamId, timestamp);
    }

    header.encode(buffer);

    if (streamId != 0U && rtpSeq == RTP_END_OF_CALL_SEQ) {
        m_streamTimestamps.lock(false);
        auto entry = m_streamTimestamps.find(streamId);
        if (entry != m_streamTimestamps.end()) {
            if (m_debug)
                LogDebugEx(LOG_NET, "FrameQueue::generateMessage()", "RTP streamId = %u, rtpSeq = %u", streamId, rtpSeq);
            m_streamTimestamps.unlock();
            m_streamTimestamps.erase(streamId);
        }
        m_streamTimestamps.unlock();
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
        Utils::dump(1U, "FrameQueue::generateMessage() Buffered Message", buffer, bufferLen);

    if (outBufferLen != nullptr) {
        *outBufferLen = bufferLen;
    }

    return buffer;
}
