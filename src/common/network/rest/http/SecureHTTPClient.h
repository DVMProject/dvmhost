// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @file SecureHTTPClient.h
 * @ingroup http
 */
#if !defined(__REST_HTTP__SECURE_HTTP_CLIENT_H__)
#define __REST_HTTP__SECURE_HTTP_CLIENT_H__

#if defined(ENABLE_SSL)

#include "common/Defines.h"
#include "common/network/rest/http/SecureClientConnection.h"
#include "common/network/rest/http/HTTPRequestHandler.h"
#include "common/Thread.h"

#include <thread>
#include <string>
#include <signal.h>
#include <utility>
#include <memory>
#include <mutex>

#include <asio.hpp>
#include <asio/ssl.hpp>

namespace network
{
    namespace rest
    {
        namespace http
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief This class implements top-level routines of the secure HTTP client.
             * @tparam RequestHandlerType Type representing a request handler.
             * @tparam ConnectionImpl Type representing the connection implementation.
             * @ingroup http
             */
            template<typename RequestHandlerType, template<class> class ConnectionImpl = SecureClientConnection>
            class SecureHTTPClient : private Thread {
            public:
                auto operator=(SecureHTTPClient&) -> SecureHTTPClient& = delete;
                auto operator=(SecureHTTPClient&&) -> SecureHTTPClient& = delete;
                SecureHTTPClient(SecureHTTPClient&) = delete;

                /**
                 * @brief Initializes a new instance of the SecureHTTPClient class.
                 * @param address Hostname/IP Address.
                 * @param port Port.
                 */
                SecureHTTPClient(const std::string& address, uint16_t port) :
                    m_address(address),
                    m_port(port),
                    m_connection(nullptr),
                    m_ioContext(),
                    m_context(asio::ssl::context::tlsv12),
                    m_socket(m_ioContext),
                    m_requestHandler()
                {
                    /* stub */
                }
                /**
                 * @brief Finalizes a instance of the SecureHTTPClient class.
                 */
                ~SecureHTTPClient() override
                {
                    if (m_connection != nullptr) {
                        close();
                    }
                }

                /**
                 * @brief Helper to set the HTTP request handlers.
                 * @tparam Handler Type representing the request handler.
                 * @param handler Request handler.
                 */
                template<typename Handler>
                void setHandler(Handler&& handler)
                {
                    m_requestHandler = RequestHandlerType(std::forward<Handler>(handler));
                }

                /**
                 * @brief Send HTTP request to HTTP server.
                 * @param request HTTP request.
                 * @returns True, if request was completed, otherwise false.
                 */
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

                /**
                 * @brief Opens connection to the network.
                 */
                bool open()
                {
                    if (m_completed) {
                        return false;
                    }

                    return run();
                }

                /**
                 * @brief Closes connection to the network.
                 */
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
                /**
                 * @brief Internal entry point for the ASIO IO context thread.
                 */
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

                /**
                 * @brief Perform an asynchronous connect operation.
                 * @param endpoints TCP endpoint to connect to.
                 */
                void connect(asio::ip::basic_resolver_results<asio::ip::tcp>& endpoints)
                {
                    asio::connect(m_socket, endpoints);

                    m_connection = std::make_unique<ConnectionType>(std::move(m_socket), m_context, m_requestHandler);
                    m_connection->start();
                }

                std::string m_address;
                uint16_t m_port;

                typedef ConnectionImpl<RequestHandlerType> ConnectionType;

                std::unique_ptr<ConnectionType> m_connection;

                bool m_completed = false;
                asio::io_context m_ioContext;

                asio::ssl::context m_context;
                asio::ip::tcp::socket m_socket;

                RequestHandlerType m_requestHandler;

                std::mutex m_lock;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // ENABLE_SSL

#endif // __REST_HTTP__SECURE_HTTP_CLIENT_H__
