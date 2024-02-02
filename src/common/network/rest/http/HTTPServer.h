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
#if !defined(__REST_HTTP__HTTP_SERVER_H__)
#define __REST_HTTP__HTTP_SERVER_H__

#include "common/Defines.h"
#include "common/network/rest/http/ServerConnection.h"
#include "common/network/rest/http/ServerConnectionManager.h"
#include "common/network/rest/http/HTTPRequestHandler.h"

#include <asio.hpp>

#include <thread>
#include <string>
#include <signal.h>
#include <utility>
#include <memory>

namespace network
{
    namespace rest
    {
        namespace http
        {

            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      This class implements top-level routines of the HTTP server.
            // ---------------------------------------------------------------------------

            template<typename RequestHandlerType, template<class> class ConnectionImpl = ServerConnection>
            class HTTPServer {
            public:
                auto operator=(HTTPServer&) -> HTTPServer& = delete;
                auto operator=(HTTPServer&&) -> HTTPServer& = delete;
                HTTPServer(HTTPServer&) = delete;

                /// <summary>Initializes a new instance of the HTTPServer class.</summary>
                explicit HTTPServer(const std::string& address, uint16_t port) :
                    m_ioService(),
                    m_acceptor(m_ioService),
                    m_connectionManager(),
                    m_socket(m_ioService),
                    m_requestHandler()
                {
                    // open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR)
                    asio::ip::address ipAddress = asio::ip::address::from_string(address);
                    asio::ip::tcp::endpoint endpoint(ipAddress, port);

                    m_acceptor.open(endpoint.protocol());
                    m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
                    m_acceptor.set_option(asio::socket_base::keep_alive(true));
                    m_acceptor.bind(endpoint);
                    m_acceptor.listen();

                    accept();
                }

                /// <summary>Helper to set the HTTP request handlers.</summary>
                template<typename Handler>
                void setHandler(Handler&& handler)
                {
                    m_requestHandler = RequestHandlerType(std::forward<Handler>(handler));
                }

                /// <summary>Run the servers ASIO IO service loop.</summary>
                void run()
                {
                    // the run() call will block until all asynchronous operations
                    // have finished; while the server is running, there is always at least one
                    // asynchronous operation outstanding: the asynchronous accept call waiting
                    // for new incoming connections
                    m_ioService.run();
                }

                /// <summary>Helper to stop running ASIO IO services.</summary>
                void stop()
                {
                    // the server is stopped by cancelling all outstanding asynchronous
                    // operations; once all operations have finished the m_ioService::run()
                    // call will exit
                    m_acceptor.close();
                    m_connectionManager.stopAll();
                }

            private:
                /// <summary>Perform an asynchronous accept operation.</summary>
                void accept()
                {
                    m_acceptor.async_accept(m_socket, [this](asio::error_code ec) {
                        // check whether the server was stopped by a signal before this
                        // completion handler had a chance to run
                        if (!m_acceptor.is_open()) {
                            return;
                        }

                        if (!ec) {
                            m_connectionManager.start(std::make_shared<ConnectionType>(std::move(m_socket), m_connectionManager, m_requestHandler));
                        }

                        accept();
                    });
                }

                typedef ConnectionImpl<RequestHandlerType> ConnectionType;
                typedef std::shared_ptr<ConnectionType> ConnectionTypePtr;

                asio::io_service m_ioService;
                asio::ip::tcp::acceptor m_acceptor;

                ServerConnectionManager<ConnectionTypePtr> m_connectionManager;

                asio::ip::tcp::socket m_socket;

                RequestHandlerType m_requestHandler;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // __REST_HTTP__HTTP_SERVER_H__
