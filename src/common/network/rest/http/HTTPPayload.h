// SPDX-License-Identifier: BSL-1.0
/**
* Digital Voice Modem - Common Library
* BSL-1.0 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom CRUD (https://github.com/venediktov/CRUD)
* @license BSL-1.0 License (https://opensource.org/license/bsl1-0-html)
*
*   Copyright (c) 2003-2013 Christopher M. Kohlhoff
*   Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__REST_HTTP__HTTP_REPLY_H__)
#define __REST_HTTP__HTTP_REPLY_H__

#include "common/Defines.h"
#include "common/network/json/json.h"
#include "common/network/rest/http/HTTPHeaders.h"

#include <string>
#include <vector>

#include <asio.hpp>

namespace network
{
    namespace rest
    {
        namespace http
        {

            // ---------------------------------------------------------------------------
            //  Constants
            // ---------------------------------------------------------------------------

            #define HTTP_GET "GET"
            #define HTTP_POST "POST"
            #define HTTP_PUT "PUT"
            #define HTTP_DELETE "DELETE"
            #define HTTP_OPTIONS "OPTIONS"

            // ---------------------------------------------------------------------------
            //  Structure Declaration
            //      This struct implements a model of a payload to be sent to a
            //      HTTP client/server.
            // ---------------------------------------------------------------------------

            struct HTTPPayload {
                /// <summary>
                /// HTTP Status/Response Codes
                /// </summary>
                enum StatusType {
                    OK = 200,
                    CREATED = 201,
                    ACCEPTED = 202,
                    NO_CONTENT = 204,
                    MULTIPLE_CHOICES = 300,
                    MOVED_PERMANENTLY = 301,
                    MOVED_TEMPORARILY = 302,
                    NOT_MODIFIED = 304,
                    BAD_REQUEST = 400,
                    UNAUTHORIZED = 401,
                    FORBIDDEN = 403,
                    NOT_FOUND = 404,
                    INTERNAL_SERVER_ERROR = 500,
                    NOT_IMPLEMENTED = 501,
                    BAD_GATEWAY = 502,
                    SERVICE_UNAVAILABLE = 503
                } status;

                HTTPHeaders headers;
                std::string content;
                size_t contentLength;

                std::string method;
                std::string uri;

                int httpVersionMajor;
                int httpVersionMinor;

                bool isClientPayload = false;

                /// <summary>Convert the payload into a vector of buffers. The buffers do not own the
                /// underlying memory blocks, therefore the payload object must remain valid and
                /// not be changed until the write operation has completed.</summary>
                std::vector<asio::const_buffer> toBuffers();

                /// <summary>Prepares payload for transmission by finalizing status and content type.</summary>
                void payload(json::object& obj, StatusType status = OK);
                /// <summary>Prepares payload for transmission by finalizing status and content type.</summary>
                void payload(std::string& content, StatusType status = OK, const std::string& contentType = "text/html");

                /// <summary>Get a request payload.</summary>
                static HTTPPayload requestPayload(std::string method, std::string uri);
                /// <summary>Get a status payload.</summary>
                static HTTPPayload statusPayload(StatusType status, const std::string& contentType = "text/html");

                /// <summary></summary>
                void attachHostHeader(const asio::ip::tcp::endpoint remoteEndpoint);
            private:
                /// <summary></summary>
                void ensureDefaultHeaders(const std::string& contentType = "text/html");
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__HTTP_REPLY_H__
