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
#if !defined(__REST_HTTP__SERVER_CONNECTION_MANAGER_H__)
#define __REST_HTTP__SERVER_CONNECTION_MANAGER_H__

#include "common/Defines.h"

#include <set>
#include <mutex>

namespace network
{
    namespace rest
    {
        namespace http
        {

            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Manages open connections so that they may be cleanly stopped when the server
            //      needs to shut down.
            // ---------------------------------------------------------------------------

            template<typename ConnectionPtr>
            class ServerConnectionManager {
            public:
                auto operator=(ServerConnectionManager&) -> ServerConnectionManager& = delete;
                auto operator=(ServerConnectionManager&&) -> ServerConnectionManager& = delete;
                ServerConnectionManager(ServerConnectionManager&) = delete;

                /// <summary>Initializes a new instance of the ServerConnectionManager class.</summary>
                ServerConnectionManager() = default;

                /// <summary>Add the specified connection to the manager and start it.</summary>
                void start(ConnectionPtr c)
                {
                    std::lock_guard<std::mutex> guard(m_lock);
                    {
                        m_connections.insert(c);
                    }
                    c->start();
                }

                /// <summary>Stop the specified connection.</summary>
                void stop(ConnectionPtr c)
                {
                    std::lock_guard<std::mutex> guard(m_lock);
                    {
                        m_connections.erase(c);
                    }
                    c->stop();
                }

                /// <summary>Stop all connections.</summary>
                void stopAll()
                {
                    for (auto c : m_connections)
                        c->stop();

                    std::lock_guard<std::mutex> guard(m_lock);
                    m_connections.clear();
                }

            private:
                std::set<ConnectionPtr> m_connections;
                std::mutex m_lock;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__SERVER_CONNECTION_MANAGER_H__
