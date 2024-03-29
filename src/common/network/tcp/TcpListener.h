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
        //      Implements a TCP server listener.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TcpListener : public Socket
        {
        public:

            auto operator=(TcpListener&) -> TcpListener& = delete;
            auto operator=(TcpListener&&) -> TcpListener& = delete;
            TcpListener(TcpListener&) = delete;

            /// <summary>Initializes a new instance of the TcpListener class.</summary>
            TcpListener() noexcept(false) : Socket(AF_INET, SOCK_STREAM, 0)
            {
                int reuse = 1;
                if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char*)& reuse, sizeof(reuse)) != 0) {
                    LogError(LOG_NET, "Cannot set the TCP socket option, err: %d", errno);
                    throw std::runtime_error("Cannot set the TCP socket option");
                }
            }
            /// <summary>Initializes a new instance of the TcpListener class.</summary>
            /// <param name="port"></param>
            /// <param name="ipAddr"></param>
            explicit TcpListener(const uint16_t port, const std::string& ipAddr = "0.0.0.0") noexcept(false) : TcpListener()
            {
                if (!bind(ipAddr, port)) {
                    LogError(LOG_NET, "Cannot to bind TCP server, err: %d", errno);
                    throw std::runtime_error("Cannot to bind TCP server");
                }
            }
            /// <summary>Initializes a new instance of the TcpListener class.</summary>
            /// <param name="ipAddr"></param>
            /// <param name="port"></param>
            /// <param name="backlog"></param>
            TcpListener(const std::string& ipAddr, const uint16_t port, const int backlog) noexcept(false) : TcpListener(port, ipAddr)
            {
                if (listen(ipAddr, port, backlog) < 0) {
                    LogError(LOG_NET, "Failed to listen on TCP server, err: %d", errno);
                    throw std::runtime_error("Failed to listen on TCP server.");
                }
            }

            /// <summary>
            /// Accept a new TCP connection either secure or unsecure.
            /// </summary>
            /// <returns>Newly accepted TCP connection</returns>
            [[nodiscard]] TcpClient* accept() noexcept(false)
            {
                sockaddr_in client = {};
                socklen_t clientLen = sizeof(client);
                int fd = Socket::accept(reinterpret_cast<sockaddr*>(&client), &clientLen);
                if (fd < 0) {
                    return nullptr;
                }

                return new TcpClient(fd, client, clientLen);
            }
        };
    } // namespace tcp
} // namespace network

#endif // __TCP_LISTENER_H__
