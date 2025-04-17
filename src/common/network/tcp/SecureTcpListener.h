// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
*
*/
/**
 * @file SecureTcpListener.h
 * @ingroup tcp_socket
 */
#if !defined(__SECURE_TCP_SERVER_H__)
#define __SECURE_TCP_SERVER_H__

#if defined(ENABLE_SSL)

#include "Defines.h"
#include "common/network/tcp/Socket.h"
#include "common/Log.h"
#include "common/network/tcp/SecureTcpClient.h"

#include <cassert>

#include <openssl/ssl.h>
#include <openssl/err.h>

namespace network
{
    namespace tcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a secure TCP server listener.
         * @ingroup tcp_socket
         */
        class HOST_SW_API SecureTcpListener : public Socket
        {
        public:
            auto operator=(SecureTcpListener&) -> SecureTcpListener& = delete;
            auto operator=(SecureTcpListener&&) -> SecureTcpListener& = delete;
            SecureTcpListener(SecureTcpListener&) = delete;

            /**
             * @brief Initializes a new instance of the SecureTcpListener class.
             * @param keyFile SSL certificate private key.
             * @param certFile SSL certificate.
             */
            SecureTcpListener(const std::string& keyFile, const std::string& certFile) : 
                m_pSSLCtx(nullptr), 
                m_keyFile(keyFile), 
                m_certFile(certFile)
            {
                assert(!m_certFile.empty());
                assert(!m_keyFile.empty());

                SSL_load_error_strings();
                OpenSSL_add_all_algorithms();
                const SSL_METHOD* method = SSLv23_server_method();
                m_pSSLCtx = SSL_CTX_new(method);
                if (m_pSSLCtx == nullptr) {
                    LogError(LOG_NET, "Cannot create server SSL context, %s err: %d", ERR_error_string(ERR_get_error(), NULL), errno);
                    throw std::runtime_error("Cannot create server SSL context");
                }

                m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
                if (m_fd < 0) {
                    LogError(LOG_NET, "Cannot create the TCP socket, err: %d", errno);
                    throw std::runtime_error("Cannot create the TCP socket");
                }
                
                int reuse = 1;
                if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char*)& reuse, sizeof(reuse)) != 0) {
                    LogError(LOG_NET, "Cannot set the TCP socket option, err: %d", errno);
                    throw std::runtime_error("Cannot set the TCP socket option");
                }

                initSecureFiles();
            }
            /**
             * @brief Initializes a new instance of the SecureTcpListener class.
             * @param keyFile SSL certificate private key.
             * @param certFile SSL certificate.
             * @param port Port to listen on.
             * @param address Address to listen on.
             */
            SecureTcpListener(const std::string& keyFile, const std::string& certFile, const uint16_t port, const std::string& address = "0.0.0.0") : SecureTcpListener(keyFile, certFile)
            {
                if (!bind(address, port)) {
                    LogError(LOG_NET, "Cannot to bind secure TCP server, err: %d", errno);
                    throw std::runtime_error("Cannot to bind secure TCP server");
                }
            }
            /**
             * @brief Finalizes a instance of the SecureTcpListener class.
             */
            ~SecureTcpListener() override
            {
                if (m_pSSLCtx != nullptr)
                    SSL_CTX_free(m_pSSLCtx);
            }

            /**
             * @brief Accept a new TCP connection either secure or unsecure.
             * @param nonBlocking Flag indicating accepted TCP connections should use non-blocking sockets.
             * @returns SecureTcpClient* Newly accepted TCP connection.
             */
            [[nodiscard]] SecureTcpClient* accept(bool nonBlocking = false)
            {
                sockaddr_in client = {};
                socklen_t clientLen = sizeof(client);
                int fd = Socket::accept(reinterpret_cast<sockaddr*>(&client), &clientLen);
                if (fd < 0) {
                    return nullptr;
                }

                return new SecureTcpClient(fd, m_pSSLCtx, client, clientLen, nonBlocking);
            }

        private:
            SSL_CTX* m_pSSLCtx;
            const std::string m_keyFile;
            const std::string m_certFile;
            
            /**
             * @brief Internal helper to initialize the SSL certificate.
             */
            void initSecureFiles()
            {
                if (SSL_CTX_use_certificate_file(m_pSSLCtx, m_certFile.c_str(), SSL_FILETYPE_PEM) != 1) {
                    ERR_print_errors_fp(stderr);
                    throw std::runtime_error("Failed to use PEM certificate file");
                }

                if (SSL_CTX_use_PrivateKey_file(m_pSSLCtx, m_keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
                    ERR_print_errors_fp(stderr);
                    throw std::runtime_error("Failed to use PEM private key file");
                }

                if (SSL_CTX_check_private_key(m_pSSLCtx) != 1) {
                    ERR_print_errors_fp(stderr);
                    throw std::runtime_error("Keys do not match!");
                }
            }
        };
    } // namespace tcp
} // namespace network

#endif // ENABLE_SSL

#endif // __SECURE_TCP_SERVER_H__
