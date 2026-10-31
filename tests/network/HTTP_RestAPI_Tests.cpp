// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include <catch2/catch_test_macros.hpp>

#include "common/restapi/http/HTTPLexer.h"
#include "common/restapi/http/HTTPClient.h"
#include "common/restapi/http/HTTPPayload.h"
#include "common/restapi/http/HTTPServer.h"

#include <asio.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

using namespace restapi::http;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
namespace {
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Represents the state of an HTTP request for testing purposes.
     */
    struct RequestState {
        std::atomic<unsigned int> calls { 0U };
        std::mutex mutex;
        HTTPPayload request;
    };

    struct ResponseState {
        std::atomic<bool> received { false };
        std::mutex mutex;
        HTTPPayload response;
    };

    /**
     * @brief Sends a raw HTTP request to the specified port using the provided fragments and returns the response.
     * @param port The port to which the HTTP request should be sent.
     * @param fragments The fragments of the HTTP request to be sent.
     * @return The raw HTTP response received from the server.
     */
    std::string sendRawRequest(uint16_t port, const std::vector<std::string>& fragments)
    {
        asio::io_service ioService;
        asio::ip::tcp::socket socket(ioService);
        socket.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port));

        for (const std::string& fragment : fragments) {
            asio::write(socket, asio::buffer(fragment));
            if (fragments.size() > 1U)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::string response;
        std::array<char, 4096U> buffer {};
        asio::error_code ec;
        do {
            const size_t length = socket.read_some(asio::buffer(buffer), ec);
            response.append(buffer.data(), length);
        } while (!ec);

        REQUIRE(ec == asio::error::eof);
        return response;
    }

    std::string responseBody(const std::string& response)
    {
        const size_t separator = response.find("\r\n\r\n");
        REQUIRE(separator != std::string::npos);
        return response.substr(separator + 4U);
    }
}

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief A request handler that echoes back the received HTTP request content.
 */
class EchoRequestHandler {
public:
    /**
     * @brief Initializes a new instance of the EchoRequestHandler.
     */
    EchoRequestHandler() : m_state(std::make_shared<RequestState>()) { }
    /**
     * @brief Initializes a new instance of the EchoRequestHandler with the specified request state.
     */
    explicit EchoRequestHandler(std::shared_ptr<RequestState> state) : m_state(std::move(state)) { }

    /**
     * @brief Handles an incoming HTTP request and prepares the corresponding reply.
     * @param request The incoming HTTP request.
     * @param reply The HTTP reply to be sent back to the client.
     */
    void handleRequest(const HTTPPayload& request, HTTPPayload& reply)
    {
        // scope is intentional
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->request = request;
        }
        ++m_state->calls;

        std::string content = request.content;
        reply.payload(content, HTTPPayload::OK, "application/octet-stream");
    }

private:
    std::shared_ptr<RequestState> m_state;
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief A request handler that captures the HTTP response from the server.
 */
class ClientResponseHandler {
public:
    /**
     * @brief Initializes a new instance of the ClientResponseHandler.
     */
    ClientResponseHandler() : m_state(std::make_shared<ResponseState>()) { }
    /**
     * @brief Initializes a new instance of the ClientResponseHandler with the specified response state.
     * @param state The shared response state to be used by the handler.
     */
    explicit ClientResponseHandler(std::shared_ptr<ResponseState> state) : m_state(std::move(state)) { }

    /**
     * @brief Handles an incoming HTTP response from the server.
     * @param response The HTTP response received from the server.
     * @param reply The HTTP reply to be sent back to the server (ignored in this handler).
     */
    void handleRequest(const HTTPPayload& response, HTTPPayload& reply)
    {
        (void)reply;
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->response = response;
        }
        m_state->received = true;
    }

private:
    std::shared_ptr<ResponseState> m_state;
};

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief A simple loopback HTTP server for testing purposes.
 */
class LoopbackHTTPServer {
public:
    /**
     * @brief Initializes an instance of the loopback HTTP server.
     */
    LoopbackHTTPServer() :
        state(std::make_shared<RequestState>()),
        server("127.0.0.1", 0U, false)
    {
        server.setHandler(EchoRequestHandler(state));
        server.open();
        port = server.localPort();
        thread = std::thread([this]() { server.run(); });
    }

    /**
     * @brief Finalizes and stops the loopback HTTP server.
     */
    ~LoopbackHTTPServer()
    {
        server.stop();
        if (thread.joinable())
            thread.join();
    }

    LoopbackHTTPServer(const LoopbackHTTPServer&) = delete;
    LoopbackHTTPServer& operator=(const LoopbackHTTPServer&) = delete;

    std::shared_ptr<RequestState> state;
    HTTPServer<EchoRequestHandler> server;
    uint16_t port { 0U };
    std::thread thread;
};

TEST_CASE("HTTPLexer parses a valid HTTP request", "[restapi][http][lexer]")
{
    const std::string request =
        "GET /api/v1/status HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    HTTPLexer lexer(false);
    HTTPPayload payload;

    auto result = lexer.parse(payload, request.begin(), request.end());

    REQUIRE(std::get<0>(result) == HTTPLexer::GOOD);
    REQUIRE(payload.method == "GET");
    REQUIRE(payload.uri == "/api/v1/status");
    REQUIRE(payload.httpVersionMajor == 1);
    REQUIRE(payload.httpVersionMinor == 1);
    REQUIRE(payload.headers.find("host") == "localhost");
    REQUIRE(payload.headers.find("Content-Length") == "0");
}

TEST_CASE("HTTPLexer reports incomplete request as indeterminate", "[restapi][http][lexer]")
{
    const std::string partialRequest =
        "GET /health HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r";

    HTTPLexer lexer(false);
    HTTPPayload payload;

    auto result = lexer.parse(payload, partialRequest.begin(), partialRequest.end());

    REQUIRE(std::get<0>(result) == HTTPLexer::INDETERMINATE);
}

TEST_CASE("HTTPLexer rejects malformed request", "[restapi][http][lexer][negative]")
{
    const std::string malformedRequest =
        "GET /bad HTTP/1.x\r\n"
        "Host localhost\r\n"
        "\r\n";

    HTTPLexer lexer(false);
    HTTPPayload payload;

    auto result = lexer.parse(payload, malformedRequest.begin(), malformedRequest.end());

    REQUIRE(std::get<0>(result) == HTTPLexer::BAD);
}

TEST_CASE("HTTPLexer parses a valid HTTP response", "[restapi][http][lexer]")
{
    const std::string response =
        "HTTP/1.0 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    HTTPLexer lexer(true);
    HTTPPayload payload;

    auto result = lexer.parse(payload, response.begin(), response.end());

    REQUIRE(std::get<0>(result) == HTTPLexer::GOOD);
    REQUIRE(payload.status == HTTPPayload::OK);
    REQUIRE(payload.httpVersionMajor == 1);
    REQUIRE(payload.httpVersionMinor == 0);
    REQUIRE(payload.headers.find("content-length") == "0");
}

TEST_CASE("HTTP status payload applies default REST headers", "[restapi][http][payload]")
{
    HTTPPayload payload = HTTPPayload::statusPayload(HTTPPayload::BAD_REQUEST, "application/json");

    REQUIRE(payload.isClientPayload == false);
    REQUIRE(payload.status == HTTPPayload::BAD_REQUEST);
    REQUIRE(payload.headers.find("Content-Type") == "application/json");
    REQUIRE(payload.headers.find("Content-Length") == std::to_string(payload.content.size()));
    REQUIRE(payload.headers.find("Server").empty() == false);
}

TEST_CASE("HTTP client frames a GET request carrying JSON", "[restapi][http][client][e2e]")
{
    LoopbackHTTPServer fixture;
    auto responseState = std::make_shared<ResponseState>();

    HTTPClient<ClientResponseHandler> client("127.0.0.1", fixture.port);
    client.setHandler(ClientResponseHandler(responseState));
    REQUIRE(client.open());

    json::object body = json::object();
    HTTPPayload request = HTTPPayload::requestPayload(HTTP_GET, "/echo");
    request.payload(body);

    REQUIRE(request.content.empty() == false);
    REQUIRE(request.headers.find("Content-Type") == "application/json");
    REQUIRE(request.headers.find("Content-Length") == std::to_string(request.content.size()));
    REQUIRE(client.request(request));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!responseState->received && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    client.close();

    REQUIRE(responseState->received);
    REQUIRE(fixture.state->calls == 1U);
    {
        std::lock_guard<std::mutex> lock(fixture.state->mutex);
        REQUIRE(fixture.state->request.method == HTTP_GET);
        REQUIRE(fixture.state->request.uri == "/echo");
        REQUIRE(fixture.state->request.content == request.content);
    }
    {
        std::lock_guard<std::mutex> lock(responseState->mutex);
        REQUIRE(responseState->response.status == HTTPPayload::OK);
        REQUIRE(responseState->response.content == request.content);
    }
}

TEST_CASE("HTTP server handles a complete request over TCP", "[restapi][http][e2e]")
{
    LoopbackHTTPServer fixture;
    const std::string body("echo\0payload", 12U);
    const std::string request =
        "PUT /echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;

    const std::string response = sendRawRequest(fixture.port, { request });

    REQUIRE(response.find("HTTP/1.0 200 OK\r\n") == 0U);
    REQUIRE(responseBody(response) == body);
    REQUIRE(fixture.state->calls == 1U);
    std::lock_guard<std::mutex> lock(fixture.state->mutex);
    REQUIRE(fixture.state->request.method == "PUT");
    REQUIRE(fixture.state->request.uri == "/echo");
    REQUIRE(fixture.state->request.content == body);
}

TEST_CASE("HTTP server accumulates a fragmented request body", "[restapi][http][e2e]")
{
    LoopbackHTTPServer fixture;
    const std::vector<std::string> fragments {
        "PUT /fragmented HTTP/1.1\r\nHost: localhost\r\nContent-Length: 11\r\n\r\n",
        "hello ",
        "world"
    };

    const std::string response = sendRawRequest(fixture.port, fragments);

    REQUIRE(response.find("HTTP/1.0 200 OK\r\n") == 0U);
    REQUIRE(responseBody(response) == "hello world");
    REQUIRE(fixture.state->calls == 1U);
}

TEST_CASE("HTTP server rejects malformed Content-Length", "[restapi][http][e2e][negative]")
{
    LoopbackHTTPServer fixture;
    const std::string request =
        "PUT /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 12x\r\n\r\n";

    const std::string response = sendRawRequest(fixture.port, { request });

    REQUIRE(response.find("HTTP/1.0 400 Bad Request\r\n") == 0U);
    REQUIRE(fixture.state->calls == 0U);
}

TEST_CASE("HTTP server rejects an oversized declared body", "[restapi][http][e2e][negative]")
{
    LoopbackHTTPServer fixture;
    const std::string request =
        "PUT /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1048577\r\n\r\n";

    const std::string response = sendRawRequest(fixture.port, { request });

    REQUIRE(response.find("HTTP/1.0 400 Bad Request\r\n") == 0U);
    REQUIRE(fixture.state->calls == 0U);
}

TEST_CASE("HTTP server rejects bytes beyond the declared body", "[restapi][http][e2e][negative]")
{
    LoopbackHTTPServer fixture;
    const std::string request =
        "PUT /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\nextra";

    const std::string response = sendRawRequest(fixture.port, { request });

    REQUIRE(response.find("HTTP/1.0 400 Bad Request\r\n") == 0U);
    REQUIRE(fixture.state->calls == 0U);
}
