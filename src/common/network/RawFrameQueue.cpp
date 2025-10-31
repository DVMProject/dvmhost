// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "network/RawFrameQueue.h"
#include "network/udp/Socket.h"
#include "Log.h"
#include "Thread.h"
#include "Utils.h"

using namespace network;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

std::mutex RawFrameQueue::s_flushMtx;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RawFrameQueue class. */

RawFrameQueue::RawFrameQueue(udp::Socket* socket, bool debug) :
    m_socket(socket),
    m_failedReadCnt(0U),
    m_debug(debug)
{
    /* stub */
}

/* Finalizes a instance of the RawFrameQueue class. */

RawFrameQueue::~RawFrameQueue() = default;

/* Read message from the received UDP packet. */

UInt8Array RawFrameQueue::read(int& messageLength, sockaddr_storage& address, uint32_t& addrLen)
{
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
            Utils::dump(1U, "RawFrameQueue::read(), Network Packet", buffer, length);

        m_failedReadCnt = 0U;

        // copy message
        messageLength = length;
        UInt8Array message = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
        ::memcpy(message.get(), buffer, length);

        return message;
    }

    return nullptr;
}

/* Write message to the UDP socket. */

bool RawFrameQueue::write(const uint8_t* message, uint32_t length, sockaddr_storage& addr, uint32_t addrLen, ssize_t* lenWritten)
{
    if (message == nullptr) {
        LogError(LOG_NET, "RawFrameQueue::write(), message is null");
        return false;
    }
    if (length == 0U) {
        LogError(LOG_NET, "RawFrameQueue::write(), message length is zero");
        return false;
    }

    uint8_t* buffer = new uint8_t[length];
    ::memset(buffer, 0x00U, length);
    ::memcpy(buffer, message, length);

    if (m_debug)
        Utils::dump(1U, "RawFrameQueue::write(), Message", buffer, length);

    // bryanb: this is really a developer warning not a end-user warning, there's nothing the end-users can do about
    //  this message
    if (length > (DATA_PACKET_LENGTH - OVERSIZED_PACKET_WARN)) {
        LogDebug(LOG_NET, "RawFrameQueue::write(), WARN: packet length is possibly oversized, possible data truncation - BUGBUG");
    }

    bool ret = true;
    if (!m_socket->write(buffer, length, addr, addrLen, lenWritten)) {
        // LogError(LOG_NET, "Failed writing data to the network");
        ret = false;
    }

    delete[] buffer;
    return ret;
}

/* Cache message to frame queue. */

void RawFrameQueue::enqueueMessage(udp::BufferQueue* queue, const uint8_t* message, uint32_t length, sockaddr_storage& addr, uint32_t addrLen)
{
    if (queue == nullptr) {
        LogError(LOG_NET, "RawFrameQueue::enqueueMessage(), queue is null");
        return;
    }
    if (message == nullptr) {
        LogError(LOG_NET, "RawFrameQueue::enqueueMessage(), message is null");
        return;
    }
    if (length == 0U) {
        LogError(LOG_NET, "RawFrameQueue::enqueueMessage(), message length is zero");
        return;
    }

    // bryanb: this is really a developer warning not a end-user warning, there's nothing the end-users can do about
    //  this message
    if (length > (DATA_PACKET_LENGTH - OVERSIZED_PACKET_WARN)) {
        LogDebug(LOG_NET, "RawFrameQueue::enqueueMessage(), WARN: packet length is possibly oversized, possible data truncation - BUGBUG");
    }

    uint8_t* buffer = new uint8_t[length];
    ::memset(buffer, 0x00U, length);
    ::memcpy(buffer, message, length);

    if (m_debug)
        Utils::dump(1U, "RawFrameQueue::enqueueMessage(), Buffered Message", buffer, length);

    udp::UDPDatagram* dgram = new udp::UDPDatagram;
    dgram->buffer = buffer;
    dgram->length = length;
    dgram->address = addr;
    dgram->addrLen = addrLen;

    queue->push(dgram);
}

/* Flush the message queue. */

bool RawFrameQueue::flushQueue(udp::BufferQueue* queue)
{
    if (queue == nullptr) {
        LogError(LOG_NET, "RawFrameQueue::flushQueue(), queue is null");
        return false;
    }

    std::lock_guard<std::mutex> lock(s_flushMtx);

    if (queue->empty()) {
        return false;
    }

    // bryanb: this is the same as above -- but for some assinine reason prevents
    // weirdness
    if (queue->size() == 0U) {
        return false;
    }

    // LogDebug(LOG_NET, "queue len = %u", queue.size());

    bool ret = true;
    if (!m_socket->write(queue)) {
        // LogError(LOG_NET, "Failed writing data to the network");
        ret = false;
    }

    return ret;
}
