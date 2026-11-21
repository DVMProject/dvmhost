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
#include "common/restapi/http/ServerConnectionManager.h"
#include "common/restapi/RequestDispatcher.h"

#include <asio.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

    /**
     * @brief Sends a partial request, closes the sending direction, and collects any response.
     * @param port The port to which the partial request is sent.
     * @param request Partial HTTP request bytes.
     * @return Raw response bytes received before the server closes the connection.
     */
    std::string sendPartialRequest(uint16_t port, const std::string& request)
    {
        asio::io_service ioService;
        asio::ip::tcp::socket socket(ioService);
        socket.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port));
        asio::write(socket, asio::buffer(request));

        asio::error_code ec;
        socket.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
        REQUIRE_FALSE(ec);

        std::string response;
        std::array<char, 4096U> buffer {};
        do {
            const size_t length = socket.read_some(asio::buffer(buffer), ec);
            response.append(buffer.data(), length);
        } while (!ec);

        REQUIRE(ec == asio::error::eof);
        return response;
    }

    /**
     * @brief Sends a raw HTTP request without using test assertions in the calling thread.
     * @param port The port to which the HTTP request should be sent.
     * @param request Complete HTTP request.
     * @return The raw HTTP response received from the server.
     */
    std::string sendRawRequestUnchecked(uint16_t port, const std::string& request)
    {
        asio::io_service ioService;
        asio::ip::tcp::socket socket(ioService);
        asio::error_code ec;
        socket.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port), ec);
        if (ec)
            return std::string();

        asio::write(socket, asio::buffer(request), ec);
        if (ec)
            return std::string();

        std::string response;
        std::array<char, 4096U> buffer {};
        do {
            const size_t length = socket.read_some(asio::buffer(buffer), ec);
            response.append(buffer.data(), length);
        } while (!ec);

        return response;
    }

    std::string responseBody(const std::string& response)
    {
        const size_t separator = response.find("\r\n\r\n");
        REQUIRE(separator != std::string::npos);
        return response.substr(separator + 4U);
    }

    struct ManagedConnection;
    using ManagedConnectionPtr = std::shared_ptr<ManagedConnection>;
    using TestConnectionManager = ServerConnectionManager<ManagedConnectionPtr>;

    /**
     * @brief Minimal managed connection used to verify manager callback lock scope.
     */
    struct ManagedConnection {
        std::function<void()> onStart;
        std::function<void()> onStop;
        std::atomic<unsigned int> starts { 0U };
        std::atomic<unsigned int> stops { 0U };

        /** @brief Records start and invokes the configured callback. */
        void start()
        {
            ++starts;
            if (onStart)
                onStart();
        }

        /** @brief Records stop and invokes the configured callback. */
        void stop()
        {
            ++stops;
            if (onStop)
                onStop();
        }
    };
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

TEST_CASE("HTTP server accepts every request header and body split", "[restapi][http][framing][e2e]")
{
    LoopbackHTTPServer fixture;
    const std::string body("split\0body", 10U);
    const std::string request =
        "PUT /split HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;

    for (size_t split = 1U; split < request.size(); ++split) {
        CAPTURE(split);
        const std::string response = sendRawRequest(fixture.port,
            { request.substr(0U, split), request.substr(split) });
        REQUIRE(response.find("HTTP/1.0 200 OK\r\n") == 0U);
        REQUIRE(responseBody(response) == body);
    }

    REQUIRE(fixture.state->calls == request.size() - 1U);
}

TEST_CASE("HTTP server preserves fragmented binary bodies", "[restapi][http][framing][e2e]")
{
    LoopbackHTTPServer fixture;
    std::string body(20000U, 'a');
    body[0U] = '\0';
    body[8191U] = '\0';
    body[19999U] = '\0';

    const std::string header =
        "PUT /binary HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n";
    std::vector<std::string> fragments { header };
    for (size_t offset = 0U; offset < body.size(); offset += 997U)
        fragments.push_back(body.substr(offset, std::min<size_t>(997U, body.size() - offset)));

    const std::string response = sendRawRequest(fixture.port, fragments);

    REQUIRE(response.find("HTTP/1.0 200 OK\r\n") == 0U);
    REQUIRE(responseBody(response) == body);
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

TEST_CASE("HTTP server rejects ambiguous Content-Length syntax", "[restapi][http][framing][e2e][negative]")
{
    LoopbackHTTPServer fixture;
    const std::vector<std::string> values {
        "+1",
        "-1",
        "1, 1",
        "1x",
        "1844674407370955161600000"
    };

    for (const std::string& value : values) {
        CAPTURE(value);
        const std::string request =
            "PUT /length HTTP/1.1\r\nHost: localhost\r\nContent-Length: " + value + "\r\n\r\n";
        const std::string response = sendRawRequest(fixture.port, { request });
        REQUIRE(response.find("HTTP/1.0 400 Bad Request\r\n") == 0U);
    }

    REQUIRE(fixture.state->calls == 0U);
}

TEST_CASE("HTTP server never dispatches truncated requests", "[restapi][http][framing][e2e][negative]")
{
    LoopbackHTTPServer fixture;

    REQUIRE(sendPartialRequest(fixture.port,
        "PUT /partial HTTP/1.1\r\nHost: localhost\r\nContent-Len").empty());
    REQUIRE(sendPartialRequest(fixture.port,
        "PUT /partial HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhe").empty());
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

TEST_CASE("HTTP server services another connection while a REST handler is running", "[restapi][http][dispatcher][e2e]")
{
    std::atomic<bool> slowStarted { false };
    restapi::DefaultRequestDispatcher dispatcher;
    dispatcher.match("/slow").get([&slowStarted](const HTTPPayload&, HTTPPayload& reply, const restapi::RequestMatch&) {
        slowStarted = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::string content = "slow";
        reply.payload(content, HTTPPayload::OK, "text/plain");
    });
    dispatcher.match("/fast").get([](const HTTPPayload&, HTTPPayload& reply, const restapi::RequestMatch&) {
        std::string content = "fast";
        reply.payload(content, HTTPPayload::OK, "text/plain");
    });

    HTTPServer<restapi::DefaultRequestDispatcher> server("127.0.0.1", 0U, false);
    server.setHandler(dispatcher);
    server.open();
    const uint16_t port = server.localPort();
    std::thread serverThread([&server]() { server.run(); });

    const std::string slowRequest = "GET /slow HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n";
    const std::string fastRequest = "GET /fast HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n";
    std::string slowResponse;
    std::thread slowThread([&]() { slowResponse = sendRawRequestUnchecked(port, slowRequest); });

    const auto startDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!slowStarted && std::chrono::steady_clock::now() < startDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool slowObserved = slowStarted;

    const auto fastStarted = std::chrono::steady_clock::now();
    const std::string fastResponse = sendRawRequestUnchecked(port, fastRequest);
    const auto fastElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - fastStarted);

    slowThread.join();
    server.stop();
    serverThread.join();

    REQUIRE(slowObserved);
    REQUIRE(fastResponse.find("HTTP/1.0 200 OK\r\n") == 0U);
    REQUIRE(responseBody(fastResponse) == "fast");
    REQUIRE(fastElapsed < std::chrono::milliseconds(400));
    REQUIRE(responseBody(slowResponse) == "slow");
}

TEST_CASE("HTTP connection manager invokes lifecycle callbacks outside its lock", "[restapi][http][connection-manager]")
{
    TestConnectionManager manager;
    ManagedConnectionPtr first = std::make_shared<ManagedConnection>();
    ManagedConnectionPtr second = std::make_shared<ManagedConnection>();

    first->onStart = [&manager, second]() { manager.start(second); };
    first->onStop = [&manager, second]() { manager.stop(second); };

    manager.start(first);
    REQUIRE(first->starts == 1U);
    REQUIRE(second->starts == 1U);

    manager.stop(first);
    REQUIRE(first->stops == 1U);
    REQUIRE(second->stops == 1U);
}

TEST_CASE("HTTP connection manager drains before invoking stop callbacks", "[restapi][http][connection-manager]")
{
    TestConnectionManager manager;
    ManagedConnectionPtr first = std::make_shared<ManagedConnection>();
    ManagedConnectionPtr replacement = std::make_shared<ManagedConnection>();

    first->onStop = [&manager, replacement]() { manager.start(replacement); };
    manager.start(first);
    manager.stopAll();

    REQUIRE(first->stops == 1U);
    REQUIRE(replacement->starts == 1U);
    REQUIRE(replacement->stops == 0U);

    manager.stopAll();
    REQUIRE(replacement->stops == 1U);
}
