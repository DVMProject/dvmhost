// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Remote Command Client
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Remote Command Client
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @defgroup remote_rest REST Client
 * @brief Implementation for the remote command REST client.
 * @ingroup remote
 * 
 * @file RESTClient.h
 * @ingroup remote_rest
 * @file RESTClient.cpp
 * @ingroup remote_rest
 */
#if !defined(__REST_CLIENT_H__)
#define __REST_CLIENT_H__

#include "Defines.h"
#include "common/network/json/json.h"
#include "common/network/rest/http/HTTPPayload.h"

#include <string>

#define REST_DEFAULT_WAIT 500
#define REST_QUICK_WAIT 150

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the REST client logic.
 * @ingroup remote_rest
 */
class HOST_SW_API RESTClient
{
public:
    /**
     * @brief Initializes a new instance of the RESTClient class.
     * @param address Network Hostname/IP address to connect to.
     * @param port Network port number.
     * @param password Authentication password.
     * @param enableSSL Flag indicating whether or not HTTPS is enabled.
     * @param debug Flag indicating whether debug is enabled.
     */
    RESTClient(const std::string& address, uint32_t port, const std::string& password, bool enableSSL, bool debug);
    /**
     * @brief Finalizes a instance of the RESTClient class.
     */
    ~RESTClient();

    /**
     * @brief Sends remote control command to the specified modem.
     * @param method REST API method.
     * @param endpoint REST API endpoint.
     * @param payload REST API endpoint payload.
     * @return EXIT_SUCCESS, if command was sent, otherwise EXIT_FAILURE.
     */
    int send(const std::string method, const std::string endpoint, json::object payload);
    /**
     * @brief Sends remote control command to the specified modem.
     * @param method REST API method.
     * @param endpoint REST API endpoint.
     * @param payload REST API endpoint payload.
     * @param response REST API endpoint response.
     * @returns EXIT_SUCCESS, if command was sent, otherwise EXIT_FAILURE.
     */
    int send(const std::string method, const std::string endpoint, json::object payload, json::object& response);

    /**
     * @brief Sends remote control command to the specified modem.
     * @param address Network Hostname/IP address to connect to.
     * @param port Network port number.
     * @param password Authentication password.
     * @param method REST API method.
     * @param endpoint REST API endpoint.
     * @param payload REST API endpoint payload.
     * @param enableSSL Flag indicating whether or not HTTPS is enabled.
     * @param debug Flag indicating whether debug is enabled.
     * @returns EXIT_SUCCESS, if command was sent, otherwise EXIT_FAILURE.
     */
    static int send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
        const std::string endpoint, json::object payload, bool enableSSL, int timeout = REST_DEFAULT_WAIT, bool debug = false);
    /**
     * @brief Sends remote control command to the specified modem.
     * @param address Network Hostname/IP address to connect to.
     * @param port Network port number.
     * @param password Authentication password.
     * @param method REST API method.
     * @param endpoint REST API endpoint.
     * @param payload REST API endpoint payload.
     * @param response REST API endpoint response.
     * @param enableSSL Flag indicating whether or not HTTPS is enabled.
     * @param timeout REST response wait timeout.
     * @param debug Flag indicating whether debug is enabled.
     * @returns EXIT_SUCCESS, if command was sent, otherwise EXIT_FAILURE.
     */
    static int send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
        const std::string endpoint, json::object payload, json::object& response, bool enableSSL, int timeout, bool debug = false);

private:
    typedef network::rest::http::HTTPPayload HTTPPayload;
    /**
     * @brief HTTP response handler.
     * @param request HTTP request.
     * @param reply HTTP reply.
     */
    static void responseHandler(const HTTPPayload& request, HTTPPayload& reply);

    /**
     * @brief Helper to wait for a HTTP response.
     * @returns True, if timed out, otherwise false.
     */
    static bool wait(const int t = REST_DEFAULT_WAIT);

    std::string m_address;
    uint32_t m_port;
    std::string m_password;

    static bool m_console;

    static bool m_responseAvailable;
    static HTTPPayload m_response;

    static bool m_enableSSL;
    static bool m_debug;
};

#endif // __REMOTE_COMMAND_H__
