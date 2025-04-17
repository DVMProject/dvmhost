// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SecureTcpClient.h
 * @ingroup tcp_socket
 * @file SecureTcpClient.cpp
 * @ingroup tcp_socket
 */
#if !defined(__SECURE_TCP_CLIENT_H__)
#define __SECURE_TCP_CLIENT_H__

#if defined(ENABLE_SSL)

#include "Defines.h"
#include "common/Log.h"
#include "common/network/tcp/Socket.h"

#include <cassert>
#include <cstring>

#include <fcntl.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

namespace network
{
    namespace tcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a secure TCP client.
         * @ingroup tcp_socket
         */
        class HOST_SW_API SecureTcpClient : public Socket
        {
        public:
            auto operator=(SecureTcpClient&) -> SecureTcpClient& = delete;
            auto operator=(SecureTcpClient&&) -> SecureTcpClient& = delete;
            SecureTcpClient(SecureTcpClient&) = delete;

            /**
             * @brief Initializes a new instance of the SecureTcpClient class.
             * @param fd File Descriptor for existing socket.
             * @param sslCtx Instance of the OpenSSL context.
             * @param client Address for client.
             * @param clientLen Length of sockaddr_in structure.
             * @param nonBlocking Flag indicating socket operations should be non-blocking.
             */
            SecureTcpClient(const int fd, SSL_CTX* sslCtx, sockaddr_in& client, int clientLen, bool nonBlocking) : Socket(fd),
                m_sockaddr(),
                m_pSSL(nullptr),
                m_pSSLCtx(nullptr)
            {
                ::memcpy(reinterpret_cast<char*>(&m_sockaddr), reinterpret_cast<char*>(&client), clientLen);

                initSsl(sslCtx);

                // setup socket for non-blocking operations
                int flags = fcntl(fd, F_GETFL, 0);
                if (flags < 0) {
                    LogError(LOG_NET, "failed fcntl(F_GETFL), err: %d", errno);
                    throw std::runtime_error("Cannot accept SSL client");
                }

                if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
                    LogError(LOG_NET, "failed fcntl(F_SETFL), err: %d", errno);
                    throw std::runtime_error("Cannot accept SSL client");
                }

                int status = -1;
                struct timeval tv, tvRestore;
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                tvRestore = tv;

                fd_set writeFdSet;
                fd_set readFdSet;

                // begin application specific OpenSSL handshake polling
                do {
                    tv = tvRestore;
                    FD_ZERO(&writeFdSet);
                    FD_ZERO(&readFdSet);

                    status = ::SSL_accept(m_pSSL);
                    switch (::SSL_get_error(m_pSSL, status)) {
                    case SSL_ERROR_NONE:
                        status = 0;
                        break;

                    case SSL_ERROR_WANT_WRITE:
                        FD_SET(fd, &writeFdSet);
                        status = 1; // wait for more activity
                        break;

                    case SSL_ERROR_WANT_READ:
                        FD_SET(fd, &readFdSet);
                        status = 1; // wait for more activity
                        break;

                    case SSL_ERROR_ZERO_RETURN:
                    case SSL_ERROR_SYSCALL:
                        // the peer has notified us that it is shutting down via
                        // the SSL "close_notify" message so we need to
                        // shutdown, too.
                        LogError(LOG_NET, "Cannot accept SSL client, connection closed during handshake, %s err: %d", ERR_error_string(ERR_get_error(), NULL), errno);
                        throw std::runtime_error("Cannot accept SSL client");
                    default:
                        LogError(LOG_NET, "Cannot accept SSL client, unexpected error during handshake, %s err: %d", ERR_error_string(ERR_get_error(), NULL), errno);
                        throw std::runtime_error("Cannot accept SSL client");
                    }

                    if (status == 1) {
                        // must have at least one handle to wait for at this point.
                        status = select(fd + 1, &readFdSet, &writeFdSet, NULL, &tv);
                        if (status >= 1) {
                            status = 1;
                        }
                        else {
                            LogError(LOG_NET, "Cannot accept SSL client, timeout during handshake, %s err: %d", ERR_error_string(ERR_get_error(), NULL), errno);
                            throw std::runtime_error("Cannot accept SSL client");
                        }
                    }
                } while (status == 1 && !SSL_is_init_finished(m_pSSL));

                // reset socket blocking operations
                if (!nonBlocking) {
                    flags = fcntl(fd, F_GETFL, 0);
                    if (flags < 0) {
                        LogError(LOG_NET, "failed fcntl(F_GETFL), err: %d", errno);
                        throw std::runtime_error("Cannot accept SSL client");
                    }

                    if (fcntl(fd, F_SETFL, flags & (~O_NONBLOCK)) < 0) {
                        LogError(LOG_NET, "failed fcntl(F_SETFL), err: %d", errno);
                        throw std::runtime_error("Cannot accept SSL client");
                    }
                }
            }
            /**
             * @brief Initializes a new instance of the SecureTcpClient class.
             * @param address IP Address.
             * @param port Port.
             * @param nonBlocking Flag indicating socket operations should be non-blocking.
             */
            SecureTcpClient(const std::string& address, const uint16_t port, bool nonBlocking) : 
                m_pSSL(nullptr),
                m_pSSLCtx(nullptr)
            {
                assert(!address.empty());
                assert(port > 0U);

                OpenSSL_add_ssl_algorithms();
                const SSL_METHOD* method = SSLv23_client_method();
                SSL_load_error_strings();
                m_pSSLCtx = SSL_CTX_new(method);

                init();

                sockaddr_in addr = {};
                initAddr(address, port, addr);

                ::memcpy(reinterpret_cast<char*>(&m_sockaddr), reinterpret_cast<char*>(&addr), sizeof(addr));

                ssize_t ret = ::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
                if (ret < 0) {
                    LogError(LOG_NET, "Failed to connect to server, err: %d", errno);
                }

                initSsl(m_pSSLCtx);
                if (SSL_connect(m_pSSL) <= 0) {
                    LogError(LOG_NET, "Failed to connect to server, %s err: %d", ERR_error_string(ERR_get_error(), NULL), errno);
                    throw std::runtime_error("Failed to SSL connect to server");
                }
            
                // setup socket for non-blocking operations
                if (nonBlocking) {
                    int flags = fcntl(m_fd, F_GETFL, 0);
                    if (flags < 0) {
                        LogError(LOG_NET, "failed fcntl(F_GETFL), err: %d", errno);
                        throw std::runtime_error("Failed to set SSL server connection to non-blocking");
                    }

                    if (fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
                        LogError(LOG_NET, "failed fcntl(F_SETFL), err: %d", errno);
                        throw std::runtime_error("Failed to set SSL server connection to non-blocking");
                    }
                }
            }
            /**
             * @brief Finalizes a instance of the SecureTcpClient class.
             */
            ~SecureTcpClient() override
            {
                if (m_pSSL != nullptr) {
                    SSL_shutdown(m_pSSL);
                    SSL_free(m_pSSL);
                }

                if (m_pSSLCtx != nullptr)
                    SSL_CTX_free(m_pSSLCtx);
            }

            /**
             * @brief Read data from the socket.
             * @param[out] buffer Buffer to read data into.
             * @param[out] length Length of data to read.
             * @returns ssize_t Actual length of data read from remote TCP socket.
             */
            [[nodiscard]] ssize_t read(uint8_t* buffer, size_t len) noexcept override
            {
                return SSL_read(m_pSSL, buffer, (int)len);
            }
            /**
             * @brief Write data to the socket.
             * @param[in] buffer Buffer containing data to write to socket.
             * @param length Length of data to write.
             * @returns ssize_t Length of data written.
             */
            ssize_t write(const uint8_t* buffer, size_t len) noexcept override
            {
                return SSL_write(m_pSSL, buffer, (int)len);
            }

            /**
             * @brief Helper to get an IP address from the sockaddr_storage.
             * @returns sockaddr_storage sockaddr_storage structure.
             */
            sockaddr_storage getAddress() const { return m_sockaddr; }

            /**
             * @brief Sets the hostname for the SSL certificate.
             * @param hostname Hostname.
             */
            static void setHostname(std::string hostname) { m_sslHostname = hostname; }

        private:
            sockaddr_storage m_sockaddr;
            static std::string m_sslHostname;

            SSL* m_pSSL;
            SSL_CTX* m_pSSLCtx;

            /**
             * @brief Internal helper to initialize the TCP socket.
             */
            void init() noexcept(false)
            {
                m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
                if (m_fd < 0) {
                    LogError(LOG_NET, "Cannot create the TCP socket, err: %d", errno);
                    throw std::runtime_error("Cannot create the TCP socket");
                }

                int reuse = 1;
                if (::setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, (char*)& reuse, sizeof(reuse)) != 0) {
                    LogError(LOG_NET, "Cannot set the TCP socket option, err: %d", errno);
                    throw std::runtime_error("Cannot set the TCP socket option");
                }
            }

            /**
             * @brief Internal helper to initialize SSL.
             * @param sslCtx Instance of the OpenSSL context.
             */
            void initSsl(SSL_CTX* sslCtx) noexcept(false)
            {
                m_pSSL = SSL_new(sslCtx);
                if (m_pSSL == nullptr) {
                    LogError(LOG_NET, "Failed to create SSL client, %s err: %d", ERR_error_string(ERR_get_error(), NULL), errno);
                    throw std::runtime_error("Failed to create SSL client");
                }

                SSL_set_hostflags(m_pSSL, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
                if (!SSL_set1_host(m_pSSL, SecureTcpClient::m_sslHostname.c_str())) {
                    LogError(LOG_NET, "Failed to set SSL hostname, %s err: %d", ERR_error_string(ERR_get_error(), NULL), errno);
                    throw std::runtime_error("Failed to set SSL hostname");
                }

                SSL_set_verify(m_pSSL, SSL_VERIFY_NONE, NULL);
                SSL_set_fd(m_pSSL, m_fd);
            }
        };
    } // namespace tcp
} // namespace network

#endif // ENABLE_SSL

#endif // __SECURE_TCP_CLIENT_H__
