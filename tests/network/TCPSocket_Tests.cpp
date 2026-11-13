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
#include "common/network/tcp/Socket.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

using namespace network::tcp;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Finds an available loopback TCP port.
 * @return uint16_t A free TCP port, or 0 on failure.
 */
static uint16_t reserveLoopbackPort()
{
#if defined(_WIN32)
    SOCKET fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET)
        return 0U;
#else
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
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
 * @brief Waits for a pending TCP accept and returns the accepted file descriptor.
 */
static int acceptWithRetry(Socket& server, sockaddr_storage& remote, socklen_t& remoteLen)
{
    for (uint32_t attempt = 0U; attempt < 100U; attempt++) {
        int fd = server.accept(reinterpret_cast<sockaddr*>(&remote), &remoteLen);
        if (fd >= 0)
            return fd;

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return -1;
}

/**
 * @brief Reads a single TCP message with bounded retries.
 */
static bool readWithRetry(Socket& socket, const std::string& expected)
{
    for (uint32_t attempt = 0U; attempt < 100U; attempt++) {
        uint8_t buffer[256U] = { 0U };
        ssize_t len = socket.read(buffer, sizeof(buffer));
        if (len < 0)
            return false;

        if (len > 0) {
            if ((size_t)len != expected.size())
                return false;

            return ::memcmp(buffer, expected.data(), expected.size()) == 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}

TEST_CASE("TCP socket exchanges data over loopback", "[network][tcp]")
{
    const uint16_t port = reserveLoopbackPort();
    REQUIRE(port != 0U);

    Socket server(AF_INET, SOCK_STREAM, 0);
    REQUIRE(server.listen("127.0.0.1", port, 1) == 0);

    Socket client(AF_INET, SOCK_STREAM, 0);
    REQUIRE(client.connect("127.0.0.1", port));

    sockaddr_storage remote = {};
    socklen_t remoteLen = sizeof(remote);
    int acceptedFd = acceptWithRetry(server, remote, remoteLen);
    REQUIRE(acceptedFd >= 0);

    Socket accepted(acceptedFd);

    const std::string clientToServer = "tcp-client-to-server";
    REQUIRE(client.write(reinterpret_cast<const uint8_t*>(clientToServer.data()), clientToServer.size()) == (ssize_t)clientToServer.size());
    REQUIRE(readWithRetry(accepted, clientToServer));

    const std::string serverToClient = "tcp-server-to-client";
    REQUIRE(accepted.write(reinterpret_cast<const uint8_t*>(serverToClient.data()), serverToClient.size()) == (ssize_t)serverToClient.size());
    REQUIRE(readWithRetry(client, serverToClient));

    REQUIRE(Socket::address(remote) == "127.0.0.1");
    REQUIRE(Socket::port(remote) > 0U);
    REQUIRE(Socket::addr(remote) == htonl(INADDR_LOOPBACK));
}

TEST_CASE("TCP socket read returns zero when no data is ready", "[network][tcp]")
{
    const uint16_t port = reserveLoopbackPort();
    REQUIRE(port != 0U);

    Socket server(AF_INET, SOCK_STREAM, 0);
    REQUIRE(server.listen("127.0.0.1", port, 1) == 0);

    Socket client(AF_INET, SOCK_STREAM, 0);
    REQUIRE(client.connect("127.0.0.1", port));

    sockaddr_storage remote = {};
    socklen_t remoteLen = sizeof(remote);
    int acceptedFd = acceptWithRetry(server, remote, remoteLen);
    REQUIRE(acceptedFd >= 0);

    Socket accepted(acceptedFd);

    uint8_t buffer[32U] = { 0U };
    ssize_t len = accepted.read(buffer, sizeof(buffer));
    REQUIRE(len == 0);
}

TEST_CASE("TCP helper detects INADDR_NONE", "[network][tcp]")
{
    sockaddr_storage noneAddr = {};
    sockaddr_in* in = reinterpret_cast<sockaddr_in*>(&noneAddr);
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = htonl(INADDR_NONE);
    in->sin_port = htons(12345U);

    REQUIRE(Socket::isNone(noneAddr));

    sockaddr_storage loopbackAddr = {};
    sockaddr_in* loop = reinterpret_cast<sockaddr_in*>(&loopbackAddr);
    loop->sin_family = AF_INET;
    loop->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    loop->sin_port = htons(54321U);

    REQUIRE_FALSE(Socket::isNone(loopbackAddr));
    REQUIRE(Socket::address(loopbackAddr) == "127.0.0.1");
    REQUIRE(Socket::port(loopbackAddr) == 54321U);
}

TEST_CASE("TCP operations fail on uninitialized socket descriptor", "[network][tcp]")
{
    Socket socket;

    sockaddr_storage target = {};
    sockaddr_in* in = reinterpret_cast<sockaddr_in*>(&target);
    in->sin_family = AF_INET;
    in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    in->sin_port = htons(6553U);

    const std::string payload = "payload";
    uint8_t buffer[16U] = { 0U };

    REQUIRE_FALSE(socket.connect("127.0.0.1", 6553U));
    REQUIRE(socket.listen("127.0.0.1", 6553U, 1) == -1);
    REQUIRE(socket.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size()) == -1);
    REQUIRE(socket.read(buffer, sizeof(buffer)) == -1);
}

TEST_CASE("TCP socket rejects invalid connect hostnames", "[network][tcp]")
{
    Socket client(AF_INET, SOCK_STREAM, 0);
    REQUIRE_THROWS_AS(client.connect("not-an-ip-address", 12345U), std::runtime_error);
}

TEST_CASE("TCP listen fails when the port is already in use", "[network][tcp]")
{
    const uint16_t port = reserveLoopbackPort();
    REQUIRE(port != 0U);

    Socket server1(AF_INET, SOCK_STREAM, 0);
    REQUIRE(server1.listen("127.0.0.1", port, 1) == 0);

    Socket server2(AF_INET, SOCK_STREAM, 0);
    REQUIRE(server2.listen("127.0.0.1", port, 1) == -1);
}