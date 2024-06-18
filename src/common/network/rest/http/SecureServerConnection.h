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
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__REST_HTTP__SECURE_SERVER_CONNECTION_H__)
#define __REST_HTTP__SECURE_SERVER_CONNECTION_H__

#if defined(ENABLE_TCP_SSL)

#include "common/Defines.h"
#include "common/network/rest/http/HTTPLexer.h"
#include "common/network/rest/http/HTTPPayload.h"
#include "common/Log.h"

#include <array>
#include <memory>
#include <utility>
#include <iterator>

#include <asio.hpp>
#include <asio/ssl.hpp>

namespace network
{
    namespace rest
    {
        namespace http
        {
            // ---------------------------------------------------------------------------
            //  Class Prototypes
            // ---------------------------------------------------------------------------

            template<class> class ServerConnectionManager;

            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      This class represents a single connection from a client.
            // ---------------------------------------------------------------------------

            template <typename RequestHandlerType>
            class SecureServerConnection : public std::enable_shared_from_this<SecureServerConnection<RequestHandlerType>> {
                typedef SecureServerConnection<RequestHandlerType> selfType;
                typedef std::shared_ptr<selfType> selfTypePtr;
                typedef ServerConnectionManager<selfTypePtr> ConnectionManagerType;
            public:
                auto operator=(SecureServerConnection&) -> SecureServerConnection& = delete;
                auto operator=(SecureServerConnection&&) -> SecureServerConnection& = delete;
                SecureServerConnection(SecureServerConnection&) = delete;

                /// <summary>Initializes a new instance of the SecureServerConnection class.</summary>
                /// <param name="socket"></param>
                /// <param name="context"></param>
                /// <param name="manager"></param>
                /// <param name="handler"></param>
                /// <param name="persistent"></param>
                /// <param name="debug"></param>
                explicit SecureServerConnection(asio::ip::tcp::socket socket, asio::ssl::context& context, ConnectionManagerType& manager, RequestHandlerType& handler,
                    bool persistent = false, bool debug = false) :
                    m_socket(std::move(socket), context),
                    m_connectionManager(manager),
                    m_requestHandler(handler),
                    m_lexer(HTTPLexer(false)),
                    m_continue(false),
                    m_contResult(HTTPLexer::INDETERMINATE),
                    m_persistent(persistent),
                    m_debug(debug)
                {
                    /* stub */
                }

                /// <summary>Start the first asynchronous operation for the connection.</summary>
                void start() { handshake(); }
                /// <summary>Stop all asynchronous operations associated with the connection.</summary>
                void stop()
                {
                    try
                    {
                        if (m_socket.lowest_layer().is_open()) {
                            m_socket.lowest_layer().close();
                        }
                    }
                    catch(const std::exception&) { /* ignore */ }
                }

            private:
                /// <summary>Perform an asynchronous SSL handshake.</summary>
                void handshake()
                {
                    if (!m_persistent) {
                        auto self(this->shared_from_this());
                    }

                    m_socket.async_handshake(asio::ssl::stream_base::server, [this](asio::error_code ec) {
                        if (!ec) {
                            read();
                        }
                    });
                }

                /// <summary>Perform an asynchronous read operation.</summary>
                void read()
                {
                    if (!m_persistent) {
                        auto self(this->shared_from_this());
                    }

                    m_socket.async_read_some(asio::buffer(m_buffer), [=](asio::error_code ec, std::size_t recvLength) {
                        if (!ec) {
                            HTTPLexer::ResultType result = HTTPLexer::GOOD;
                            char* content;

                            // catch exceptions here so we don't blatently crash the system
                            try
                            {
                                if (!m_continue) {
                                    std::tie(result, content) = m_lexer.parse(m_request, m_buffer.data(), m_buffer.data() + recvLength);

                                    m_request.content = std::string();
                                    std::string contentLength = m_request.headers.find("Content-Length");
                                    if (contentLength != "" && (::strlen(content) != 0)) {
                                        size_t length = (size_t)::strtoul(contentLength.c_str(), NULL, 10);
                                        m_request.contentLength = length;
                                        m_request.content = std::string(content, length);
                                    }

                                    m_request.headers.add("RemoteHost", m_socket.lowest_layer().remote_endpoint().address().to_string());

                                    uint32_t consumed = m_lexer.consumed();
                                    if (result == HTTPLexer::GOOD && consumed == recvLength && 
                                        ((m_request.method == HTTP_POST) || (m_request.method == HTTP_PUT))) {
                                        if (m_debug) {
                                            LogDebug(LOG_REST, "HTTPS Partial Request, recvLength = %u, consumed = %u, result = %u", recvLength, consumed, result);
                                            Utils::dump(1U, "m_buffer", (uint8_t*)m_buffer.data(), recvLength);
                                        }

                                        result = HTTPLexer::INDETERMINATE;
                                        m_continue = true;
                                    }
                                } else {
                                    if (m_debug) {
                                        LogDebug(LOG_REST, "HTTP Partial Request, recvLength = %u, result = %u", recvLength, result);
                                        Utils::dump(1U, "m_buffer", (uint8_t*)m_buffer.data(), recvLength);
                                    }

                                    if (m_contResult == HTTPLexer::INDETERMINATE) {
                                        m_request.content = std::string(m_buffer.data(), recvLength);
                                    } else {
                                        m_request.content.append(std::string(m_buffer.data(), recvLength));
                                    }

                                    if (m_request.contentLength != 0 && recvLength < m_request.contentLength) {
                                        m_contResult = result = HTTPLexer::CONTINUE;
                                        m_continue = true;
                                    }
                                }

                                if (result == HTTPLexer::GOOD) {
                                    if (m_debug) {
                                        Utils::dump(1U, "HTTPS Request Content", (uint8_t*)m_request.content.c_str(), m_request.content.length());
                                    }

                                    m_continue = false;
                                    m_contResult = HTTPLexer::INDETERMINATE;
                                    m_requestHandler.handleRequest(m_request, m_reply);

                                    if (m_debug) {
                                        Utils::dump(1U, "HTTP Reply Content", (uint8_t*)m_reply.content.c_str(), m_reply.content.length());
                                    }

                                    write();
                                }
                                else if (result == HTTPLexer::BAD) {
                                    m_continue = false;
                                    m_contResult = HTTPLexer::INDETERMINATE;
                                    m_reply = HTTPPayload::statusPayload(HTTPPayload::BAD_REQUEST);
                                    write();
                                }
                                else {
                                    read();
                                }
                            }
                            catch(const std::exception& e) { 
                                ::LogError(LOG_REST, "SecureServerConnection::read(), %s", ec.message().c_str());
                                m_continue = false;
                                m_contResult = HTTPLexer::INDETERMINATE;
                            }
                        }
                        else if (ec != asio::error::operation_aborted) {
                            if (ec) {
                                ::LogError(LOG_REST, "SecureServerConnection::read(), %s, code = %u", ec.message().c_str(), ec.value());
                            }
                            m_connectionManager.stop(this->shared_from_this());
                            m_continue = false;
                        }
                    });
                }

                /// <summary>Perform an asynchronous write operation.</summary>
                void write()
                {
                    if (!m_persistent) {
                        auto self(this->shared_from_this());
                    } else {
                        m_reply.headers.add("Connection", "keep-alive");
                    }

                    auto buffers = m_reply.toBuffers();
                    asio::async_write(m_socket, buffers, [=](asio::error_code ec, std::size_t) {
                        if (m_persistent) {
                            m_lexer.reset();
                            m_reply.headers = HTTPHeaders();
                            m_reply.status = HTTPPayload::OK;
                            m_reply.content = "";
                            m_request = HTTPPayload();
                            read();
                        }
                        else {
                            if (!ec) {
                                try
                                {
                                    // initiate graceful connection closure
                                    asio::error_code ignored_ec;
                                    m_socket.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
                                }
                                catch(const std::exception& e) { ::LogError(LOG_REST, "%s", ec.message().c_str()); }
                            }

                            if (ec != asio::error::operation_aborted) {
                                if (ec) {
                                    ::LogError(LOG_REST, "SecureServerConnection::write(), %s, code = %u", ec.message().c_str(), ec.value());
                                }
                                m_connectionManager.stop(this->shared_from_this());
                            }
                        }
                    });
                }

                asio::ssl::stream<asio::ip::tcp::socket> m_socket;

                ConnectionManagerType& m_connectionManager;
                RequestHandlerType& m_requestHandler;

                std::array<char, 8192> m_buffer;

                HTTPPayload m_request;
                HTTPLexer m_lexer;
                HTTPPayload m_reply;

                bool m_continue;
                HTTPLexer::ResultType m_contResult;

                bool m_persistent;
                bool m_debug;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // ENABLE_TCP_SSL

#endif // __REST_HTTP__SECURE_SERVER_CONNECTION_H__
