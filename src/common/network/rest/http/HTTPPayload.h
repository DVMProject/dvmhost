// SPDX-License-Identifier: BSL-1.0
/*
 * Digital Voice Modem - Common Library
 * BSL-1.0 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2003-2013 Christopher M. Kohlhoff
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file HTTPPayload.h
 * @ingroup http
 * @file HTTPPayload.cpp
 * @ingroup http
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
            // ---------------------------------------------------------------------------

            /**
             * @brief This struct implements a model of a payload to be sent to a
             *  HTTP client/server.
             * @ingroup http
             */
            struct HTTPPayload {
                /**
                 * @brief HTTP Status/Response Codes
                 */
                enum StatusType {
                    OK = 200,                       //! HTTP OK 200
                    CREATED = 201,                  //! HTTP Created 201
                    ACCEPTED = 202,                 //! HTTP Accepted 202
                    NO_CONTENT = 204,               //! HTTP No Content 204
                    
                    MULTIPLE_CHOICES = 300,         //! HTTP Multiple Choices 300
                    MOVED_PERMANENTLY = 301,        //! HTTP Moved Permenantly 301
                    MOVED_TEMPORARILY = 302,        //! HTTP Moved Temporarily 302
                    NOT_MODIFIED = 304,             //! HTTP Not Modified 304
                    
                    BAD_REQUEST = 400,              //! HTTP Bad Request 400
                    UNAUTHORIZED = 401,             //! HTTP Unauthorized 401
                    FORBIDDEN = 403,                //! HTTP Forbidden 403
                    NOT_FOUND = 404,                //! HTTP Not Found 404
                    
                    INTERNAL_SERVER_ERROR = 500,    //! HTTP Internal Server Error 500
                    NOT_IMPLEMENTED = 501,          //! HTTP Not Implemented 501
                    BAD_GATEWAY = 502,              //! HTTP Bad Gateway 502
                    SERVICE_UNAVAILABLE = 503       //! HTTP Service Unavailable 503
                } status;

                HTTPHeaders headers;
                std::string content;
                size_t contentLength;

                std::string method;
                std::string uri;

                int httpVersionMajor;
                int httpVersionMinor;

                bool isClientPayload = false;

                /**
                 * @brief Convert the payload into a vector of buffers. The buffers do not own the
                 *  underlying memory blocks, therefore the payload object must remain valid and
                 *  not be changed until the write operation has completed.
                 * @returns std::vector<asio::const_buffer> List of buffers representing the HTTP payload.
                 */
                std::vector<asio::const_buffer> toBuffers();

                /**
                 * @brief Prepares payload for transmission by finalizing status and content type.
                 * @param obj 
                 * @param status HTTP status.
                 */
                void payload(json::object& obj, StatusType status = OK);
                /**
                 * @brief Prepares payload for transmission by finalizing status and content type.
                 * @param content 
                 * @param status HTTP status.
                 * @param contentType HTTP content type.
                 */
                void payload(std::string& content, StatusType status = OK, const std::string& contentType = "text/html");

                /**
                 * @brief Get a request payload.
                 * @param method HTTP method.
                 * @param uri HTTP uri.
                 */
                static HTTPPayload requestPayload(std::string method, std::string uri);
                /**
                 * @brief Get a status payload.
                 * @param status HTTP status.
                 * @param contentType HTTP content type.
                 */
                static HTTPPayload statusPayload(StatusType status, const std::string& contentType = "text/html");

                /**
                 * @brief Helper to attach a host TCP stream reader.
                 * @param remoteEndpoint Endpoint.
                 */
                void attachHostHeader(const asio::ip::tcp::endpoint remoteEndpoint);

            private:
                /**
                 * @brief Internal helper to ensure the headers are of a default for the given content type.
                 * @param contentType HTTP content type.
                 */
                void ensureDefaultHeaders(const std::string& contentType = "text/html");
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__HTTP_REPLY_H__
