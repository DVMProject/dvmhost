// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
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

std::mutex RawFrameQueue::m_queueMutex;
bool RawFrameQueue::m_queueFlushing = false;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RawFrameQueue class. */

RawFrameQueue::RawFrameQueue(udp::Socket* socket, bool debug) :
    m_socket(socket),
    m_buffers(),
    m_failedReadCnt(0U),
    m_debug(debug)
{
    /* stub */
}

/* Finalizes a instance of the RawFrameQueue class. */

RawFrameQueue::~RawFrameQueue()
{
    deleteBuffers();
}

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
            Utils::dump(1U, "Network Packet", buffer, length);

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
    assert(message != nullptr);
    assert(length > 0U);

    uint8_t* buffer = new uint8_t[length];
    ::memset(buffer, 0x00U, length);
    ::memcpy(buffer, message, length);

    if (m_debug)
        Utils::dump(1U, "RawFrameQueue::write() Message", buffer, length);

    bool ret = true;
    if (!m_socket->write(buffer, length, addr, addrLen, lenWritten)) {
        // LogError(LOG_NET, "Failed writing data to the network");
        ret = false;
    }

    return ret;
}

/* Cache message to frame queue. */

void RawFrameQueue::enqueueMessage(const uint8_t* message, uint32_t length, sockaddr_storage& addr, uint32_t addrLen)
{
    assert(message != nullptr);
    assert(length > 0U);

    // if the queue is flushing -- don't attempt to enqueue any messages
    if (m_queueFlushing) {
        LogWarning(LOG_NET, "RawFrameQueue::enqueueMessage() -- queue is flushing, waiting to enqueue message");
        while (m_queueFlushing)
            Thread::sleep(2U);
    }

    uint8_t* buffer = new uint8_t[length];
    ::memset(buffer, 0x00U, length);
    ::memcpy(buffer, message, length);

    if (m_debug)
        Utils::dump(1U, "RawFrameQueue::enqueueMessage() Buffered Message", buffer, length);

    udp::UDPDatagram* dgram = new udp::UDPDatagram;
    dgram->buffer = buffer;
    dgram->length = length;
    dgram->address = addr;
    dgram->addrLen = addrLen;

    m_buffers.push_back(dgram);
}

/* Flush the message queue. */

bool RawFrameQueue::flushQueue()
{
    bool ret = true;
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_queueFlushing = true;

    if (m_buffers.empty()) {
        return false;
    }

    // bryanb: this is the same as above -- but for some assinine reason prevents
    // weirdness
    if (m_buffers.size() == 0U) {
        return false;
    }

    // LogDebug(LOG_NET, "m_buffers len = %u", m_buffers.size());

    ret = true;
    if (!m_socket->write(m_buffers)) {
        // LogError(LOG_NET, "Failed writing data to the network");
        ret = false;
    }

    deleteBuffers();
    m_queueFlushing = false;
    return ret;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper to ensure buffers are deleted. */

void RawFrameQueue::deleteBuffers()
{
    for (auto& buffer : m_buffers) {
        if (buffer != nullptr) {
            // LogDebug(LOG_NET, "deleting buffer, addr %p len %u", buffer->buffer, buffer->length);
            if (buffer->buffer != nullptr) {
                delete[] buffer->buffer;
                buffer->length = 0;
                buffer->buffer = nullptr;
            }

            delete buffer;
            buffer = nullptr;
        }
    }
    m_buffers.clear();
}
