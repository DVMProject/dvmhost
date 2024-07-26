// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TcpListener.h
 * @ingroup tcp_socket
 */
#if !defined(__TCP_LISTENER_H__)
#define __TCP_LISTENER_H__

#include "Defines.h"
#include "common/network/tcp/Socket.h"
#include "common/network/tcp/TcpClient.h"
#include "common/Log.h"

namespace network
{
    namespace tcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a TCP server listener.
         * @ingroup tcp_socket
         */
        class HOST_SW_API TcpListener : public Socket
        {
        public:

            auto operator=(TcpListener&) -> TcpListener& = delete;
            auto operator=(TcpListener&&) -> TcpListener& = delete;
            TcpListener(TcpListener&) = delete;

            /**
             * @brief Initializes a new instance of the TcpListener class.
             */
            TcpListener() noexcept(false) : Socket(AF_INET, SOCK_STREAM, 0)
            {
                int reuse = 1;
#if defined(_WIN32)
                if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) != 0) {
                    LogError(LOG_NET, "Cannot set the TCP socket option, err: %lu", ::GetLastError());
                    throw std::runtime_error("Cannot set the TCP socket option");
                }
#else
                if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char*)& reuse, sizeof(reuse)) != 0) {
                    LogError(LOG_NET, "Cannot set the TCP socket option, err: %d", errno);
                    throw std::runtime_error("Cannot set the TCP socket option");
                }
#endif // defined(_WIN32)
            }
            /**
             * @brief Initializes a new instance of the TcpListener class.
             * @param port Port to listen on.
             * @param address Address to listen on.
             */
            explicit TcpListener(const uint16_t port, const std::string& address = "0.0.0.0") noexcept(false) : TcpListener()
            {
                if (!bind(address, port)) {
#if defined(_WIN32)
                    LogError(LOG_NET, "Cannot to bind TCP server, err: %lu", ::GetLastError());
#else
                    LogError(LOG_NET, "Cannot to bind TCP server, err: %d", errno);
#endif // defined(_WIN32)
                    throw std::runtime_error("Cannot to bind TCP server");
                }
            }
            /**
             * @brief Initializes a new instance of the TcpListener class.
             * @param ipAddr IP address.
             * @param port Port.
             * @param backlog 
             */
            TcpListener(const std::string& ipAddr, const uint16_t port, const int backlog) noexcept(false) : TcpListener(port, ipAddr)
            {
                if (listen(ipAddr, port, backlog) < 0) {
#if defined(_WIN32)
                    LogError(LOG_NET, "Failed to listen on TCP server, err: %lu", ::GetLastError());
#else
                    LogError(LOG_NET, "Failed to listen on TCP server, err: %d", errno);
#endif // defined(_WIN32)
                    throw std::runtime_error("Failed to listen on TCP server.");
                }
            }

            /**
             * @brief Accept a new TCP connection either secure or unsecure.
             * @returns TcpClient* Newly accepted TCP connection.
             */
            [[nodiscard]] TcpClient* accept() noexcept(false)
            {
                sockaddr_in client = {};
                socklen_t clientLen = sizeof(client);
#if defined(_WIN32)
                SOCKET fd = Socket::accept(reinterpret_cast<sockaddr*>(&client), &clientLen);
                if (fd == INVALID_SOCKET) {
                    return nullptr;
                }
#else
                int fd = Socket::accept(reinterpret_cast<sockaddr*>(&client), &clientLen);
                if (fd < 0) {
                    return nullptr;
                }
#endif // defined(_WIN32)

                return new TcpClient(fd, client, clientLen);
            }
        };
    } // namespace tcp
} // namespace network

#endif // __TCP_LISTENER_H__
