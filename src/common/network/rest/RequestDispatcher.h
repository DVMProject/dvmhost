// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup rest REST Services
 * @brief Implementation for REST services.
 * @ingroup network_core
 * @defgroup http Embedded HTTP Core
 * @brief Implementation for basic HTTP services.
 * @ingroup rest
 * 
 * @file RequestDispatcher.h
 * @ingroup tcp_socket
 */
#if !defined(__REST__DISPATCHER_H__)
#define __REST__DISPATCHER_H__

#include "common/Defines.h"
#include "common/network/rest/http/HTTPPayload.h"
#include "common/Log.h"

#include <functional>
#include <map>
#include <string>
#include <regex>
#include <memory>

namespace network
{
    namespace rest
    {
        // ---------------------------------------------------------------------------
        //  Structure Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Structure representing a REST API request match.
         * @ingroup rest
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
             * @brief Handler for GET requests.
             * @param handler GET request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& get(RequestHandlerType handler) {
                m_handlers[HTTP_GET] = handler;
                return *this;
            }
            /**
             * @brief Handler for POST requests.
             * @param handler POST request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& post(RequestHandlerType handler) {
                m_handlers[HTTP_POST] = handler;
                return *this;
            }
            /**
             * @brief Handler for PUT requests.
             * @param handler PUT request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& put(RequestHandlerType handler) {
                m_handlers[HTTP_PUT] = handler;
                return *this;
            }
            /**
             * @brief Handler for DELETE requests.
             * @param handler DELETE request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& del(RequestHandlerType handler) {
                m_handlers[HTTP_DELETE] = handler;
                return *this;
            }
            /**
             * @brief Handler for OPTIONS requests.
             * @param handler OPTIONS request handler.
             * @return RequestMatcher* Instance of a RequestMatcher.
             */
            RequestMatcher<Request, Reply>& options(RequestHandlerType handler) {
                m_handlers[HTTP_OPTIONS] = handler;
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
         * @brief This class implements RESTful web request dispatching.
         * @tparam Request HTTP request.
         * @tparam Reply HTTP reply.
         */
        template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
        class RequestDispatcher {
            typedef RequestMatcher<Request, Reply> MatcherType;
        public:
            /**
             * @brief Initializes a new instance of the RequestDispatcher class.
             */
            RequestDispatcher() : m_basePath(), m_debug(false) { /* stub */ }
            /**
             * @brief Initializes a new instance of the RequestDispatcher class.
             * @param debug Flag indicating whether or not verbose logging should be enabled.
             */
            RequestDispatcher(bool debug) : m_basePath(), m_debug(debug) { /* stub */ }
            /**
             * @brief Initializes a new instance of the RequestDispatcher class.
             * @param basePath 
             * @param debug Flag indicating whether or not verbose logging should be enabled.
             */
            RequestDispatcher(const std::string& basePath, bool debug) : m_basePath(basePath), m_debug(debug) { /* stub */ }

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
                        ::LogDebug(LOG_REST, "creating RequestDispatcher, expression = %s", expression.c_str());
                    }
                    p = std::make_shared<MatcherType>(expression);
                } else {
                    if (m_debug) {
                        ::LogDebug(LOG_REST, "fetching RequestDispatcher, expression = %s", expression.c_str());
                    }
                }

                p->setRegEx(regex);
                return *p;
            }

            /**
             * @brief Helper to handle HTTP request.
             * @param request HTTP request.
             * @param reply HTTP reply.
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

                            // ensure CORS headers are added
                            reply.headers.add("Access-Control-Allow-Origin", "*");
                            reply.headers.add("Access-Control-Allow-Methods", "*");
                            reply.headers.add("Access-Control-Allow-Headers", "*");
                            
                            if (request.method == HTTP_OPTIONS) {
                                reply.status = http::HTTPPayload::OK;
                            }
                            
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
                reply = http::HTTPPayload::statusPayload(http::HTTPPayload::BAD_REQUEST, "application/json");
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
         * @tparam Request HTTP request.
         * @tparam Reply HTTP reply.
         */
        template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
        class BasicRequestDispatcher {
        public:
            typedef std::function<void(const Request&, Reply&)> RequestHandlerType;

            /**
             * @brief Initializes a new instance of the DebugRequestDispatcher class.
             */
            BasicRequestDispatcher() { /* stub */ }
            /**
             * @brief Initializes a new instance of the BasicRequestDispatcher class.
             * @param handler Instance of a RequestHandlerType for this dispatcher.
             */
            BasicRequestDispatcher(RequestHandlerType handler) : m_handler(handler) { /* stub */ }

            /**
             * @brief Helper to handle HTTP request.
             * @param request HTTP request.
             * @param reply HTTP reply.
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
         * @tparam Request HTTP request.
         * @tparam Reply HTTP reply.
         */
        template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
        class DebugRequestDispatcher {
        public:
            /**
             * @brief Initializes a new instance of the DebugRequestDispatcher class.
             */
            DebugRequestDispatcher() { /* stub */ }

            /**
             * @brief Helper to handle HTTP request.
             * @param request HTTP request.
             * @param reply HTTP reply.
             */
            void handleRequest(const Request& request, Reply& reply)
            {
                for (auto header : request.headers.headers())
                    ::LogDebug(LOG_REST, "DebugRequestDispatcher::handleRequest() header = %s, value = %s", header.name.c_str(), header.value.c_str());

                ::LogDebug(LOG_REST, "DebugRequestDispatcher::handleRequest() content = %s", request.content.c_str());
            }
        };

        typedef RequestDispatcher<http::HTTPPayload, http::HTTPPayload> DefaultRequestDispatcher;
    } // namespace rest
} // namespace network

#endif // __REST__DISPATCHER_H__
