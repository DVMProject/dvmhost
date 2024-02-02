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
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__REST_HTTP__HTTP_REQUEST_HANDLER_H__)
#define __REST_HTTP__HTTP_REQUEST_HANDLER_H__

#include "common/Defines.h"

#include <string>

namespace network
{
    namespace rest
    {
        namespace http
        {

            // ---------------------------------------------------------------------------
            //  Class Prototypes
            // ---------------------------------------------------------------------------

            struct HTTPPayload;

            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      This class implements the common handler for all incoming requests.
            // ---------------------------------------------------------------------------

            class HTTPRequestHandler {
            public:
                auto operator=(HTTPRequestHandler&) -> HTTPRequestHandler& = delete;
                HTTPRequestHandler(HTTPRequestHandler&) = delete;

                /// <summary>Initializes a new instance of the HTTPRequestHandler class.</summary>
                explicit HTTPRequestHandler(const std::string& docRoot);
                /// <summary></summary>
                HTTPRequestHandler(HTTPRequestHandler&&) = default;

                /// <summary></summary>
                HTTPRequestHandler& operator=(HTTPRequestHandler&&) = default;

                /// <summary>Handle a request and produce a reply.</summary>
                void handleRequest(const HTTPPayload& req, HTTPPayload& reply);

            private:
                /// <summary>Perform URL-decoding on a string. Returns false if the encoding was
                /// invalid.</summary>
                static bool urlDecode(const std::string& in, std::string& out);

                std::string m_docRoot;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__HTTP_REQUEST_HANDLER_H__
