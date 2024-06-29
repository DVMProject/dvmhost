// SPDX-License-Identifier: BSL-1.0
/*
 * Digital Voice Modem - Common Library
 * BSL-1.0 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2003-2013 Christopher M. Kohlhoff
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file HTTPRequestHandler.h
 * @ingroup http
 * @file HTTPRequestHandler.cpp
 * @ingroup http
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
            // ---------------------------------------------------------------------------

            /**
             * @brief This class implements the common handler for all incoming requests.
             * @ingroup http
             */
            class HTTPRequestHandler {
            public:
                auto operator=(HTTPRequestHandler&) -> HTTPRequestHandler& = delete;
                HTTPRequestHandler(HTTPRequestHandler&) = delete;

                /**
                 * @brief Initializes a new instance of the HTTPRequestHandler class.
                 * @param docRoot Path to the document root to serve.
                 */
                explicit HTTPRequestHandler(const std::string& docRoot);
                /**
                 * @brief 
                 */
                HTTPRequestHandler(HTTPRequestHandler&&) = default;

                /**
                 * @brief 
                 */
                HTTPRequestHandler& operator=(HTTPRequestHandler&&) = default;

                /**
                 * @brief Handle a request and produce a reply.
                 * @param req HTTP request.
                 * @param reply HTTP reply.
                 */
                void handleRequest(const HTTPPayload& req, HTTPPayload& reply);

            private:
                /**
                 * @brief Internal helper to decode an incoming URL.
                 * @param[in] in Incoming URL string.
                 * @param[out] out Decoded URL string.
                 * @returns bool True, if URL decoded, otherwise false.
                 */
                static bool urlDecode(const std::string& in, std::string& out);

                std::string m_docRoot;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__HTTP_REQUEST_HANDLER_H__
