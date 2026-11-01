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
#include "common/json/json.h"
#include "common/network/NetRPC.h"
#include "common/network/RPCHeader.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

using namespace network;

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
 * @brief Pumps two NetRPC clocks until a condition is met.
 * @param first The first NetRPC instance to clock.
 * @param second The second NetRPC instance to clock.
 * @param done A callable that returns true when the pumping should stop.
 * @returns True if the pumping stopped because the condition was met, false if the maximum number of iterations was reached.
 */
static bool pumpRPCUntil(NetRPC& first, NetRPC& second, const std::function<bool()>& done)
{
    for (uint32_t i = 0U; i < 120U; i++) {
        first.clock(1U);
        second.clock(1U);
        if (done())
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}

TEST_CASE("NetRPC defaultResponse populates status and message", "[network][rpc][netrpc]")
{
    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    NetRPC rpc("127.0.0.1", localPort, localPort, "test-password", false);

    json::object reply;
    rpc.defaultResponse(reply, "invalid arguments", NetRPC::INVALID_ARGS);

    REQUIRE(reply["status"].is<int>());
    REQUIRE(reply["status"].get<int>() == (int)NetRPC::INVALID_ARGS);
    REQUIRE(reply["message"].is<std::string>());
    REQUIRE(reply["message"].get<std::string>() == "invalid arguments");
}

TEST_CASE("NetRPC register and unregister handler guards duplicate and out-of-range IDs", "[network][rpc][netrpc]")
{
    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    NetRPC rpc("127.0.0.1", localPort, localPort, "test-password", false);

    NetRPC::RPCType handler = [](json::object& request, json::object& reply) {
        (void)request;
        int ok = (int)NetRPC::OK;
        reply["status"].set<int>(ok);
    };

    REQUIRE(rpc.registerHandler(0x0123U, handler));
    REQUIRE_FALSE(rpc.registerHandler(0x0123U, handler));
    REQUIRE(rpc.unregisterHandler(0x0123U));
    REQUIRE_FALSE(rpc.unregisterHandler(0x0123U));

    REQUIRE_FALSE(rpc.registerHandler(RPC_MAX_FUNC + 1U, handler));
    REQUIRE_FALSE(rpc.unregisterHandler(RPC_MAX_FUNC + 1U));
}

TEST_CASE("NetRPC refuses to send requests to itself", "[network][rpc][netrpc]")
{
    const uint16_t localPort = reserveLoopbackPort();
    REQUIRE(localPort != 0U);

    NetRPC rpc("127.0.0.1", localPort, localPort, "test-password", false);

    json::object request;
    request["op"].set<std::string>("self-test");

    REQUIRE_FALSE(rpc.req(0x0100U, request, NetRPC::RPCType(), "127.0.0.1", localPort, false));
}

TEST_CASE("NetRPC request receives JSON reply from registered remote handler", "[network][rpc][netrpc]")
{
    const uint16_t serverPort = reserveLoopbackPort();
    const uint16_t clientPort = reserveLoopbackPort();
    REQUIRE(serverPort != 0U);
    REQUIRE(clientPort != 0U);
    REQUIRE(serverPort != clientPort);

    NetRPC server("127.0.0.1", serverPort, serverPort, "shared-password", false);
    NetRPC client("127.0.0.1", clientPort, clientPort, "shared-password", false);

    REQUIRE(server.open());
    REQUIRE(client.open());

    bool serverCalled = false;
    bool clientReplyCalled = false;
    std::string echoed;

    REQUIRE(server.registerHandler(0x2222U, [&](json::object& request, json::object& reply) {
        serverCalled = true;

        std::string op = "";
        if (request["op"].is<std::string>())
            op = request["op"].get<std::string>();

        int status = (int)NetRPC::OK;
        reply["status"].set<int>(status);
        reply["message"].set<std::string>("ok");
        reply["echo"].set<std::string>(op);
    }));

    json::object request;
    request["op"].set<std::string>("ping");

    REQUIRE(client.req(0x2222U, request,
        [&](json::object& response, json::object& unused) {
            (void)unused;
            if (!response["status"].is<int>())
                return;

            if (response["status"].get<int>() != (int)NetRPC::OK)
                return;

            if (!response["echo"].is<std::string>())
                return;

            echoed = response["echo"].get<std::string>();
            clientReplyCalled = true;
        },
        "127.0.0.1", serverPort, false));

    REQUIRE(pumpRPCUntil(server, client, [&]() { return clientReplyCalled; }));
    REQUIRE(serverCalled);
    REQUIRE(clientReplyCalled);
    REQUIRE(echoed == "ping");

    server.close();
    client.close();
}

TEST_CASE("NetRPC blocking request times out without a reply", "[network][rpc][netrpc]")
{
    const uint16_t serverPort = reserveLoopbackPort();
    const uint16_t clientPort = reserveLoopbackPort();
    REQUIRE(serverPort != 0U);
    REQUIRE(clientPort != 0U);

    NetRPC server("127.0.0.1", serverPort, serverPort, "shared-password", false);
    NetRPC client("127.0.0.1", clientPort, clientPort, "shared-password", false);

    REQUIRE(server.open());
    REQUIRE(client.open());

    json::object request;
    request["op"].set<std::string>("timeout");

    REQUIRE_FALSE(client.req(0x3333U, request, NetRPC::RPCType(), "127.0.0.1", serverPort, true));

    server.close();
    client.close();
}