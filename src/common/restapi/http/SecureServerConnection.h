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
 * @file SecureServerConnection.h
 * @ingroup http
 */
#if !defined(__REST_HTTP__SECURE_SERVER_CONNECTION_H__)
#define __REST_HTTP__SECURE_SERVER_CONNECTION_H__

#if defined(ENABLE_SSL)

#include "common/Defines.h"
#include "common/restapi/http/HTTPLexer.h"
#include "common/restapi/http/HTTPPayload.h"
#include "common/Log.h"

#include <array>
#include <memory>
#include <limits>
#include <utility>
#include <iterator>

#include <asio.hpp>
#include <asio/ssl.hpp>

namespace restapi
{
    namespace http
    {
        // ---------------------------------------------------------------------------
        //  Class Prototypes
        // ---------------------------------------------------------------------------

        template<class> class ServerConnectionManager;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class represents a single connection from a client.
         * @tparam RequestHandlerType Type representing a request handler.
         * @ingroup http
         */
        template <typename RequestHandlerType>
        class SecureServerConnection : public std::enable_shared_from_this<SecureServerConnection<RequestHandlerType>> {
            typedef SecureServerConnection<RequestHandlerType> selfType;
            typedef std::shared_ptr<selfType> selfTypePtr;
            typedef ServerConnectionManager<selfTypePtr> ConnectionManagerType;
        public:
            auto operator=(SecureServerConnection&) -> SecureServerConnection& = delete;
            auto operator=(SecureServerConnection&&) -> SecureServerConnection& = delete;
            SecureServerConnection(SecureServerConnection&) = delete;

            /**
             * @brief Initializes a new instance of the SecureServerConnection class.
             * @param socket TCP socket for this connection.
             * @param context SSL context.
             * @param manager Connection manager for this connection.
             * @param handler Request handler for this connection.
             * @param persistent Flag indicating whether or not the connection is persistent.
             * @param debug Flag indicating whether or not verbose logging should be enabled.
             */
            explicit SecureServerConnection(asio::ip::tcp::socket socket, asio::ssl::context& context, ConnectionManagerType& manager, RequestHandlerType& handler,
                bool persistent = false, bool debug = false) :
                m_socket(std::move(socket), context),
                m_connectionManager(manager),
                m_requestHandler(handler),
                m_lexer(HTTPLexer(false)),
                m_continue(false),
                m_contResult(HTTPLexer::INDETERMINATE),
                m_headerBytes(0U),
                m_persistent(persistent),
                m_debug(debug)
            {
                /* stub */
            }

            /**
             * @brief Start the first asynchronous operation for the connection.
             */
            void start() { handshake(); }
            /**
             * @brief Stop all asynchronous operations associated with the connection.
             */
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
            /**
             * @brief Perform an asynchronous SSL handshake.
             */
            void handshake()
            {
                auto self = this->shared_from_this();

                m_socket.async_handshake(asio::ssl::stream_base::server, [this, self](asio::error_code ec) {
                    if (!ec) {
                        read();
                    }
                });
            }

            /**
             * @brief Perform an asynchronous read operation.
             */
            void read()
            {
                auto self = this->shared_from_this();
                m_socket.async_read_some(asio::buffer(m_buffer), [this, self](asio::error_code ec, std::size_t recvLength) {
                    if (!ec) {
                        HTTPLexer::ResultType result = HTTPLexer::GOOD;
                        char* content;

                        // catch exceptions here so we don't blatently crash the system
                        try
                        {
                            if (!m_continue) {
                                m_headerBytes += recvLength;
                                if (m_headerBytes > MAX_HTTP_HEADER_LENGTH) {
                                    result = HTTPLexer::BAD;
                                } else {
                                    std::tie(result, content) = m_lexer.parse(m_request, m_buffer.data(), m_buffer.data() + recvLength);

                                    if (result == HTTPLexer::GOOD) {
                                        size_t length = 0U;
                                        const std::string contentLength = m_request.headers.find("Content-Length");
                                        if (!contentLength.empty() && !parseContentLength(contentLength, length)) {
                                            result = HTTPLexer::BAD;
                                        } else {
                                            m_request.contentLength = length;
                                            m_request.content.clear();

                                            const size_t available = static_cast<size_t>((m_buffer.data() + recvLength) - content);
                                            if (available > length) {
                                                result = HTTPLexer::BAD;
                                            } else {
                                                m_request.content.assign(content, available);
                                                m_request.headers.add("RemoteHost", m_socket.lowest_layer().remote_endpoint().address().to_string());

                                                if (available < length) {
                                                    if (m_debug) {
                                                        LogDebug(LOG_REST, "HTTPS Partial Request, recvLength = %zu, body = %zu/%zu", recvLength,
                                                            available, length);
                                                    }
                                                    m_contResult = result = HTTPLexer::CONTINUE;
                                                    m_continue = true;
                                                }
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (m_debug) {
                                    LogDebug(LOG_REST, "HTTP Partial Request, recvLength = %u, result = %u", recvLength, result);
                                    Utils::dump(1U, "SecureServerConnection::read(), m_buffer", (uint8_t*)m_buffer.data(), recvLength);
                                }

                                const size_t received = m_request.content.size();
                                const size_t remaining = m_request.contentLength - received;
                                if (recvLength > remaining) {
                                    result = HTTPLexer::BAD;
                                } else {
                                    m_request.content.append(m_buffer.data(), recvLength);

                                    if (m_request.content.size() < m_request.contentLength) {
                                        m_contResult = result = HTTPLexer::CONTINUE;
                                    } else {
                                        result = HTTPLexer::GOOD;
                                    }
                                }
                            }

                            if (result == HTTPLexer::GOOD) {
                                if (m_debug) {
                                    Utils::dump(1U, "SecureServerConnection::read(), HTTPS Request Content", (uint8_t*)m_request.content.c_str(), m_request.content.length());
                                }

                                m_continue = false;
                                m_contResult = HTTPLexer::INDETERMINATE;
                                m_headerBytes = 0U;
                                m_requestHandler.handleRequest(m_request, m_reply);

                                if (m_debug) {
                                    Utils::dump(1U, "SecureServerConnection::read(), HTTPS Reply Content", (uint8_t*)m_reply.content.c_str(), m_reply.content.length());
                                }

                                write();
                            }
                            else if (result == HTTPLexer::BAD) {
                                m_continue = false;
                                m_contResult = HTTPLexer::INDETERMINATE;
                                m_headerBytes = 0U;
                                m_reply = HTTPPayload::statusPayload(HTTPPayload::BAD_REQUEST);
                                write();
                            }
                            else {
                                read();
                            }
                        }
                        catch(const std::exception& e) { 
                            ::LogError(LOG_REST, "SecureServerConnection::read(), %s %s", e.what(), ec.message().c_str());
                            m_continue = false;
                            m_contResult = HTTPLexer::INDETERMINATE;
                            m_headerBytes = 0U;

                            m_reply = HTTPPayload::statusPayload(HTTPPayload::INTERNAL_SERVER_ERROR);
                            write();
                        }
                    }
                    else if (ec != asio::error::operation_aborted) {
                        if (ec) {
                            ::LogError(LOG_REST, "SecureServerConnection::read(), %s, code = %u", ec.message().c_str(), ec.value());
                        }
                        m_connectionManager.stop(self);
                        m_continue = false;
                        m_headerBytes = 0U;
                    }
                });
            }

            /**
             * @brief Perform an asynchronous write operation.
             */
            void write()
            {
                auto self = this->shared_from_this();

                if (m_persistent) {
                    m_reply.headers.add("Connection", "keep-alive");
                }

                auto buffers = m_reply.toBuffers();
                asio::async_write(m_socket, buffers, [this, self](asio::error_code ec, std::size_t) {
                    if (m_persistent) {
                        m_lexer.reset();
                        m_reply.headers = HTTPHeaders();
                        m_reply.status = HTTPPayload::OK;
                        m_reply.content = "";
                        m_request = HTTPPayload();
                        m_headerBytes = 0U;
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
                            m_connectionManager.stop(self);
                        }
                    }
                });
            }

            /**
             * @brief Parses the Content-Length header value.
             * @param value The string value of the Content-Length header.
             * @param length The parsed content length.
             * @return True if the content length was successfully parsed and is within the allowed limit, false otherwise.
             */
            static bool parseContentLength(const std::string& value, size_t& length)
            {
                if (value.empty())
                    return false;

                size_t parsed = 0U;
                for (char c : value) {
                    if (c < '0' || c > '9')
                        return false;

                    const size_t digit = static_cast<size_t>(c - '0');
                    if (parsed > (std::numeric_limits<size_t>::max() - digit) / 10U)
                        return false;
                    parsed = parsed * 10U + digit;
                    if (parsed > MAX_HTTP_CONTENT_LENGTH)
                        return false;
                }

                length = parsed;
                return true;
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
            size_t m_headerBytes;

            static constexpr size_t MAX_HTTP_HEADER_LENGTH = 32768U;
            static constexpr size_t MAX_HTTP_CONTENT_LENGTH = 1048576U;

            bool m_persistent;
            bool m_debug;
        };
    } // namespace http
} // namespace restapi

#endif // ENABLE_SSL

#endif // __REST_HTTP__SECURE_SERVER_CONNECTION_H__
