/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
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
#if !defined(__REST_CLIENT_H__)
#define __REST_CLIENT_H__

#include "Defines.h"
#include "common/network/json/json.h"
#include "common/network/rest/http/HTTPPayload.h"

#include <string>

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the REST client logic.
// ---------------------------------------------------------------------------

class HOST_SW_API RESTClient
{
public:
    /// <summary>Initializes a new instance of the RESTClient class.</summary>
    RESTClient(const std::string& address, uint32_t port, const std::string& password, bool debug);
    /// <summary>Finalizes a instance of the RESTClient class.</summary>
    ~RESTClient();

    /// <summary>Sends remote control command to the specified modem.</summary>
    int send(const std::string method, const std::string endpoint, json::object payload);
    /// <summary>Sends remote control command to the specified modem.</summary>
    int send(const std::string method, const std::string endpoint, json::object payload, json::object& response);

    /// <summary>Sends remote control command to the specified modem.</summary>
    static int send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
        const std::string endpoint, json::object payload, bool debug = false);
    /// <summary>Sends remote control command to the specified modem.</summary>
    static int send(const std::string& address, uint32_t port, const std::string& password, const std::string method,
        const std::string endpoint, json::object payload, json::object& response, bool debug = false);

private:
    typedef network::rest::http::HTTPPayload HTTPPayload;
    /// <summary></summary>
    static void responseHandler(const HTTPPayload& request, HTTPPayload& reply);

    /// <summary></summary>
    static bool wait();

    std::string m_address;
    uint32_t m_port;
    std::string m_password;

    static bool m_console;

    static bool m_responseAvailable;
    static HTTPPayload m_response;

    static bool m_debug;
};

#endif // __REMOTE_COMMAND_H__
