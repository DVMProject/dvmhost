// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
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
        //
        // ---------------------------------------------------------------------------

        struct RequestMatch : std::smatch {
            /// <summary>Initializes a new instance of the RequestMatch structure.</summary>
            /// <param name="m"></param>
            /// <param name="c"></param>
            RequestMatch(const std::smatch& m, const std::string& c) : std::smatch(m), content(c) { /* stub */ }

            std::string content;
        };

        // ---------------------------------------------------------------------------
        //  Structure Declaration
        //
        // ---------------------------------------------------------------------------

        template<typename Request, typename Reply>
        struct RequestMatcher {
            typedef std::function<void(const Request&, Reply&, const RequestMatch&)> RequestHandlerType;

            /// <summary>Initializes a new instance of the RequestMatcher structure.</summary>
            /// <param name="expression"></param>
            explicit RequestMatcher(const std::string& expression) : m_expression(expression), m_isRegEx(false) { /* stub */ }

            /// <summary></summary>
            /// <param name="handler"></param>
            /// <returns></returns>
            RequestMatcher<Request, Reply>& get(RequestHandlerType handler) {
                m_handlers[HTTP_GET] = handler;
                return *this;
            }
            /// <summary></summary>
            /// <param name="handler"></param>
            /// <returns></returns>
            RequestMatcher<Request, Reply>& post(RequestHandlerType handler) {
                m_handlers[HTTP_POST] = handler;
                return *this;
            }
            /// <summary></summary>
            /// <param name="handler"></param>
            /// <returns></returns>
            RequestMatcher<Request, Reply>& put(RequestHandlerType handler) {
                m_handlers[HTTP_PUT] = handler;
                return *this;
            }
            /// <summary></summary>
            /// <param name="handler"></param>
            /// <returns></returns>
            RequestMatcher<Request, Reply>& del(RequestHandlerType handler) {
                m_handlers[HTTP_DELETE] = handler;
                return *this;
            }
            /// <summary></summary>
            /// <param name="handler"></param>
            /// <returns></returns>
            RequestMatcher<Request, Reply>& options(RequestHandlerType handler) {
                m_handlers[HTTP_OPTIONS] = handler;
                return *this;
            }

            /// <summary></summary>
            /// <returns></returns>
            bool regex() const { return m_isRegEx; }
            /// <summary></summary>
            /// <param name="regEx"></param>
            void setRegEx(bool regEx) { m_isRegEx = regEx; }

            /// <summary></summary>
            /// <param name="request"></param>
            /// <param name="reply"></param>
            /// <param name="what"></param>
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
        //      This class implements RESTful web request dispatching.
        // ---------------------------------------------------------------------------

        template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
        class RequestDispatcher {
            typedef RequestMatcher<Request, Reply> MatcherType;
        public:
            /// <summary>Initializes a new instance of the RequestDispatcher class.</summary>
            RequestDispatcher() : m_basePath(), m_debug(false) { /* stub */ }
            /// <summary>Initializes a new instance of the RequestDispatcher class.</summary>
            /// <param name="debug"></param>
            RequestDispatcher(bool debug) : m_basePath(), m_debug(debug) { /* stub */ }
            /// <summary>Initializes a new instance of the RequestDispatcher class.</summary>
            /// <param name="basePath"></param>
            /// <param name="debug"></param>
            RequestDispatcher(const std::string& basePath, bool debug) : m_basePath(basePath), m_debug(debug) { /* stub */ }

            /// <summary></summary>
            /// <param name="expression"></param>
            /// <param name="regex"></param>
            /// <returns></returns>
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

            /// <summary></summary>
            /// <param name="request"></param>
            /// <param name="reply"></param>
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
        //      This class implements a generic basic request dispatcher.
        // ---------------------------------------------------------------------------

        template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
        class BasicRequestDispatcher {
        public:
            typedef std::function<void(const Request&, Reply&)> RequestHandlerType;

            /// <summary>Initializes a new instance of the DebugRequestDispatcher class.</summary>
            BasicRequestDispatcher() { /* stub */ }
            /// <summary>Initializes a new instance of the BasicRequestDispatcher class.</summary>
            /// <param name="handler"></param>
            BasicRequestDispatcher(RequestHandlerType handler) : m_handler(handler) { /* stub */ }

            /// <summary></summary>
            /// <param name="request"></param>
            /// <param name="reply"></param>
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
        //      This class implements a generic debug request dispatcher.
        // ---------------------------------------------------------------------------

        template<typename Request = http::HTTPPayload, typename Reply = http::HTTPPayload>
        class DebugRequestDispatcher {
        public:
            /// <summary>Initializes a new instance of the DebugRequestDispatcher class.</summary>
            DebugRequestDispatcher() { /* stub */ }

            /// <summary></summary>
            /// <param name="request"></param>
            /// <param name="reply"></param>
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
