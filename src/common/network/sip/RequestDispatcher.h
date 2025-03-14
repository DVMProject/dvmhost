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
         * @brief Structure representing a SIP request match.
         * @ingroup sip
         */
        struct RequestMatch : std::smatch {
            /**
             * @brief Initializes a new instance of the RequestMatch structure.
             * @param m String matcher.
             * @param c Content.
             */
            RequestMatch(const std::smatch& m, const std::string& c) : std::smatch(m), content(c) { /* stub */ }

            std::string content;
        };

        // ---------------------------------------------------------------------------
        //  Structure Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Structure representing a request matcher.
         * @ingroup rest
         */
        template<typename Request, typename Reply>
        struct RequestMatcher {
            typedef std::function<void(const Request&, Reply&, const RequestMatch&)> RequestHandlerType;

            /**
             * @brief Initializes a new instance of the RequestMatcher structure.
             * @param expression Matching expression.
             */
            explicit RequestMatcher(const std::string& expression) : m_expression(expression), m_isRegEx(false) { /* stub */ }

            /**
             * @brief Handler for INVITE requests.
             * @param handler INVITE request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& invite(RequestHandlerType handler) {
                m_handlers[SIP_INVITE] = handler;
                return *this;
            }
            /**
             * @brief Handler for ACK requests.
             * @param handler ACK request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& ack(RequestHandlerType handler) {
                m_handlers[SIP_ACK] = handler;
                return *this;
            }
            /**
             * @brief Handler for BYE requests.
             * @param handler BYE request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& bye(RequestHandlerType handler) {
                m_handlers[SIP_BYE] = handler;
                return *this;
            }
            /**
             * @brief Handler for CANCEL requests.
             * @param handler CANCEL request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& cancel(RequestHandlerType handler) {
                m_handlers[SIP_CANCEL] = handler;
                return *this;
            }
            /**
             * @brief Handler for REGISTER requests.
             * @param handler REGISTER request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& registerReq(RequestHandlerType handler) {
                m_handlers[SIP_REGISTER] = handler;
                return *this;
            }
            /**
             * @brief Handler for OPTIONS requests.
             * @param handler OPTIONS request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& options(RequestHandlerType handler) {
                m_handlers[SIP_OPTIONS] = handler;
                return *this;
            }
            /**
             * @brief Handler for SUBSCRIBE requests.
             * @param handler SUBSCRIBE request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& subscribe(RequestHandlerType handler) {
                m_handlers[SIP_SUBSCRIBE] = handler;
                return *this;
            }
            /**
             * @brief Handler for NOTIFY requests.
             * @param handler NOTIFY request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& notify(RequestHandlerType handler) {
                m_handlers[SIP_NOTIFY] = handler;
                return *this;
            }
            /**
             * @brief Handler for PUBLISH requests.
             * @param handler PUBLISH request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& publish(RequestHandlerType handler) {
                m_handlers[SIP_PUBLISH] = handler;
                return *this;
            }
            /**
             * @brief Handler for INFO requests.
             * @param handler INFO request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& info(RequestHandlerType handler) {
                m_handlers[SIP_INFO] = handler;
                return *this;
            }
            /**
             * @brief Handler for MESSAGE requests.
             * @param handler MESSAGE request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& message(RequestHandlerType handler) {
                m_handlers[SIP_MESSAGE] = handler;
                return *this;
            }
            /**
             * @brief Handler for UPDATE requests.
             * @param handler UPDATE request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& update(RequestHandlerType handler) {
                m_handlers[SIP_UPDATE] = handler;
                return *this;
            }

            /**
             * @brief Helper to determine if the request matcher is a regular expression.
             * @returns bool True, if request matcher is a regular expression, otherwise false.
             */
            bool regex() const { return m_isRegEx; }
            /**
             * @brief Helper to set the regular expression flag.
             * @param regEx Flag indicating whether or not the request matcher is a regular expression.
             */
            void setRegEx(bool regEx) { m_isRegEx = regEx; }

            /**
             * @brief Helper to handle the actual request.
             * @param request HTTP request.
             * @param reply HTTP reply.
             * @param what What matched.
             */
            void handleRequest(const Request& request, Reply& reply, const std::smatch &what) {
                // dispatching to matching based on handler
                RequestMatch match(what, request.content);
                auto& handler = m_handlers[request.method];
                if (handler) {
                    handler(request, reply, match);
                }
            }

        private:
            std::string m_expression;
            bool m_isRegEx;
            std::map<std::string, RequestHandlerType> m_handlers;
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
            typedef RequestMatcher<Request, Reply> MatcherType;
        public:
            /**
             * @brief Initializes a new instance of the SIPRequestDispatcher class.
             */
            SIPRequestDispatcher() : m_basePath(), m_debug(false) { /* stub */ }
            /**
             * @brief Initializes a new instance of the SIPRequestDispatcher class.
             * @param debug Flag indicating whether or not verbose logging should be enabled.
             */
            SIPRequestDispatcher(bool debug) : m_basePath(), m_debug(debug) { /* stub */ }
            /**
             * @brief Initializes a new instance of the SIPRequestDispatcher class.
             * @param basePath 
             * @param debug Flag indicating whether or not verbose logging should be enabled.
             */
            SIPRequestDispatcher(const std::string& basePath, bool debug) : m_basePath(basePath), m_debug(debug) { /* stub */ }

            /**
             * @brief Helper to match a request patch.
             * @param expression Matching expression.
             * @param regex Flag indicating whether or not this match is a regular expression.
             * @returns MatcherType Instance of a request matcher.
             */
            MatcherType& match(const std::string& expression, bool regex = false)
            {
                MatcherTypePtr& p = m_matchers[expression];
                if (!p) {
                    if (m_debug) {
                        ::LogDebug(LOG_REST, "creating SIPRequestDispatcher, expression = %s", expression.c_str());
                    }
                    p = std::make_shared<MatcherType>(expression);
                } else {
                    if (m_debug) {
                        ::LogDebug(LOG_REST, "fetching SIPRequestDispatcher, expression = %s", expression.c_str());
                    }
                }

                p->setRegEx(regex);
                return *p;
            }

            /**
             * @brief Helper to handle SIP request.
             * @param request SIP request.
             * @param reply SIP reply.
             */
            void handleRequest(const Request& request, Reply& reply)
            {
                for (const auto& matcher : m_matchers) {
                    std::smatch what;
                    if (!matcher.second->regex()) {
                        if (request.uri.find(matcher.first) != std::string::npos) {
                            if (m_debug) {
                                ::LogDebug(LOG_REST, "non-regex endpoint, uri = %s, expression = %s", request.uri.c_str(), matcher.first.c_str());
                            }

                            //what = matcher.first;
                            
                            matcher.second->handleRequest(request, reply, what);
                            return;
                        }
                    } else {
                        if (std::regex_match(request.uri, what, std::regex(matcher.first))) {
                            if (m_debug) {
                                ::LogDebug(LOG_REST, "regex endpoint, uri = %s, expression = %s", request.uri.c_str(), matcher.first.c_str());
                            }

                            matcher.second->handleRequest(request, reply, what);
                            return;
                        }
                    }
                }

                ::LogError(LOG_REST, "unknown endpoint, uri = %s", request.uri.c_str());
                reply = SIPPayload::statusPayload(SIPPayload::BAD_REQUEST, "application/sdp");
            }

        private:
            typedef std::shared_ptr<MatcherType> MatcherTypePtr;

            std::string m_basePath;
            std::map<std::string, MatcherTypePtr> m_matchers;

            bool m_debug;
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements a generic basic request dispatcher.
         * @tparam Request SIP request.
         * @tparam Reply SIP reply.
         */
        template<typename Request = SIPPayload, typename Reply = SIPPayload>
        class SIPBasicRequestDispatcher {
        public:
            typedef std::function<void(const Request&, Reply&)> RequestHandlerType;

            /**
             * @brief Initializes a new instance of the SIPBasicRequestDispatcher class.
             */
            SIPBasicRequestDispatcher() { /* stub */ }
            /**
             * @brief Initializes a new instance of the SIPBasicRequestDispatcher class.
             * @param handler Instance of a RequestHandlerType for this dispatcher.
             */
            SIPBasicRequestDispatcher(RequestHandlerType handler) : m_handler(handler) { /* stub */ }

            /**
             * @brief Helper to handle SIP request.
             * @param request SIP request.
             * @param reply SIP reply.
             */
            void handleRequest(const Request& request, Reply& reply)
            {
                if (m_handler) {
                    m_handler(request, reply);
                }
            }

        private:
            RequestHandlerType m_handler;
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
