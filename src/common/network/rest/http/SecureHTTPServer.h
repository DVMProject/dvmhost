// SPDX-License-Identifier: BSL-1.0
/*
 * Digital Voice Modem - Common Library
 * BSL-1.0 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2003-2013 Christopher M. Kohlhoff
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file HTTPServer.h
 * @ingroup http
 */
#if !defined(__REST_HTTP__SECURE_HTTP_SERVER_H__)
#define __REST_HTTP__SECURE_HTTP_SERVER_H__

#if defined(ENABLE_SSL)

#include "common/Defines.h"
#include "common/network/rest/http/SecureServerConnection.h"
#include "common/network/rest/http/ServerConnectionManager.h"
#include "common/network/rest/http/HTTPRequestHandler.h"

#include <thread>
#include <string>
#include <signal.h>
#include <utility>
#include <memory>

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
             * @brief This class implements top-level routines of the secure HTTP server.
             * @tparam RequestHandlerType Type representing a request handler.
             * @tparam ConnectionImpl Type representing the connection implementation.
             * @ingroup http
             */
            template<typename RequestHandlerType, template<class> class ConnectionImpl = SecureServerConnection>
            class SecureHTTPServer {
            public:
                auto operator=(SecureHTTPServer&) -> SecureHTTPServer& = delete;
                auto operator=(SecureHTTPServer&&) -> SecureHTTPServer& = delete;
                SecureHTTPServer(SecureHTTPServer&) = delete;

                /**
                 * @brief Initializes a new instance of the SecureHTTPServer class.
                 * @param address Hostname/IP Address.
                 * @param port Port.
                 * @param debug Flag indicating whether or not verbose logging should be enabled.
                 */
                explicit SecureHTTPServer(const std::string& address, uint16_t port, bool debug) :
                    m_ioService(),
                    m_acceptor(m_ioService),
                    m_connectionManager(),
                    m_context(asio::ssl::context::tlsv12),
                    m_socket(m_ioService),
                    m_requestHandler(),
                    m_debug(debug)
                {
                    asio::ip::address ipAddress = asio::ip::address::from_string(address);
                    m_endpoint = asio::ip::tcp::endpoint(ipAddress, port);
                }

                /**
                 * @brief Helper to set the SSL certificate and private key.
                 * @param keyFile SSL certificate private key.
                 * @param certFile SSL certificate.
                 */
                bool setCertAndKey(const std::string& keyFile, const std::string& certFile)
                {
                    try
                    {
                        m_context.use_certificate_chain_file(certFile);
                        m_context.use_private_key_file(keyFile, asio::ssl::context::pem);
                        return true;
                    }
                    catch(const std::exception& e) { 
                        ::LogError(LOG_REST, "%s", e.what()); 
                        return false;
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
                 * @brief Open TCP acceptor.
                 */
                void open()
                {
                    // open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR)
                    m_acceptor.open(m_endpoint.protocol());
                    m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
                    m_acceptor.set_option(asio::socket_base::keep_alive(true));
                    m_acceptor.bind(m_endpoint);
                    m_acceptor.listen();

                    accept();
                }

                /**
                 * @brief Run the servers ASIO IO service loop.
                 */
                void run()
                {
                    // the run() call will block until all asynchronous operations
                    // have finished; while the server is running, there is always at least one
                    // asynchronous operation outstanding: the asynchronous accept call waiting
                    // for new incoming connections
                    m_ioService.run();
                }

                /**
                 * @brief Helper to stop running ASIO IO services.
                 */
                void stop()
                {
                    // the server is stopped by cancelling all outstanding asynchronous
                    // operations; once all operations have finished the m_ioService::run()
                    // call will exit
                    m_acceptor.close();
                    m_connectionManager.stopAll();
                }

            private:
                /**
                 * @brief Perform an asynchronous accept operation.
                 */
                void accept()
                {
                    m_acceptor.async_accept(m_socket, [this](asio::error_code ec) {
                        // check whether the server was stopped by a signal before this
                        // completion handler had a chance to run
                        if (!m_acceptor.is_open()) {
                            return;
                        }

                        if (!ec) {
                            m_connectionManager.start(std::make_shared<ConnectionType>(std::move(m_socket), m_context, m_connectionManager, m_requestHandler, false, m_debug));
                        }

                        accept();
                    });
                }

                typedef ConnectionImpl<RequestHandlerType> ConnectionType;
                typedef std::shared_ptr<ConnectionType> ConnectionTypePtr;

                asio::io_service m_ioService;
                asio::ip::tcp::acceptor m_acceptor;

                asio::ip::tcp::endpoint m_endpoint;

                ServerConnectionManager<ConnectionTypePtr> m_connectionManager;

                asio::ssl::context m_context;
                asio::ip::tcp::socket m_socket;

                std::string m_certFile;
                std::string m_keyFile;

                RequestHandlerType m_requestHandler;
                bool m_debug;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // ENABLE_SSL

#endif // __REST_HTTP__SECURE_HTTP_SERVER_H__
