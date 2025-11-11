// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Remote Command Client
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/edac/SHA256.h"
#include "common/json/json.h"
#include "common/restapi/http/HTTPClient.h"
#include "common/restapi/http/SecureHTTPClient.h"
#include "common/restapi/RequestDispatcher.h"
#include "common/Thread.h"
#include "common/Log.h"
#include "remote/RESTClient.h"

using namespace restapi;
using namespace restapi::http;

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define ERRNO_SOCK_OPEN 98
#define ERRNO_BAD_API_RESPONSE 97
#define ERRNO_API_CALL_TIMEOUT 96
#define ERRNO_BAD_AUTH_RESPONSE 95
#define ERRNO_INTERNAL_ERROR 100

#define ERRNO_NO_ADDRESS 404
#define ERRNO_NO_PASSWORD 403

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

bool RESTClient::s_responseAvailable = false;
HTTPPayload RESTClient::s_response;

bool RESTClient::s_console = false;
bool RESTClient::s_enableSSL = false;
bool RESTClient::s_debug = false;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* */

bool parseResponseBody(const HTTPPayload& response, json::object& obj)
{
    std::string contentType = response.headers.find("Content-Type");
    if (contentType != "application/json") {
        return false;
    }

    // parse JSON body
    json::value v;
    std::string err = json::parse(v, response.content);
    if (!err.empty()) {
        return false;
    }

    // ensure parsed JSON is an object
    if (!v.is<json::object>()) {
        return false;
    }

    obj = v.get<json::object>();
    return true;
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RESTClient class. */

RESTClient::RESTClient(const std::string& address, uint32_t port, const std::string& password, bool enableSSL, bool debug) :
    m_address(address),
    m_port(port),
    m_password(password)
{
    assert(!address.empty());
    assert(port > 0U);

    s_console = true;
    s_enableSSL = enableSSL;
    s_debug = debug;
}

/* Finalizes a instance of the RESTClient class. */

RESTClient::~RESTClient() = default;

/* Sends remote control command to the specified modem. */

int RESTClient::send(const std::string method, const std::string endpoint, json::object payload)
{
    json::object rsp = json::object();
    return send(method, endpoint, payload, rsp);
}

/* Sends remote control command to the specified modem. */

int RESTClient::send(const std::string method, const std::string endpoint, json::object payload, json::object& response)
{
    return send(m_address, m_port, m_password, method, endpoint, payload, response, s_enableSSL, s_debug);
}

/* Sends remote control command to the specified modem. */

int RESTClient::send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
    const std::string endpoint, json::object payload, bool enableSSL, int timeout, bool debug)
{
    json::object rsp = json::object();
    return send(address, port, password, method, endpoint, payload, rsp, enableSSL, timeout, debug);
}

/* Sends remote control command to the specified modem. */

int RESTClient::send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
    const std::string endpoint, json::object payload, json::object& response, bool enableSSL, int timeout, bool debug)
{
    if (address.empty()) {
        return ERRNO_NO_ADDRESS;
    }
    if (address == "0.0.0.0") {
        return ERRNO_NO_ADDRESS;
    }
    if (port <= 0U) {
        return ERRNO_NO_ADDRESS;
    }
    if (password.empty()) {
        return ERRNO_NO_PASSWORD;
    }

    int ret = EXIT_SUCCESS;
    s_enableSSL = enableSSL;
    s_debug = debug;

    typedef restapi::BasicRequestDispatcher<restapi::http::HTTPPayload, restapi::http::HTTPPayload> RESTDispatcherType;
    RESTDispatcherType m_dispatcher(RESTClient::responseHandler);
    HTTPClient<RESTDispatcherType>* client = nullptr;
#if defined(ENABLE_SSL)
    SecureHTTPClient<RESTDispatcherType>* sslClient = nullptr;
#endif // ENABLE_SSL

    try {
        // setup HTTP client for authentication payload
#if defined(ENABLE_SSL)
        if (s_enableSSL) {
            sslClient = new SecureHTTPClient<RESTDispatcherType>(address, port);
            if (!sslClient->open()) {
                delete sslClient;
                return ERRNO_SOCK_OPEN;
            }
            sslClient->setHandler(m_dispatcher);
        } else {
#endif // ENABLE_SSL
            client = new HTTPClient<RESTDispatcherType>(address, port);
            if (!client->open()) {
                delete client;
                return ERRNO_SOCK_OPEN;
            }
            client->setHandler(m_dispatcher);
#if defined(ENABLE_SSL)
        }
#endif // ENABLE_SSL

        // generate password SHA hash
        size_t size = password.size();

        uint8_t* in = new uint8_t[size];
        for (size_t i = 0U; i < size; i++)
            in[i] = password.at(i);

        uint8_t out[32U];
        ::memset(out, 0x00U, 32U);

        edac::SHA256 sha256;
        sha256.buffer(in, (uint32_t)(size), out);

        delete[] in;

        std::stringstream ss;
        ss << std::hex;

        for (uint8_t i = 0; i < 32U; i++)
            ss << std::setw(2) << std::setfill('0') << (int)out[i];

        std::string hash = ss.str();

        // send authentication API
        json::object request = json::object();
        request["auth"].set<std::string>(hash);

        HTTPPayload httpPayload = HTTPPayload::requestPayload(HTTP_PUT, "/auth");
        httpPayload.payload(request);
#if defined(ENABLE_SSL)
        if (s_enableSSL) {
            sslClient->request(httpPayload);
        } else {
#endif // ENABLE_SSL
            client->request(httpPayload);
#if defined(ENABLE_SSL)
        }
#endif // ENABLE_SSL

        // wait for response and parse
        if (wait()) {
#if defined(ENABLE_SSL)
            if (s_enableSSL) {
                sslClient->close();
                delete sslClient;
            } else {
#endif // ENABLE_SSL
                client->close();
                delete client;
#if defined(ENABLE_SSL)
            }
#endif // ENABLE_SSL
            return ERRNO_API_CALL_TIMEOUT;
        }

        json::object rsp = json::object();
        if (!parseResponseBody(s_response, rsp)) {
            return ERRNO_BAD_API_RESPONSE;
        }

        std::string token = "";
        int status = rsp["status"].get<int>();
        if (status == HTTPPayload::StatusType::OK) {
            token = rsp["token"].get<std::string>();
        }
        else {
#if defined(ENABLE_SSL)
            if (s_enableSSL) {
                sslClient->close();
                delete sslClient;
            } else {
#endif // ENABLE_SSL
                client->close();
                delete client;
#if defined(ENABLE_SSL)
            }
#endif // ENABLE_SSL
            return ERRNO_BAD_AUTH_RESPONSE;
        }

#if defined(ENABLE_SSL)
        if (s_enableSSL) {
            sslClient->close();
            delete sslClient;
        } else {
#endif // ENABLE_SSL
            client->close();
            delete client;
#if defined(ENABLE_SSL)
        }
#endif // ENABLE_SSL

        // reset the HTTP client and setup for actual payload request
#if defined(ENABLE_SSL)
        if (s_enableSSL) {
            sslClient = new SecureHTTPClient<RESTDispatcherType>(address, port);
            if (!sslClient->open()) {
                delete sslClient;
                return ERRNO_SOCK_OPEN;
            }
            sslClient->setHandler(m_dispatcher);
        } else {
#endif // ENABLE_SSL
            client = new HTTPClient<RESTDispatcherType>(address, port);
            if (!client->open()) {
                delete client;
                return ERRNO_SOCK_OPEN;
            }
            client->setHandler(m_dispatcher);
#if defined(ENABLE_SSL)
        }
#endif // ENABLE_SSL

        // send actual API request
        httpPayload = HTTPPayload::requestPayload(method, endpoint);
        httpPayload.headers.add("X-DVM-Auth-Token", token);
        httpPayload.payload(payload);
#if defined(ENABLE_SSL)
        if (s_enableSSL) {
            sslClient->request(httpPayload);
        } else {
#endif // ENABLE_SSL
            client->request(httpPayload);
#if defined(ENABLE_SSL)
        }
#endif // ENABLE_SSL

        // wait for response and parse
        if (wait()) {
#if defined(ENABLE_SSL)
            if (s_enableSSL) {
                sslClient->close();
                delete sslClient;
            } else {
#endif // ENABLE_SSL
                client->close();
                delete client;
#if defined(ENABLE_SSL)
            }
#endif // ENABLE_SSL
            return ERRNO_API_CALL_TIMEOUT;
        }

        response = json::object();
        if (!parseResponseBody(s_response, response)) {
            return ERRNO_BAD_API_RESPONSE;
        }

        ret = response["status"].get<int>();
        if (s_console) {
            fprintf(stdout, "%s\r\n", s_response.content.c_str());
        }
        else {
            if (s_debug) {
                if (s_response.content.size() < 4095) {
                    ::LogDebug(LOG_REST, "REST Response: %s", s_response.content.c_str());
                }
                // bryanb: this will cause REST responses >4095 characters to simply not print...
            }
        }

#if defined(ENABLE_SSL)
        if (s_enableSSL) {
            sslClient->close();
            delete sslClient;
        } else {
#endif // ENABLE_SSL
            client->close();
            delete client;
#if defined(ENABLE_SSL)
        }
#endif // ENABLE_SSL
    }
    catch (std::exception&) {
#if defined(ENABLE_SSL)
        if (s_enableSSL) {
            if (sslClient != nullptr) {
                delete sslClient;
            }
        } else {
#endif // ENABLE_SSL
            if (client != nullptr) {
                delete client;
            }
#if defined(ENABLE_SSL)
        }
#endif // ENABLE_SSL
        return ERRNO_INTERNAL_ERROR;
    }

    return ret;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* HTTP response handler. */

void RESTClient::responseHandler(const HTTPPayload& request, HTTPPayload& reply)
{
    s_responseAvailable = true;
    s_response = request;
}

/* Helper to wait for a HTTP response. */

bool RESTClient::wait(const int t)
{
    s_responseAvailable = false;

    int timeout = t;
    while (!s_responseAvailable && timeout > 0) {
        timeout--;
        Thread::sleep(1);
    }

    if (timeout == 0) {
        return true;
    }

    return false;
}
