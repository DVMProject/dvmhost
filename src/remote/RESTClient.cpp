/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2023 by Bryan Biedenkapp <gatekeep@gmail.com>
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "edac/SHA256.h"
#include "network/json/json.h"
#include "network/rest/http/HTTPClient.h"
#include "network/rest/RequestDispatcher.h"
#include "remote/RESTClient.h"
#include "Thread.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::rest::http;

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
#define ERRNO_INTERNAL_ERROR 100

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

bool RESTClient::m_responseAvailable = false;
HTTPPayload RESTClient::m_response;

bool RESTClient::m_console = false;
bool RESTClient::m_debug = false;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="response"></param>
/// <param name="obj"></param>
/// <returns></returns>
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

/// <summary>
/// Initializes a new instance of the RESTClient class.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="password">Authentication password.</param>
/// <param name="debug">Flag indicating whether debug is enabled.</param>
RESTClient::RESTClient(const std::string& address, uint32_t port, const std::string& password, bool debug) :
    m_address(address),
    m_port(port),
    m_password(password)
{
    assert(!address.empty());
    assert(port > 0U);

    m_console = true;
    m_debug = debug;
}

/// <summary>
/// Finalizes a instance of the RESTClient class.
/// </summary>
RESTClient::~RESTClient()
{
    /* stub */
}

/// <summary>
/// Sends remote control command to the specified modem.
/// </summary>
/// <param name="method">REST API method.</param>
/// <param name="endpoint">REST API endpoint.</param>
/// <param name="payload">REST API endpoint payload.</param>
/// <returns>EXIT_SUCCESS, if command was sent, otherwise EXIT_FAILURE.</returns>
int RESTClient::send(const std::string method, const std::string endpoint, json::object payload)
{
    assert(!m_address.empty());
    assert(m_port > 0U);

    return send(m_address, m_port, m_password, method, endpoint, payload, m_debug);
}

/// <summary>
/// Sends remote control command to the specified modem.
/// </summary>
/// <param name="address">Network Hostname/IP address to connect to.</param>
/// <param name="port">Network port number.</param>
/// <param name="password">Authentication password.</param>
/// <param name="method">REST API method.</param>
/// <param name="endpoint">REST API endpoint.</param>
/// <param name="payload">REST API endpoint payload.</param>
/// <param name="debug">Flag indicating whether debug is enabled.</param>
/// <returns>EXIT_SUCCESS, if command was sent, otherwise EXIT_FAILURE.</returns>
int RESTClient::send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
    const std::string endpoint, json::object payload, bool debug)
{
    assert(!address.empty());
    assert(port > 0U);
    assert(password.empty());

    int ret = EXIT_SUCCESS;
    m_debug = debug;

    typedef network::rest::BasicRequestDispatcher<network::rest::http::HTTPPayload, network::rest::http::HTTPPayload> RESTDispatcherType;
    RESTDispatcherType m_dispatcher(RESTClient::responseHandler);
    HTTPClient<RESTDispatcherType>* client = nullptr;

    try {
        // setup HTTP client for authentication payload
        client = new HTTPClient<RESTDispatcherType>(address, port);
        if (!client->open()) {
            delete client;
            return ERRNO_SOCK_OPEN;
        }
        client->setHandler(m_dispatcher);

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
        client->request(httpPayload);

        // wait for response and parse
        if (wait()) {
            client->close();
            delete client;
            return ERRNO_API_CALL_TIMEOUT;
        }

        json::object rsp = json::object();
        if (!parseResponseBody(m_response, rsp)) {
            return ERRNO_BAD_API_RESPONSE;
        }

        std::string token = "";
        int status = rsp["status"].get<int>();
        if (status == HTTPPayload::StatusType::OK) {
            token = rsp["token"].get<std::string>();
        }
        else {
            client->close();
            delete client;
            return ERRNO_BAD_API_RESPONSE;
        }

        client->close();
        delete client;

        // reset the HTTP client and setup for actual payload request
        client = new HTTPClient<RESTDispatcherType>(address, port);
        if (!client->open())
            return ERRNO_SOCK_OPEN;
        client->setHandler(m_dispatcher);

        // send actual API request
        httpPayload = HTTPPayload::requestPayload(method, endpoint);
        httpPayload.headers.add("X-DVM-Auth-Token", token);
        httpPayload.payload(payload);
        client->request(httpPayload);

        // wait for response and parse
        if (wait()) {
            client->close();
            delete client;
            return ERRNO_API_CALL_TIMEOUT;
        }

        rsp = json::object();
        if (!parseResponseBody(m_response, rsp)) {
            return ERRNO_BAD_API_RESPONSE;
        }

        ret = rsp["status"].get<int>();
        if (m_console) {
            fprintf(stdout, "%s\r\n", m_response.content.c_str());
        }
        else {
            if (m_debug) {
                ::LogDebug(LOG_REST, "REST Response: %s", m_response.content.c_str());
            }
        }

        client->close();
        delete client;
    }
    catch (std::exception&) {
        if (client != nullptr) {
            delete client;
        }
        return ERRNO_INTERNAL_ERROR;
    }

    return ret;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="request"></param>
/// <param name="reply"></param>
void RESTClient::responseHandler(const HTTPPayload& request, HTTPPayload& reply)
{
    m_responseAvailable = true;
    m_response = request;
}

/// <summary>
///
/// </summary>
bool RESTClient::wait()
{
    m_responseAvailable = false;

    int timeout = 500;
    while (!m_responseAvailable && timeout > 0) {
        timeout--;
        Thread::sleep(1);
    }

    if (timeout == 0) {
        return true;
    }

    return false;
}
