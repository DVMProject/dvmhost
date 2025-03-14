// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup sip Session Initiation Protocol Services
 * @brief Implementation for Session Initiation Protocol services.
 * @ingroup network_core
 * 
 * @file RequestDispatcher.h
 * @ingroup sip
 */
#if !defined(__SIP__DISPATCHER_H__)
#define __SIP__DISPATCHER_H__

#include "common/Defines.h"
#include "common/network/sip/SIPPayload.h"
#include "common/Log.h"

#include <functional>
#include <map>
#include <string>
#include <regex>
#include <memory>

namespace network
{
    namespace sip
    {
        // ---------------------------------------------------------------------------
        //  Structure Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Structure representing a request handler.
         * @ingroup rest
         */
        template<typename Request, typename Reply>
        struct RequestHandler {
            typedef std::function<void(const Request&, Reply&)> RequestHandlerType;

            /**
             * @brief Initializes a new instance of the RequestHandler structure.
             */
            explicit RequestHandler() { /* stub */}

            /**
             * @brief Handler for INVITE requests.
             * @param handler INVITE request handler.
             * @return RequestHandler* Instance of a RequestHandler.
             */
            RequestHandler<Request, Reply>& invite(RequestHandlerType handler) {
                m_handlers[SIP_INVITE] = handler;
                return *this;
            }
            /**
             * @brief Handler for ACK requests.
             * @param handler ACK request handler.
             * @return RequestHandler* Instance of a RequestHandler.
             */
            RequestHandler<Request, Reply>& ack(RequestHandlerType handler) {
                m_handlers[SIP_ACK] = handler;
                return *this;
            }
            /**
             * @brief Handler for BYE requests.
             * @param handler BYE request handler.
             * @return RequestHandler* Instance of a RequestHandler.
             */
            RequestHandler<Request, Reply>& bye(RequestHandlerType handler) {
                m_handlers[SIP_BYE] = handler;
                return *this;
            }
            /**
             * @brief Handler for CANCEL requests.
             * @param handler CANCEL request handler.
             * @return RequestHandler* Instance of a RequestHandler.
             */
            RequestHandler<Request, Reply>& cancel(RequestHandlerType handler) {
                m_handlers[SIP_CANCEL] = handler;
                return *this;
            }
            /**
             * @brief Handler for REGISTER requests.
             * @param handler REGISTER request handler.
             * @return RequestHandler* Instance of a RequestHandler.
             */
            RequestHandler<Request, Reply>& registerReq(RequestHandlerType handler) {
                m_handlers[SIP_REGISTER] = handler;
                return *this;
            }
            /**
             * @brief Handler for OPTIONS requests.
             * @param handler OPTIONS request handler.
             * @return RequestHandler* Instance of a RequestHandler.
             */
            RequestHandler<Request, Reply>& options(RequestHandlerType handler) {
                m_handlers[SIP_OPTIONS] = handler;
                return *this;
            }
            /**
             * @brief Handler for MESSAGE requests.
             * @param handler MESSAGE request handler.
             * @return RequestHandler* Instance of a RequestHandler.
             */
            RequestHandler<Request, Reply>& message(RequestHandlerType handler) {
                m_handlers[SIP_MESSAGE] = handler;
                return *this;
            }

            /**
             * @brief Helper to handle the actual request.
             * @param request HTTP request.
             * @param reply HTTP reply.
             */
            void handleRequest(const Request& request, Reply& reply) {
                // dispatching to handler
                auto& handler = m_handlers[request.method];
                if (handler) {
                    handler(request, reply);
                }
            }

        private:
            std::map<std::string, RequestHandler> m_handlers;
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements SIP request dispatching.
         * @tparam Request SIP request.
         * @tparam Reply SIP reply.
         */
        template<typename Request = SIPPayload, typename Reply = SIPPayload>
        class SIPRequestDispatcher {
            typedef RequestHandler<Request, Reply> HandlerType;
        public:
            /**
             * @brief Initializes a new instance of the SIPRequestDispatcher class.
             */
            SIPRequestDispatcher() : m_handlers(), m_debug(false) { /* stub */ }
            /**
             * @brief Initializes a new instance of the SIPRequestDispatcher class.
             * @param debug Flag indicating whether or not verbose logging should be enabled.
             */
            SIPRequestDispatcher(bool debug) : m_handlers(), m_debug(debug) { /* stub */ }

            /**
             * @brief Helper to set a request handler.
             * @returns HandlerType Instance of a request handler.
             */
            HandlerType& handler()
            {
                HandlerTypePtr& p = m_handlers;
                return *p;
            }

            /**
             * @brief Helper to handle SIP request.
             * @param request SIP request.
             * @param reply SIP reply.
             */
            void handleRequest(const Request& request, Reply& reply)
            {
                m_handlers.handleRequest(request, reply);
                return;
            }

        private:
            typedef std::shared_ptr<HandlerType> HandlerTypePtr;
            HandlerType m_handlers;

            bool m_debug;
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements a generic debug request dispatcher.
         * @tparam Request SIP request.
         * @tparam Reply SIP reply.
         */
        template<typename Request = SIPPayload, typename Reply = SIPPayload>
        class SIPDebugRequestDispatcher {
        public:
            /**
             * @brief Initializes a new instance of the SIPDebugRequestDispatcher class.
             */
            SIPDebugRequestDispatcher() { /* stub */ }

            /**
             * @brief Helper to handle SIP request.
             * @param request SIP request.
             * @param reply SIP reply.
             */
            void handleRequest(const Request& request, Reply& reply)
            {
                for (auto header : request.headers.headers())
                    ::LogDebugEx(LOG_SIP, "SIPDebugRequestDispatcher::handleRequest()", "header = %s, value = %s", header.name.c_str(), header.value.c_str());

                ::LogDebugEx(LOG_SIP, "SIPDebugRequestDispatcher::handleRequest()", "content = %s", request.content.c_str());
            }
        };

        typedef SIPRequestDispatcher<SIPPayload, SIPPayload> DefaultSIPRequestDispatcher;
    } // namespace sip
} // namespace network

#endif // __SIP__DISPATCHER_H__
