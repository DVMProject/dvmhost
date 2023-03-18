/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the CRUD project. (https://github.com/venediktov/CRUD)
// Licensed under the BPL-1.0 License (https://opensource.org/license/bsl1-0-html)
//
/*
*   Copyright (c) 2003-2013 Christopher M. Kohlhoff
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   Permission is hereby granted, free of charge, to any person or organization 
*   obtaining a copy of the software and accompanying documentation covered by 
*   this license (the “Software”) to use, reproduce, display, distribute, execute, 
*   and transmit the Software, and to prepare derivative works of the Software, and
*   to permit third-parties to whom the Software is furnished to do so, all subject
*   to the following:
*
*   The copyright notices in the Software and this entire statement, including the
*   above license grant, this restriction and the following disclaimer, must be included
*   in all copies of the Software, in whole or in part, and all derivative works of the
*   Software, unless such copies or derivative works are solely in the form of
*   machine-executable object code generated by a source language processor.
*
*   THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
*   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*   PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE
*   DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN
*   CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
*   OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#if !defined(__REST_HTTP__HTTP_SERVER_H__)
#define __REST_HTTP__HTTP_SERVER_H__

#include "Defines.h"
#include "network/rest/http/Connection.h"
#include "network/rest/http/ConnectionManager.h"
#include "network/rest/http/HTTPRequestHandler.h"
#include "network/rest/http/HTTPReply.h"
#include "network/rest/http/HTTPRequest.h"

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

            template<typename RequestHandlerType, template<class> class ConnectionImpl = Connection>
            class HTTPServer {
            public:
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
                /// <summary>Initializes a copy instance of the HTTPServer class.</summary>
                HTTPServer(const HTTPServer&) = delete;
    
                /// <summary></summary>
                HTTPServer& operator=(const HTTPServer&) = delete;

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
                
                ConnectionManager<ConnectionTypePtr> m_connectionManager;
                
                asio::ip::tcp::socket m_socket;
                
                RequestHandlerType m_requestHandler;
            };
        } // namespace http
    } // namespace rest
} // namespace network
 
#endif // __REST_HTTP__HTTP_SERVER_H__
