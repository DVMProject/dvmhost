// SPDX-License-Identifier: BSL-1.0
/*
 * Digital Voice Modem - Common Library
 * BSL-1.0 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2003-2013 Christopher M. Kohlhoff
 *  Copyright (C) 2023-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SIPPayload.h
 * @ingroup sip
 * @file SIPPayload.cpp
 * @ingroup sip
 */
#if !defined(__SIP__SIP_PAYLOAD_H__)
#define __SIP__SIP_PAYLOAD_H__

#include "common/Defines.h"
#include "common/network/json/json.h"
#include "common/network/sip/SIPHeaders.h"

#include <string>
#include <vector>

#include <asio.hpp>

namespace network
{
    namespace sip
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        #define SIP_INVITE "INVITE"
        #define SIP_ACK "ACK"
        #define SIP_BYE "BYE"
        #define SIP_CANCEL "CANCEL"
        #define SIP_REGISTER "REGISTER"
        #define SIP_OPTIONS "OPTIONS"
        #define SIP_SUBSCRIBE "SUBSCRIBE"
        #define SIP_NOTIFY "NOTIFY"
        #define SIP_PUBLISH "PUBLISH"
        #define SIP_INFO "INFO"
        #define SIP_MESSAGE "MESSAGE"
        #define SIP_UPDATE "UPDATE"

        // ---------------------------------------------------------------------------
        //  Structure Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This struct implements a model of a payload to be sent to a
         *  SIP client/server.
         * @ingroup sip
         */
        struct SIPPayload {
            /**
             * @brief SIP Status/Response Codes
             */
            enum StatusType {
                TRYING = 100,                   //! SIP Trying 100
                RINGING = 180,                  //! SIP Ringing 180

                OK = 200,                       //! SIP OK 200
                ACCEPTED = 202,                 //! SIP Accepted 202
                NO_NOTIFY = 204,                //! SIP No Notification 204
                
                MULTIPLE_CHOICES = 300,         //! SIP Multiple Choices 300
                MOVED_PERMANENTLY = 301,        //! SIP Moved Permenantly 301
                MOVED_TEMPORARILY = 302,        //! SIP Moved Temporarily 302
                
                BAD_REQUEST = 400,              //! SIP Bad Request 400
                UNAUTHORIZED = 401,             //! SIP Unauthorized 401
                FORBIDDEN = 403,                //! SIP Forbidden 403
                NOT_FOUND = 404,                //! SIP Not Found 404
                
                INTERNAL_SERVER_ERROR = 500,    //! SIP Internal Server Error 500
                NOT_IMPLEMENTED = 501,          //! SIP Not Implemented 501
                BAD_GATEWAY = 502,              //! SIP Bad Gateway 502
                SERVICE_UNAVAILABLE = 503,      //! SIP Service Unavailable 503

                BUSY_EVERYWHERE = 600,          //! SIP Busy Everywhere 600
                DECLINE = 603,                  //! SIP Decline 603
            } status;

            SIPHeaders headers;
            std::string content;
            size_t contentLength;

            std::string method;
            std::string uri;

            int sipVersionMajor;
            int sipVersionMinor;

            bool isClientPayload = false;

            /**
             * @brief Convert the payload into a vector of buffers. The buffers do not own the
             *  underlying memory blocks, therefore the payload object must remain valid and
             *  not be changed until the write operation has completed.
             * @returns std::vector<asio::const_buffer> List of buffers representing the SIP payload.
             */
            std::vector<asio::const_buffer> toBuffers();

            /**
             * @brief Prepares payload for transmission by finalizing status and content type.
             * @param obj 
             * @param status SIP status.
             */
            void payload(json::object& obj, StatusType status = OK);
            /**
             * @brief Prepares payload for transmission by finalizing status and content type.
             * @param content 
             * @param status SIP status.
             * @param contentType SIP content type.
             */
            void payload(std::string& content, StatusType status = OK, const std::string& contentType = "application/sdp");

            /**
             * @brief Get a request payload.
             * @param method SIP method.
             * @param uri SIP uri.
             */
            static SIPPayload requestPayload(std::string method, std::string uri);
            /**
             * @brief Get a status payload.
             * @param status SIP status.
             * @param contentType SIP content type.
             */
            static SIPPayload statusPayload(StatusType status, const std::string& contentType = "application/sdp");

            /**
             * @brief Helper to attach a host TCP stream reader.
             * @param remoteEndpoint Endpoint.
             */
            void attachHostHeader(const asio::ip::tcp::endpoint remoteEndpoint);

        private:
            /**
             * @brief Internal helper to ensure the headers are of a default for the given content type.
             * @param contentType SIP content type.
             */
            void ensureDefaultHeaders(const std::string& contentType = "application/sdp");
        };
    } // namespace sip
} // namespace network

#endif // __SIP__SIP_PAYLOAD_H__
