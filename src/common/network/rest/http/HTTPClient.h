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
#if !defined(__REST_HTTP__HTTP_CLIENT_H__)
#define __REST_HTTP__HTTP_CLIENT_H__

#include "common/Defines.h"
#include "common/network/rest/http/ClientConnection.h"
#include "common/network/rest/http/HTTPRequestHandler.h"
#include "common/Thread.h"

#include <asio.hpp>

#include <thread>
#include <string>
#include <signal.h>
#include <utility>
#include <memory>
#include <mutex>

namespace network
{
    namespace rest
    {
        namespace http
        {

            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      This class implements top-level routines of the HTTP client.
            // ---------------------------------------------------------------------------

            template<typename RequestHandlerType, template<class> class ConnectionImpl = ClientConnection>
            class HTTPClient : private Thread {
            public:
                /// <summary>Initializes a new instance of the HTTPClient class.</summary>
                HTTPClient(const std::string& address, uint16_t port) :
                    m_address(address),
                    m_port(port),
                    m_connection(nullptr),
                    m_ioContext(),
                    m_socket(m_ioContext),
                    m_requestHandler()
                {
                    /* stub */
                }
                /// <summary>Initializes a copy instance of the HTTPClient class.</summary>
                HTTPClient(const HTTPClient&) = delete;
                /// <summary>Finalizes a instance of the HTTPClient class.</summary>
                ~HTTPClient() override
                {
                    if (m_connection != nullptr) {
                        close();
                    }
                }

                /// <summary></summary>
                HTTPClient& operator=(const HTTPClient&) = delete;

                /// <summary>Helper to set the HTTP request handlers.</summary>
                template<typename Handler>
                void setHandler(Handler&& handler)
                {
                    m_requestHandler = RequestHandlerType(std::forward<Handler>(handler));
                }

                /// <summary>Send HTTP request to HTTP server.</summary>
                bool request(HTTPPayload& request)
                {
                    if (m_completed) {
                        return false;
                    }

                    asio::post(m_ioContext, [this, request]() {
                        std::lock_guard<std::mutex> guard(m_lock);
                        {
                            if (m_connection != nullptr) {
                                m_connection->send(request);
                            }
                        }
                    });

                    return true;
                }

                /// <summary>Opens connection to the network.</summary>
                bool open()
                {
                    if (m_completed) {
                        return false;
                    }

                    return run();
                }

                /// <summary>Closes connection to the network.</summary>
                void close()
                {
                    if (m_completed) {
                        return;
                    }

                    m_completed = true;
                    m_ioContext.stop();

                    wait();
                }

            private:
                /// <summary></summary>
                void entry() override
                {
                    if (m_completed) {
                        return;
                    }

                    asio::ip::tcp::resolver resolver(m_ioContext);
                    auto endpoints = resolver.resolve(m_address, std::to_string(m_port));

                    try {
                        connect(endpoints);

                        // the entry() call will block until all asynchronous operations
                        // have finished
                        m_ioContext.run();
                    }
                    catch (std::exception&) { /* stub */ }

                    if (m_connection != nullptr) {
                        m_connection->stop();
                    }
                }

                /// <summary>Perform an asynchronous connect operation.</summary>
                void connect(asio::ip::basic_resolver_results<asio::ip::tcp>& endpoints)
                {
                    asio::connect(m_socket, endpoints);

                    m_connection = std::make_unique<ConnectionType>(std::move(m_socket), m_requestHandler);
                    m_connection->start();
                }

                std::string m_address;
                uint16_t m_port;

                typedef ConnectionImpl<RequestHandlerType> ConnectionType;

                std::unique_ptr<ConnectionType> m_connection;

                bool m_completed = false;
                asio::io_context m_ioContext;

                asio::ip::tcp::socket m_socket;

                RequestHandlerType m_requestHandler;

                std::mutex m_lock;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__HTTP_CLIENT_H__
