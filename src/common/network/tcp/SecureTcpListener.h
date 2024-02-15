// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__SECURE_TCP_SERVER_H__)
#define __SECURE_TCP_SERVER_H__

#if defined(ENABLE_TCP_SSL)

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
        //      Implements a secure TCP server listener.
        // ---------------------------------------------------------------------------

        class HOST_SW_API SecureTcpListener : public Socket
        {
        public:
            auto operator=(SecureTcpListener&) -> SecureTcpListener& = delete;
            auto operator=(SecureTcpListener&&) -> SecureTcpListener& = delete;
            SecureTcpListener(SecureTcpListener&) = delete;

            /// <summary>Initializes a new instance of the SecureTcpListener class.</summary>
            /// <param name="keyFile"></param>
            /// <param name="certFile"></param>
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
            /// <summary>Initializes a new instance of the SecureTcpListener class.</summary>
            /// <param name="keyFile"></param>
            /// <param name="certFile"></param>
            /// <param name="port"></param>
            /// <param name="address"></param>
            SecureTcpListener(const std::string& keyFile, const std::string& certFile, const uint16_t port, const std::string& address = "0.0.0.0") : SecureTcpListener(keyFile, certFile)
            {
                if (!bind(address, port)) {
                    LogError(LOG_NET, "Cannot to bind secure TCP server, err: %d", errno);
                    throw std::runtime_error("Cannot to bind secure TCP server");
                }
            }
            /// <summary>Finalizes a instance of the SecureTcpListener class.</summary>
            ~SecureTcpListener() override
            {
                if (m_pSSLCtx != nullptr)
                    SSL_CTX_free(m_pSSLCtx);
            }

            /// <summary>
            /// Accept a new TCP connection either secure or unsecure.
            /// </summary>
            /// <returns>Newly accepted TCP connection</returns>
            [[nodiscard]] SecureTcpClient* accept()
            {
                sockaddr_in client = {};
                socklen_t clientLen = sizeof(client);
                int fd = Socket::accept(reinterpret_cast<sockaddr*>(&client), &clientLen);
                if (fd < 0) {
                    return nullptr;
                }

                return new SecureTcpClient(fd, m_pSSLCtx, client, clientLen);
            }

        private:
            SSL_CTX* m_pSSLCtx;
            const std::string m_keyFile;
            const std::string m_certFile;
            
            /// <summary>
            /// 
            /// </summary>
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

#endif // ENABLE_TCP_SSL

#endif // __SECURE_TCP_SERVER_H__
