// SPDX-License-Identifier: GPL-2.0-only
/*
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
 * @file SecureClientConnection.h
 * @ingroup http
 */
#if !defined(__REST_HTTP__SECURE_CLIENT_CONNECTION_H__)
#define __REST_HTTP__SECURE_CLIENT_CONNECTION_H__

#if defined(ENABLE_SSL)

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
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief This class represents a single connection from a client.
             * @tparam RequestHandlerType Type representing a request handler.
             * @ingroup http
             */
            template <typename RequestHandlerType>
            class SecureClientConnection {
            public:
                auto operator=(SecureClientConnection&) -> SecureClientConnection& = delete;
                auto operator=(SecureClientConnection&&) -> SecureClientConnection& = delete;
                SecureClientConnection(SecureClientConnection&) = delete;

                /**
                 * @brief Initializes a new instance of the SecureClientConnection class.
                 * @param socket TCP socket for this connection.
                 * @param context SSL context for this connection.
                 * @param handler Request handler for this connection.
                 */
                explicit SecureClientConnection(asio::ip::tcp::socket socket, asio::ssl::context& context, RequestHandlerType& handler) :
                    m_socket(std::move(socket), context),
                    m_requestHandler(handler),
                    m_lexer(HTTPLexer(true))
                {
                    m_socket.set_verify_mode(asio::ssl::verify_none);
                    m_socket.set_verify_callback(std::bind(&SecureClientConnection::verify_certificate, this, std::placeholders::_1, std::placeholders::_2));
                }

                /**
                 * @brief Start the first asynchronous operation for the connection.
                 */
                void start() 
                { 
                    m_socket.handshake(asio::ssl::stream_base::client);                    
                    read(); 
                }
                /**
                 * @brief Stop all asynchronous operations associated with the connection.
                 */
                void stop()
                {
                    try
                    {
                        ensureNoLinger();
                        if (m_socket.lowest_layer().is_open()) {
                            m_socket.lowest_layer().close();
                        }
                    }
                    catch(const std::exception&) { /* ignore */ }
                }

                /**
                 * @brief Helper to enable the SO_LINGER socket option during shutdown.
                 */
                void ensureNoLinger()
                {
                    try
                    {
                        // enable SO_LINGER timeout 0
                        asio::socket_base::linger linger(true, 0);
                        m_socket.lowest_layer().set_option(linger);
                    }
                    catch(const asio::system_error& e)
                    {
                        asio::error_code ec = e.code();
                        if (ec) {
                            ::LogError(LOG_REST, "SecureClientConnection::ensureNoLinger(), %s, code = %u", ec.message().c_str(), ec.value());
                        }
                    }
                }

                /**
                 * @brief Perform an synchronous write operation.
                 * @param request HTTP request.
                 */
                void send(HTTPPayload request)
                {
                    request.attachHostHeader(m_socket.lowest_layer().remote_endpoint());
                    write(request);
                }
            private:
                /**
                 * @brief Perform an SSL certificate verification.
                 * @param preverified Flag indicating the SSL certificate was preverified.
                 * @param context SSL verification context.
                 * @returns True, if SSL certificate is valid, otherwise false.
                 */
                bool verify_certificate(bool preverified, asio::ssl::verify_context& context)
                {
                    return true; // ignore always valid
                }

                /**
                 * @brief Perform an asynchronous read operation.
                 */
                void read()
                {
                    m_socket.async_read_some(asio::buffer(m_buffer), [=](asio::error_code ec, std::size_t bytes_transferred) {
                        if (!ec) {
                            HTTPLexer::ResultType result;
                            char* content;

                            try
                            {
                                if (m_sizeToTransfer > 0U && (m_bytesTransferred + bytes_transferred) < m_sizeToTransfer) {
                                    ::memcpy(m_fullBuffer.data() + m_bytesTransferred, m_buffer.data(), bytes_transferred);
                                    m_bytesTransferred += bytes_transferred;

                                    read();
                                }
                                else {
                                    if (m_sizeToTransfer > 0U) {
                                        // final copy
                                        ::memcpy(m_fullBuffer.data() + m_bytesTransferred, m_buffer.data(), bytes_transferred);
                                        m_bytesTransferred += bytes_transferred;

                                        m_sizeToTransfer = 0U;
                                        bytes_transferred = m_bytesTransferred;

                                        // reset lexer and re-parse the full content
                                        m_lexer.reset();
                                        std::tie(result, content) = m_lexer.parse(m_request, m_fullBuffer.data(), m_fullBuffer.data() + bytes_transferred);
                                    } else {
                                        ::memcpy(m_fullBuffer.data() + m_bytesTransferred, m_buffer.data(), bytes_transferred);
                                        m_bytesTransferred += bytes_transferred;

                                        std::tie(result, content) = m_lexer.parse(m_request, m_buffer.data(), m_buffer.data() + bytes_transferred);
                                    }

                                    // determine content length
                                    std::string contentLength = m_request.headers.find("Content-Length");
                                    if (contentLength != "") {
                                        size_t length = (size_t)::strtoul(contentLength.c_str(), NULL, 10);

                                        // setup a full read if necessary
                                        if (length > bytes_transferred && m_sizeToTransfer == 0U) {
                                            m_sizeToTransfer = length;
                                        }

                                        if (m_sizeToTransfer > 0U) {
                                            result = HTTPLexer::CONTINUE;
                                        } else {
                                            m_request.content = std::string(content, length);
                                        }
                                    }

                                    m_request.headers.add("RemoteHost", m_socket.lowest_layer().remote_endpoint().address().to_string());
                                    if (result == HTTPLexer::GOOD) {
                                        m_sizeToTransfer = m_bytesTransferred = 0U;
                                        m_requestHandler.handleRequest(m_request, m_reply);
                                    }
                                    else if (result == HTTPLexer::BAD) {
                                        m_sizeToTransfer = m_bytesTransferred = 0U;
                                        return;
                                    }
                                    else {
                                        read();
                                    }
                                }
                            }
                            catch(const std::exception& e) { ::LogError(LOG_REST, "SecureClientConnection::read(), %s", ec.message().c_str()); }
                        }
                        else if (ec != asio::error::operation_aborted) {
                            if (ec) {
                                ::LogError(LOG_REST, "SecureClientConnection::read(), %s, code = %u", ec.message().c_str(), ec.value());
                            }
                            stop();
                        }
                    });
                }

                /**
                 * @brief Perform an synchronous write operation.
                 * @param request HTTP request.
                 */
                void write(HTTPPayload request)
                {
                    try
                    {
                        m_socket.handshake(asio::ssl::stream_base::client);

                        auto buffers = request.toBuffers();
                        asio::write(m_socket, buffers);
                    }
                    catch(const asio::system_error& e)
                    {
                        asio::error_code ec = e.code();
                        if (ec) {
                            ::LogError(LOG_REST, "SecureClientConnection::write(), %s, code = %u", ec.message().c_str(), ec.value());

                            try
                            {
                                // initiate graceful connection closure
                                asio::error_code ignored_ec;
                                m_socket.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
                            }
                            catch(const std::exception& e) { 
                                ::LogError(LOG_REST, "SecureClientConnection::write(), %s, code = %u", ec.message().c_str(), ec.value()); 
                            }
                        }
                    }
                }

                asio::ssl::stream<asio::ip::tcp::socket> m_socket;

                RequestHandlerType& m_requestHandler;

                std::size_t m_sizeToTransfer;
                std::size_t m_bytesTransferred;
                std::array<char, 65535> m_fullBuffer;

                std::array<char, 4096> m_buffer;

                HTTPPayload m_request;
                HTTPLexer m_lexer;
                HTTPPayload m_reply;
            };
        } // namespace http
    } // namespace rest
} // namespace network

#endif // ENABLE_SSL

#endif // __REST_HTTP__SECURE_CLIENT_CONNECTION_H__
