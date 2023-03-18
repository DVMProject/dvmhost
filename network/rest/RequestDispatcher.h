/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__REST__DISPATCHER_H__)
#define __REST__DISPATCHER_H__

#include "Defines.h"
#include "network/rest/http/HTTPRequest.h"
#include "network/rest/http/HTTPReply.h"
#include "Log.h"
 
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

        struct RequestMatch : std::smatch 
        {
            /// <summary>Initializes a new instance of the RequestMatch structure.</summary>
            RequestMatch(const std::smatch& m, const std::string& d) : std::smatch(m), data(d) { /* stub */ }
            
            std::string data;
        };

        // ---------------------------------------------------------------------------
        //  Structure Declaration
        //      
        // ---------------------------------------------------------------------------
        
        template<typename Request, typename Reply>
        struct RequestMatcher {
            typedef std::function<void(const Request&, Reply&, const RequestMatch&)> RequestHandlerType;

            /// <summary>Initializes a new instance of the RequestMatcher structure.</summary>
            explicit RequestMatcher(const std::string& expression) : m_expression(expression), m_isRegEx(false) { /* stub */ }

            /// <summary></summary>
            RequestMatcher<Request, Reply>& get(RequestHandlerType handler) {
                m_handlers["GET"] = handler;
                return *this;
            }
            /// <summary></summary>
            RequestMatcher<Request, Reply>& post(RequestHandlerType handler) {
                m_handlers["POST"] = handler;
                return *this;
            }
            /// <summary></summary>
            RequestMatcher<Request, Reply>& put(RequestHandlerType handler) {
                m_handlers["PUT"] = handler;
                return *this;
            }
            /// <summary></summary>
            RequestMatcher<Request, Reply>& del(RequestHandlerType handler) {
                m_handlers["DELETE"] = handler;
                return *this;
            }
            /// <summary></summary>
            RequestMatcher<Request, Reply>& options(RequestHandlerType handler) {
                m_handlers["OPTIONS"] = handler;
                return *this;
            }

            bool regex() const { return m_isRegEx; }
            void setRegEx(bool regEx) { m_isRegEx = regEx; }

            /// <summary></summary>
            void handleRequest(const Request& request, Reply& reply, const std::smatch &what) {
                // dispatching to matching based on handler
                RequestMatch match(what, request.data);
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

        template<typename Request = http::HTTPRequest, typename Reply = http::HTTPReply>
        class RequestDispatcher {
            typedef RequestMatcher<Request, Reply> MatcherType;
        public:
            /// <summary>Initializes a new instance of the RequestDispatcher class.</summary>
            RequestDispatcher() : m_basePath(), m_debug(false) { /* stub */ }
            /// <summary>Initializes a new instance of the RequestDispatcher class.</summary>
            RequestDispatcher(bool debug) : m_basePath(), m_debug(debug) { /* stub */ }
            /// <summary>Initializes a new instance of the RequestDispatcher class.</summary>
            RequestDispatcher(const std::string& basePath, bool debug) : m_basePath(basePath), m_debug(debug) { /* stub */ }

            /// <summary></summary>
            MatcherType& match(const std::string& expression) 
            {
                MatcherTypePtr& p = m_matchers[expression];
                if (!p) {
                    if (m_debug) {
                        ::LogDebug(LOG_RCON, "creating REST RequestDispatcher, expression = %s", expression.c_str());
                    }
                    p = std::make_shared<MatcherType>(expression);
                } else {
                    if (m_debug) {
                        ::LogDebug(LOG_RCON, "fetching REST RequestDispatcher, expression = %s", expression.c_str());
                    }
                }

                return *p;
            }

            /// <summary></summary>
            void handleRequest(const Request& request, Reply& reply) 
            {
                for (const auto& matcher : m_matchers) {
                    std::smatch what;
                    if (!matcher.second->regex()) {
                        if (request.uri.find(matcher.first) != std::string::npos) {
                            if (m_debug) {
                                ::LogDebug(LOG_RCON, "non-regex endpoint, uri = %s, expression = %s", request.uri.c_str(), matcher.first.c_str());
                            }

                            //what = matcher.first;
                            matcher.second->handleRequest(request, reply, what);
                            return;
                        }
                    } else {
                        if (std::regex_match(request.uri, what, std::regex(matcher.first))) {
                            if (m_debug) {
                                ::LogDebug(LOG_RCON, "regex endpoint, uri = %s, expression = %s", request.uri.c_str(), matcher.first.c_str());
                            }

                            matcher.second->handleRequest(request, reply, what);
                            return;
                        }
                    }
                }

                ::LogError(LOG_RCON, "unknown endpoint, uri = %s", request.uri.c_str());
                reply = http::HTTPReply::stockReply(http::HTTPReply::BAD_REQUEST, "application/json");
            }
        
        private:
            typedef std::shared_ptr<MatcherType> MatcherTypePtr;

            std::string m_basePath;
            std::map<std::string, MatcherTypePtr> m_matchers;

            bool m_debug;
        };

        typedef RequestDispatcher<http::HTTPRequest, http::HTTPReply> DefaultRequestDispatcher;        
    } // namespace rest
} // namespace network
  
#endif // __REST__DISPATCHER_H__ 
