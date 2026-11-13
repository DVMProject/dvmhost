// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES FROM THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/network/FrameQueue.h"
#include "common/network/RawFrameQueue.h"
#include "common/network/RTPFNEHeader.h"
#include "common/network/RTPHeader.h"
#include "common/network/udp/Socket.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

using namespace network;
using namespace network::frame;
using namespace network::udp;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Finds an available loopback UDP port.
 * @return uint16_t A free UDP port, or 0 on failure.
 */
static uint16_t reserveLoopbackPort()
{
#if defined(_WIN32)
    SOCKET fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == INVALID_SOCKET)
        return 0U;
#else
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return 0U;
#endif // defined(_WIN32)

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(0U);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
#if defined(_WIN32)
        ::closesocket(fd);
#else
        ::close(fd);
#endif // defined(_WIN32)
        return 0U;
    }

    socklen_t addrLen = sizeof(address);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &addrLen) < 0) {
#if defined(_WIN32)
        ::closesocket(fd);
#else
        ::close(fd);
#endif // defined(_WIN32)
        return 0U;
    }

#if defined(_WIN32)
    ::closesocket(fd);
#else
    ::close(fd);
#endif // defined(_WIN32)

    return ntohs(address.sin_port);
}

/**
 * @brief Polls a RawFrameQueue until a message arrives or retries are exhausted.
 */
static bool waitForRawMessage(RawFrameQueue& queue, std::string& out)
{
    for (uint32_t attempt = 0U; attempt < 50U; attempt++) {
        int messageLength = -1;
        sockaddr_storage source = {};
        uint32_t sourceLen = 0U;
        UInt8Array message = queue.read(messageLength, source, sourceLen);
        if (message != nullptr && messageLength > 0) {
            out.assign(reinterpret_cast<char*>(message.get()), (size_t)messageLength);
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}

/**
 * @brief Polls a FrameQueue until a message arrives or retries are exhausted.
 */
static bool waitForFrameMessage(FrameQueue& queue, std::string& out, RTPHeader& rtpHeader, RTPFNEHeader& fneHeader)
{
    for (uint32_t attempt = 0U; attempt < 50U; attempt++) {
        int messageLength = -1;
        sockaddr_storage source = {};
        uint32_t sourceLen = 0U;
        UInt8Array message = queue.read(messageLength, source, sourceLen, &rtpHeader, &fneHeader);
        if (message != nullptr && messageLength > 0) {
            out.assign(reinterpret_cast<char*>(message.get()), (size_t)messageLength);
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}

TEST_CASE("RawFrameQueue writes and reads loopback payloads", "[network][framequeue][raw]")
{
    const uint16_t receiverPort = reserveLoopbackPort();
    REQUIRE(receiverPort != 0U);

    Socket receiverSocket(receiverPort);
    REQUIRE(receiverSocket.open(AF_INET));

    Socket senderSocket;
    REQUIRE(senderSocket.open(AF_INET));

    RawFrameQueue receiverQueue(&receiverSocket, false);
    RawFrameQueue senderQueue(&senderSocket, false);

    sockaddr_storage destination = {};
    uint32_t destinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", receiverPort, destination, destinationLen) == 0);

    const std::string payload = "raw-frame-loopback";
    ssize_t bytesWritten = -1;
    REQUIRE(senderQueue.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), destination, destinationLen, &bytesWritten));
    REQUIRE(bytesWritten == (ssize_t)payload.size());

    std::string received;
    REQUIRE(waitForRawMessage(receiverQueue, received));
    REQUIRE(received == payload);

    senderSocket.close();
    receiverSocket.close();
}

TEST_CASE("RawFrameQueue enqueues and flushes buffered payloads", "[network][framequeue][raw]")
{
    const uint16_t receiverPort = reserveLoopbackPort();
    REQUIRE(receiverPort != 0U);

    Socket receiverSocket(receiverPort);
    REQUIRE(receiverSocket.open(AF_INET));

    Socket senderSocket;
    REQUIRE(senderSocket.open(AF_INET));

    RawFrameQueue receiverQueue(&receiverSocket, false);
    RawFrameQueue senderQueue(&senderSocket, false);

    sockaddr_storage destination = {};
    uint32_t destinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", receiverPort, destination, destinationLen) == 0);

    BufferQueue queue;
    const std::string first = "raw-queued-one";
    const std::string second = "raw-queued-two";
    senderQueue.enqueueMessage(&queue, reinterpret_cast<const uint8_t*>(first.data()), first.size(), destination, destinationLen);
    senderQueue.enqueueMessage(&queue, reinterpret_cast<const uint8_t*>(second.data()), second.size(), destination, destinationLen);

    REQUIRE(queue.size() == 2U);
    REQUIRE(senderQueue.flushQueue(&queue));
    REQUIRE(queue.empty());

    std::string firstReceived;
    std::string secondReceived;
    REQUIRE(waitForRawMessage(receiverQueue, firstReceived));
    REQUIRE(waitForRawMessage(receiverQueue, secondReceived));
    REQUIRE(firstReceived == first);
    REQUIRE(secondReceived == second);

    senderSocket.close();
    receiverSocket.close();
}

TEST_CASE("FrameQueue writes decodable RTP FNE payloads", "[network][framequeue]")
{
    const uint16_t receiverPort = reserveLoopbackPort();
    REQUIRE(receiverPort != 0U);

    Socket receiverSocket(receiverPort);
    REQUIRE(receiverSocket.open(AF_INET));

    Socket senderSocket;
    REQUIRE(senderSocket.open(AF_INET));

    FrameQueue senderQueue(&senderSocket, 1001U, false);
    FrameQueue receiverQueue(&receiverSocket, 2002U, false);
    senderQueue.clearTimestamps();

    sockaddr_storage destination = {};
    uint32_t destinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", receiverPort, destination, destinationLen) == 0);

    const std::string payload = "framequeue-rtp-message";
    const uint32_t streamId = 0x10203040U;
    const uint32_t peerId = 0x000F4240U;
    const uint32_t ssrc = 0x01020304U;
    const uint16_t sequence = 321U;
    FrameQueue::OpcodePair opcode = { NET_FUNC::PROTOCOL, NET_SUBFUNC::PROTOCOL_SUBFUNC_P25 };

    REQUIRE(senderQueue.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), streamId, peerId,
        ssrc, opcode, sequence, destination, destinationLen));

    std::string received;
    RTPHeader rtpHeader = RTPHeader();
    RTPFNEHeader fneHeader = RTPFNEHeader();
    REQUIRE(waitForFrameMessage(receiverQueue, received, rtpHeader, fneHeader));

    REQUIRE(received == payload);
    REQUIRE(rtpHeader.getExtension());
    REQUIRE(rtpHeader.getPayloadType() == DVM_RTP_PAYLOAD_TYPE);
    REQUIRE(rtpHeader.getSequence() == sequence);
    REQUIRE(rtpHeader.getSSRC() == ssrc);
    REQUIRE(rtpHeader.getTimestamp() != INVALID_TS);

    REQUIRE(fneHeader.getStreamId() == streamId);
    REQUIRE(fneHeader.getPeerId() == peerId);
    REQUIRE(fneHeader.getFunction() == opcode.first);
    REQUIRE(fneHeader.getSubFunction() == opcode.second);
    REQUIRE(fneHeader.getMessageLength() == payload.size());

    senderSocket.close();
    receiverSocket.close();
}

TEST_CASE("FrameQueue enqueues and flushes ordered RTP FNE payloads", "[network][framequeue]")
{
    const uint16_t receiverPort = reserveLoopbackPort();
    REQUIRE(receiverPort != 0U);

    Socket receiverSocket(receiverPort);
    REQUIRE(receiverSocket.open(AF_INET));

    Socket senderSocket;
    REQUIRE(senderSocket.open(AF_INET));

    FrameQueue senderQueue(&senderSocket, 3003U, false);
    FrameQueue receiverQueue(&receiverSocket, 4004U, false);
    senderQueue.clearTimestamps();

    sockaddr_storage destination = {};
    uint32_t destinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", receiverPort, destination, destinationLen) == 0);

    BufferQueue queue;
    const std::string first = "frame-queued-one";
    const std::string second = "frame-queued-two";
    const uint32_t streamId = 0x22334455U;
    const uint32_t peerId = 0x00001000U;
    const uint32_t ssrc = 0x0A0B0C0DU;
    FrameQueue::OpcodePair opcode = { NET_FUNC::TRANSFER, NET_SUBFUNC::TRANSFER_SUBFUNC_STATUS };

    senderQueue.enqueueMessage(&queue, reinterpret_cast<const uint8_t*>(first.data()), first.size(), streamId,
        peerId, ssrc, opcode, 500U, destination, destinationLen);
    senderQueue.enqueueMessage(&queue, reinterpret_cast<const uint8_t*>(second.data()), second.size(), streamId,
        peerId, ssrc, opcode, 501U, destination, destinationLen);

    REQUIRE(queue.size() == 2U);
    REQUIRE(senderQueue.flushQueue(&queue));
    REQUIRE(queue.empty());

    std::string firstReceived;
    std::string secondReceived;
    RTPHeader firstRtp = RTPHeader();
    RTPHeader secondRtp = RTPHeader();
    RTPFNEHeader firstFne = RTPFNEHeader();
    RTPFNEHeader secondFne = RTPFNEHeader();

    REQUIRE(waitForFrameMessage(receiverQueue, firstReceived, firstRtp, firstFne));
    REQUIRE(waitForFrameMessage(receiverQueue, secondReceived, secondRtp, secondFne));

    REQUIRE(firstReceived == first);
    REQUIRE(secondReceived == second);

    REQUIRE(firstRtp.getSequence() == 500U);
    REQUIRE(secondRtp.getSequence() == 501U);
    REQUIRE(secondRtp.getTimestamp() > firstRtp.getTimestamp());

    REQUIRE(firstFne.getStreamId() == streamId);
    REQUIRE(secondFne.getStreamId() == streamId);
    REQUIRE(firstFne.getPeerId() == peerId);
    REQUIRE(secondFne.getPeerId() == peerId);

    senderSocket.close();
    receiverSocket.close();
}