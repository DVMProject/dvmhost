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
#include "common/network/udp/Socket.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace network::udp;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Network Loopback implementation for testing.
 */
class LoopbackNetwork {
public:
    /**
     * @brief Initializes a new instance of the LoopbackNetwork class.
     */
    LoopbackNetwork() :
        m_fd(INVALID_NATIVE_SOCKET),
        m_address(),
        m_addrLen(0U),
        m_wsaStarted(false)
    {
#if defined(_WIN32)
        WSAData data;
        if (::WSAStartup(MAKEWORD(2, 2), &data) != 0)
            return;
        m_wsaStarted = true;
#endif // defined(_WIN32)

        m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (m_fd == INVALID_NATIVE_SOCKET)
            return;

        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_port = htons(0U);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(m_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
#if defined(_WIN32)
            if (m_fd != INVALID_NATIVE_SOCKET)
                ::closesocket(m_fd);
#else
            if (m_fd != INVALID_NATIVE_SOCKET)
                ::close(m_fd);
#endif // defined(_WIN32)
            m_fd = INVALID_NATIVE_SOCKET;
            return;
        }

        socklen_t addrLen = sizeof(address);
        if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&address), &addrLen) < 0) {
#if defined(_WIN32)
            if (m_fd != INVALID_NATIVE_SOCKET)
                ::closesocket(m_fd);
#else
            if (m_fd != INVALID_NATIVE_SOCKET)
                ::close(m_fd);
#endif // defined(_WIN32)
            m_fd = INVALID_NATIVE_SOCKET;
            return;
        }

        ::memcpy(&m_address, &address, sizeof(address));
        m_addrLen = (uint32_t)addrLen;

#if defined(_WIN32)
        DWORD timeout = 250U;
        ::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        timeval timeout = {};
        timeout.tv_usec = 250000;
        ::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif // defined(_WIN32)
    }

    /**
     * @brief Finalizes an instance of the LoopbackNetwork class.
     */
    ~LoopbackNetwork()
    {
#if defined(_WIN32)
        if (m_fd != INVALID_NATIVE_SOCKET)
            ::closesocket(m_fd);
        if (m_wsaStarted)
            ::WSACleanup();
#else
        if (m_fd != INVALID_NATIVE_SOCKET)
            ::close(m_fd);
#endif // defined(_WIN32)
    }

    /**
     * @brief Checks if the LoopbackNetwork instance is valid.
     * A valid instance has a properly initialized socket.
     * @return bool True if the instance is valid, false otherwise.
     */
    bool valid() const { return m_fd != INVALID_NATIVE_SOCKET; }

    /**
     * @brief Gets the address information of the LoopbackNetwork instance.
     * @return const sockaddr_storage& The address information of the LoopbackNetwork instance.
     */
    const sockaddr_storage& address() const { return m_address; }

    /**
     * @brief Gets the length of the address information of the LoopbackNetwork instance.
     * @return uint32_t The length of the address information of the LoopbackNetwork instance.
     */
    uint32_t addrLen() const { return m_addrLen; }

    /**
     * @brief Receives a UDP datagram and compares its payload with the expected string.
     * @param expected The expected payload string.
     * @return bool True if the received payload matches the expected string, false otherwise.
     */
    bool receive(const std::string& expected)
    {
        uint8_t buffer[256U] = { 0U };
        sockaddr_storage source = {};
        socklen_t sourceLen = sizeof(source);
        ssize_t length = ::recvfrom(m_fd, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
            reinterpret_cast<sockaddr*>(&source), &sourceLen);

        if (length < 0)
            return false;

        if ((size_t)length != expected.size())
            return false;

        return ::memcmp(buffer, expected.data(), expected.size()) == 0;
    }

    /**
     * @brief Receives a UDP datagram into a raw byte buffer.
     * @param[out] payload The datagram payload bytes.
     * @return bool True if a datagram was received, false otherwise.
     */
    bool receiveBytes(std::vector<uint8_t>& payload)
    {
        uint8_t buffer[256U] = { 0U };
        sockaddr_storage source = {};
        socklen_t sourceLen = sizeof(source);
        ssize_t length = ::recvfrom(m_fd, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
            reinterpret_cast<sockaddr*>(&source), &sourceLen);

        if (length <= 0)
            return false;

        payload.assign(buffer, buffer + length);
        return true;
    }

private:
#if defined(_WIN32)
    static constexpr SOCKET INVALID_NATIVE_SOCKET = INVALID_SOCKET;
    SOCKET m_fd;
#else
    static constexpr int INVALID_NATIVE_SOCKET = -1;
    int m_fd;
#endif // defined(_WIN32)
    sockaddr_storage m_address;
    uint32_t m_addrLen;
    bool m_wsaStarted;
};

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Creates a new UDPDatagram with the given payload and address information.
 * @param payload The data to be sent in the UDP datagram.
 * @param address The destination address for the UDP datagram.
 * @param addrLen The length of the destination address.
 * @return UDPDatagram* A pointer to the newly created UDPDatagram.
 */
UDPDatagram* makeDatagram(const std::string& payload, const sockaddr_storage& address, uint32_t addrLen)
{
    UDPDatagram* datagram = new UDPDatagram;
    datagram->buffer = new uint8_t[payload.size()];

    ::memcpy(datagram->buffer, payload.data(), payload.size());

    datagram->length = payload.size();
    datagram->address = address;
    datagram->addrLen = addrLen;
    return datagram;
}

/**
 * @brief Calculates the encrypted datagram payload size, including packet magic.
 * @param payloadLength Plaintext payload length.
 * @return size_t Encrypted payload length.
 */
size_t encryptedLength(size_t payloadLength)
{
    size_t padded = ((payloadLength + crypto::AES::BLOCK_BYTES_LEN - 1U) / crypto::AES::BLOCK_BYTES_LEN) * crypto::AES::BLOCK_BYTES_LEN;
    return padded + 18U;
}

/**
 * @brief Finds an available loopback UDP port.
 * @return uint16_t A free UDP port, or 0 on failure.
 */
uint16_t reserveLoopbackPort()
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
 * @brief Reads a UDP datagram from a socket with bounded retries.
 * @param socket Socket instance to read from.
 * @param expected Expected plaintext payload.
 * @return bool True if the expected payload was received.
 */
bool receiveFromSocketWithRetry(Socket& socket, const std::string& expected)
{
    for (uint32_t attempt = 0U; attempt < 50U; attempt++) {
        uint8_t buffer[256U] = { 0U };
        sockaddr_storage source = {};
        uint32_t sourceLen = 0U;

        ssize_t length = socket.read(buffer, sizeof(buffer), source, sourceLen);
        if (length < 0)
            return false;

        if (length > 0) {
            if ((size_t)length < expected.size())
                return false;

            if (::memcmp(buffer, expected.data(), expected.size()) != 0)
                return false;

            for (size_t i = expected.size(); i < (size_t)length; i++) {
                if (buffer[i] != 0x00U)
                    return false;
            }

            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}

TEST_CASE("UDP socket sends a single datagram", "[network][udp]")
{
    LoopbackNetwork receiver;
    REQUIRE(receiver.valid());

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    const std::string payload = "single-datagram";
    ssize_t bytesWritten = -1;
    REQUIRE(sender.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), receiver.address(), receiver.addrLen(), &bytesWritten));
    REQUIRE(bytesWritten == (ssize_t)payload.size());
    REQUIRE(receiver.receive(payload));

    sender.close();
}

TEST_CASE("UDP socket reports single datagram send failure", "[network][udp]")
{
    LoopbackNetwork receiver;
    REQUIRE(receiver.valid());

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    const std::string payload = "invalid-single-send";
    ssize_t bytesWritten = 0;
    REQUIRE_FALSE(sender.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), receiver.address(), 0U, &bytesWritten));
    REQUIRE(bytesWritten == -1);
    REQUIRE_FALSE(receiver.receive(payload));

    sender.close();
}

TEST_CASE("UDP socket encrypts a single datagram when a key is configured", "[network][udp][encrypted]")
{
    LoopbackNetwork receiver;
    REQUIRE(receiver.valid());

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    std::array<uint8_t, AES_WRAPPED_PCKT_KEY_LEN> key = {};
    for (size_t i = 0U; i < key.size(); i++)
        key[i] = (uint8_t)(i + 1U);
    sender.setPresharedKey(key.data());

    const std::string payload = "encrypted-single";
    ssize_t bytesWritten = -1;
    REQUIRE(sender.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), receiver.address(), receiver.addrLen(), &bytesWritten));
    REQUIRE(bytesWritten == (ssize_t)encryptedLength(payload.size()));

    std::vector<uint8_t> rawPayload;
    REQUIRE(receiver.receiveBytes(rawPayload));
    REQUIRE(rawPayload.size() == encryptedLength(payload.size()));
    const uint16_t packetMagic = (uint16_t(rawPayload[0U]) << 8) | uint16_t(rawPayload[1U]);
    REQUIRE(packetMagic == AES_WRAPPED_PCKT_MAGIC);

    sender.close();
}

TEST_CASE("UDP socket decrypts single datagrams with matching keys", "[network][udp][encrypted]")
{
    const uint16_t receiverPort = reserveLoopbackPort();
    REQUIRE(receiverPort != 0U);

    Socket receiver(receiverPort);
    REQUIRE(receiver.open(AF_INET));

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    std::array<uint8_t, AES_WRAPPED_PCKT_KEY_LEN> key = {};
    for (size_t i = 0U; i < key.size(); i++)
        key[i] = (uint8_t)(0xA0U + i);
    receiver.setPresharedKey(key.data());
    sender.setPresharedKey(key.data());

    sockaddr_storage destination = {};
    uint32_t destinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", receiverPort, destination, destinationLen) == 0);

    const std::string payload = "encrypted-roundtrip-single";
    ssize_t bytesWritten = -1;
    REQUIRE(sender.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), destination, destinationLen, &bytesWritten));
    REQUIRE(bytesWritten == (ssize_t)encryptedLength(payload.size()));
    REQUIRE(receiveFromSocketWithRetry(receiver, payload));

    sender.close();
    receiver.close();
}

TEST_CASE("UDP socket decrypts sendmmsg datagrams with matching keys", "[network][udp][encrypted]")
{
    const uint16_t receiverPort = reserveLoopbackPort();
    REQUIRE(receiverPort != 0U);

    Socket receiver(receiverPort);
    REQUIRE(receiver.open(AF_INET));

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    std::array<uint8_t, AES_WRAPPED_PCKT_KEY_LEN> key = {};
    for (size_t i = 0U; i < key.size(); i++)
        key[i] = (uint8_t)(0x30U + i);
    receiver.setPresharedKey(key.data());
    sender.setPresharedKey(key.data());

    sockaddr_storage destination = {};
    uint32_t destinationLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", receiverPort, destination, destinationLen) == 0);

    const std::string payloads[3U] = { "enc-batch-one", "enc-batch-two", "enc-batch-three" };
    BufferQueue queue;
    ssize_t expectedWritten = 0;
    for (size_t i = 0U; i < 3U; i++) {
        queue.push(makeDatagram(payloads[i], destination, destinationLen));
        expectedWritten += (ssize_t)encryptedLength(payloads[i].size());
    }

    ssize_t bytesWritten = -1;
    REQUIRE(sender.write(&queue, &bytesWritten));
    REQUIRE(queue.empty());
    REQUIRE(bytesWritten == expectedWritten);

    for (size_t i = 0U; i < 3U; i++)
        REQUIRE(receiveFromSocketWithRetry(receiver, payloads[i]));

    sender.close();
    receiver.close();
}

TEST_CASE("UDP socket sends every datagram in a sendmmsg", "[network][udp]")
{
    LoopbackNetwork receivers[5U];
    for (const LoopbackNetwork& receiver : receivers)
        REQUIRE(receiver.valid());

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    const std::string payloads[5U] = { "peer-1", "peer-2", "peer-3", "peer-4", "peer-5" };
    BufferQueue queue;
    ssize_t expectedBytes = 0;
    for (size_t i = 0U; i < 5U; i++) {
        queue.push(makeDatagram(payloads[i], receivers[i].address(), receivers[i].addrLen()));
        expectedBytes += (ssize_t)payloads[i].size();
    }

    ssize_t bytesWritten = -1;
    REQUIRE(sender.write(&queue, &bytesWritten));
    REQUIRE(bytesWritten == expectedBytes);
    REQUIRE(queue.empty());

    for (size_t i = 0U; i < 5U; i++)
        REQUIRE(receivers[i].receive(payloads[i]));

    sender.close();
}

// The old implementation may terminate under an address sanitizer because it
// reads uninitialized cleanup pointers. Keep this destructive regression case
// opt-in when the suite is run against an old binary.
TEST_CASE("UDP socket compacts a sendmmsg containing a skipped entry", "[network][udp]")
{
    LoopbackNetwork first;
    LoopbackNetwork second;
    REQUIRE(first.valid());
    REQUIRE(second.valid());

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    const std::string firstPayload = "before-skip";
    const std::string secondPayload = "after-skip";
    BufferQueue queue;
    queue.push(makeDatagram(firstPayload, first.address(), first.addrLen()));
    queue.push(nullptr);
    queue.push(makeDatagram(secondPayload, second.address(), second.addrLen()));

    ssize_t bytesWritten = -1;
    REQUIRE(sender.write(&queue, &bytesWritten));
    REQUIRE(bytesWritten == (ssize_t)(firstPayload.size() + secondPayload.size()));
    REQUIRE(queue.empty());
    REQUIRE(first.receive(firstPayload));
    REQUIRE(second.receive(secondPayload));

    sender.close();
}

TEST_CASE("UDP socket reports a complete sendmmsg send failure", "[network][udp]")
{
    LoopbackNetwork receiver;
    REQUIRE(receiver.valid());

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    BufferQueue queue;
    queue.push(makeDatagram("invalid-address-length", receiver.address(), 0U));

    ssize_t bytesWritten = 0;
    REQUIRE_FALSE(sender.write(&queue, &bytesWritten));
    REQUIRE(bytesWritten == -1);
    REQUIRE(queue.empty());
    REQUIRE_FALSE(receiver.receive("invalid-address-length"));

    sender.close();
}

TEST_CASE("UDP socket does not report a partial sendmmsg send as success", "[network][udp]")
{
    LoopbackNetwork first;
    LoopbackNetwork third;
    REQUIRE(first.valid());
    REQUIRE(third.valid());

    Socket sender;
    REQUIRE(sender.open(AF_INET));

    const std::string firstPayload = "sent-before-error";
    const std::string thirdPayload = "not-sent-after-error";
    BufferQueue queue;
    queue.push(makeDatagram(firstPayload, first.address(), first.addrLen()));
    queue.push(makeDatagram("invalid-address-length", third.address(), 0U));
    queue.push(makeDatagram(thirdPayload, third.address(), third.addrLen()));

    ssize_t bytesWritten = 0;
    REQUIRE_FALSE(sender.write(&queue, &bytesWritten));
    REQUIRE(bytesWritten == -1);
    REQUIRE(queue.empty());
    REQUIRE(first.receive(firstPayload));
    REQUIRE_FALSE(third.receive(thirdPayload));

    sender.close();
}

TEST_CASE("UDP socket resize helpers and address helpers work", "[network][udp]")
{
    Socket socket;
    REQUIRE(socket.open(AF_INET));
    REQUIRE(socket.recvBufSize(65536U));
    REQUIRE(socket.sendBufSize(65536U));

    sockaddr_storage loopback = {};
    uint32_t loopbackLen = 0U;
    REQUIRE(Socket::lookup("127.0.0.1", 12345U, loopback, loopbackLen) == 0);

    sockaddr_storage same = loopback;
    REQUIRE(Socket::match(loopback, same));
    REQUIRE(Socket::address(loopback) == "127.0.0.1");
    REQUIRE(Socket::port(loopback) == 12345U);

    sockaddr_storage invalid = {};
    uint32_t invalidLen = 0U;
    REQUIRE(Socket::lookup("not-a-hostname.invalid", 12345U, invalid, invalidLen) != 0);
    REQUIRE(Socket::isNone(invalid));
    socket.close();
}

#if (defined(HAVE_SENDMSG) && !defined(HAVE_SENDMMSG)) || defined(_WIN32)
TEST_CASE("UDP sendmmsg compatibility wrapper returns a datagram count", "[network][udp][compatibility]")
{
    LoopbackNetwork first;
    LoopbackNetwork second;
    REQUIRE(first.valid());
    REQUIRE(second.valid());

    NativeSocket sender = ::socket(AF_INET, SOCK_DGRAM, 0);
    REQUIRE(sender != INVALID_NATIVE_SOCKET);

    const std::string firstPayload = "compatibility-one";
    const std::string secondPayload = "compatibility-two";
    struct iovec chunks[2U] = {};
    struct mmsghdr headers[2U] = {};

    chunks[0U].iov_base = (void*)firstPayload.data();
    chunks[0U].iov_len = firstPayload.size();
    chunks[1U].iov_base = (void*)secondPayload.data();
    chunks[1U].iov_len = secondPayload.size();

    headers[0U].msg_hdr.msg_name = (void*)&first.address();
    headers[0U].msg_hdr.msg_namelen = first.addrLen();
    headers[0U].msg_hdr.msg_iov = &chunks[0U];
    headers[0U].msg_hdr.msg_iovlen = 1U;
    headers[1U].msg_hdr.msg_name = (void*)&second.address();
    headers[1U].msg_hdr.msg_namelen = second.addrLen();
    headers[1U].msg_hdr.msg_iov = &chunks[1U];
    headers[1U].msg_hdr.msg_iovlen = 1U;

    int sent = sendmmsg(sender, headers, 2U, 0);
    REQUIRE(sent == 2);
    REQUIRE(first.receive(firstPayload));
    REQUIRE(second.receive(secondPayload));

#if defined(_WIN32)
    if (sender != INVALID_NATIVE_SOCKET)
        ::closesocket(sender);
#else
    if (sender != INVALID_NATIVE_SOCKET)
        ::close(sender);
#endif // defined(_WIN32)
}
#endif // (defined(HAVE_SENDMSG) && !defined(HAVE_SENDMMSG)) || defined(_WIN32)
