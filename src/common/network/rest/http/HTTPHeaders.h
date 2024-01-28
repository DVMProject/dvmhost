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
#if !defined(__REST_HTTP__HTTP_HEADERS_H__)
#define __REST_HTTP__HTTP_HEADERS_H__

#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"

#include <string>
#include <vector>

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
            //  Structure Declaration
            //
            // ---------------------------------------------------------------------------

            struct HTTPHeaders {
                struct Header
                {
                    std::string name;
                    std::string value;

                    Header() : name{}, value{} { /* stub */}
                    Header(std::string n, std::string v) : name{n}, value{v} { /* stub */ }
                };

                /// <summary>Gets the list of HTTP headers.</summary>
                std::vector<Header> headers() const { return m_headers; }
                /// <summary>Returns true if the headers are empty.</summary>
                bool empty() const { return m_headers.empty(); }
                /// <summary>Returns the number of headers.</summary>
                std::size_t size() const { return m_headers.size(); }
                /// <summary>Clears the list of HTTP headers.</summary>
                void clearHeaders() { m_headers = std::vector<Header>(); }
                /// <summary>Helper to add a HTTP header.</summary>
                void add(const std::string& name, const std::string& value)
                {
                    //::LogDebug(LOG_REST, "HTTPHeaders::add(), header = %s, value = %s", name.c_str(), value.c_str());
                    for (auto& header : m_headers) {
                        if (::strtolower(header.name) == ::strtolower(name)) {
                            header.value = value;
                            return;
                        }
                    }

                    m_headers.push_back(Header(name, value));
                    //for (auto header : m_headers)
                    //    ::LogDebug(LOG_REST, "HTTPHeaders::add() m_headers.header = %s, m_headers.value = %s", header.name.c_str(), header.value.c_str());
                }
                /// <summary>Helper to add a HTTP header.</summary>
                void remove(const std::string headerName)
                {
                    auto header = std::find_if(m_headers.begin(), m_headers.end(), [&](const Header& h) {
                        return ::strtolower(h.name) == ::strtolower(headerName);
                    });

                    if (header != m_headers.end()) {
                        m_headers.erase(header);
                    }
                }
                /// <summary>Helper to find the named HTTP header.</summary>
                std::string find(const std::string headerName) const
                {
                    auto header = std::find_if(m_headers.begin(), m_headers.end(), [&](const Header& h) {
                        return ::strtolower(h.name) == ::strtolower(headerName);
                    });

                    if (header != m_headers.end()) {
                        return header->value;
                    }
                    else {
                        return "";
                    }
                }

            private:
                friend struct HTTPPayload;
                std::vector<Header> m_headers;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__HTTP_HEADERS_H__
