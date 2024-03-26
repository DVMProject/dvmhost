// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Remote Command Client
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Remote Command Client
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
*
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
//      This class implements the REST client logic.
// ---------------------------------------------------------------------------

class HOST_SW_API RESTClient
{
public:
    /// <summary>Initializes a new instance of the RESTClient class.</summary>
    RESTClient(const std::string& address, uint32_t port, const std::string& password, bool enableSSL, bool debug);
    /// <summary>Finalizes a instance of the RESTClient class.</summary>
    ~RESTClient();

    /// <summary>Sends remote control command to the specified modem.</summary>
    int send(const std::string method, const std::string endpoint, json::object payload);
    /// <summary>Sends remote control command to the specified modem.</summary>
    int send(const std::string method, const std::string endpoint, json::object payload, json::object& response);

    /// <summary>Sends remote control command to the specified modem.</summary>
    static int send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
        const std::string endpoint, json::object payload, bool enableSSL, int timeout = REST_DEFAULT_WAIT, bool debug = false);
    /// <summary>Sends remote control command to the specified modem.</summary>
    static int send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
        const std::string endpoint, json::object payload, json::object& response, bool enableSSL, int timeout, bool debug = false);

private:
    typedef network::rest::http::HTTPPayload HTTPPayload;
    /// <summary></summary>
    static void responseHandler(const HTTPPayload& request, HTTPPayload& reply);

    /// <summary></summary>
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
