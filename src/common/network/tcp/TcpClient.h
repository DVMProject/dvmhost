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
#if !defined(__TCP_CLIENT_H__)
#define __TCP_CLIENT_H__

#include "Defines.h"
#include "common/network/tcp/Socket.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

namespace network
{
    namespace tcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements a TCP client.
        // ---------------------------------------------------------------------------

        class HOST_SW_API TcpClient : public Socket
        {
        public:
            auto operator=(TcpClient&) -> TcpClient& = delete;
            auto operator=(TcpClient&&) -> TcpClient& = delete;
            TcpClient(TcpClient&) = delete;

            /// <summary>Initializes a new instance of the TcpClient class.</summary>
            TcpClient() noexcept(false) : Socket(AF_INET, SOCK_STREAM, 0)
            {
                init();
            }
            /// <summary>Initializes a new instance of the TcpClient class.</summary>
            /// <param name="fd"></param>
            /// <param name="client"></param>
            /// <param name="clientLen"></param>
            TcpClient(const int fd, sockaddr_in& client, int clientLen) noexcept(false) : Socket(fd),
                m_sockaddr()
            {
                ::memcpy(reinterpret_cast<char*>(&m_sockaddr), reinterpret_cast<char*>(&client), clientLen);
            }
            /// <summary>Initializes a new instance of the TcpClient class.</summary>
            /// <param name="address"></param>
            /// <param name="port"></param>
            TcpClient(const std::string& address, const uint16_t port) noexcept(false) : TcpClient()
            {
                assert(!address.empty());
                assert(port > 0U);

                sockaddr_in addr = {};
                initAddr(address, port, addr);

                ::memcpy(reinterpret_cast<char*>(&m_sockaddr), reinterpret_cast<char*>(&addr), sizeof(addr));

                int ret = ::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
                if (ret < 0) {
                    LogError(LOG_NET, "Failed to connect to server, err: %d", errno);
                }
            }

            /// <summary></summary>
            /// <returns></returns>
            sockaddr_storage getAddress() const { return m_sockaddr; }

        protected:
            sockaddr_storage m_sockaddr;

            /// <summary>
            /// 
            /// </summary>
            void init() noexcept(false)
            {
                int reuse = 1;
                if (::setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, (char*)& reuse, sizeof(reuse)) != 0) {
                    LogError(LOG_NET, "Cannot set the TCP socket option, err: %d", errno);
                    throw std::runtime_error("Cannot set the TCP socket option");
                }
            }
        };
    } // namespace tcp
} // namespace network

#endif // __TCP_CLIENT_H__
