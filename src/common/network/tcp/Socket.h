/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
/*
*   Copyright (C) 2024 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__SOCKET_H__)
#define __SOCKET_H__

#include "Defines.h"
#include "common/Log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#include <cstdlib>
#include <ctime>

#if !defined(TCP_SOCKET_MAX)
#define TCP_SOCKET_MAX	1
#endif

namespace network
{
    namespace tcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements the core network socket.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Socket
        {
        public:
            auto operator=(Socket&) -> Socket& = delete;
            auto operator=(Socket&&) -> Socket& = delete;
            Socket(Socket&) = delete;

            /// <summary>Initializes a new instance of the Socket class.</summary>
            Socket() noexcept(false) : 
                m_fd(-1),
                m_address(),
                m_port(0U),
                m_counter(0U)
            {
                /* stub */
            }
            /// <summary>Initializes a new instance of the Socket class.</summary>
            /// <param name="fd"></param>
            Socket(const int fd) noexcept :
                m_fd(fd),
                m_address(),
                m_port(0U),
                m_counter(0U)
            {
                /* stub */
            }
            /// <summary>Initializes a new instance of the Socket class.</summary>
            /// <param name="domain"></param>
            /// <param name="type"></param>
            /// <param name="protocol"></param>
            Socket(const int domain, const int type, const int protocol) noexcept(false) : Socket()
            {
                initSocket(domain, type, protocol);
            }
            /// <summary>Finalizes a instance of the Socket class.</summary>
            virtual ~Socket()
            {
                static_cast<void>(::shutdown(m_fd, SHUT_RDWR));
                static_cast<void>(::close(m_fd));
            }

            /// <summary>
            ///
            /// </summary>
            /// <param name="domain"></param>
            /// <param name="type"></param>
            /// <param name="protocol"></param>
            void initSocket(const int domain, const int type, const int protocol) noexcept(false)
            {
                m_fd = ::socket(domain, type, protocol);
                if (m_fd < 0)
                    throw std::runtime_error("Failed to create Socket");
            }

            /// <summary>
            ///
            /// </summary>
            /// <param name="address"></param>
            /// <param name="addrlen"></param>
            /// <returns></returns>
            int accept(sockaddr* address, socklen_t* addrlen) const noexcept
            {
                // check that the accept() won't block
                int i, n;
                struct pollfd pfd[TCP_SOCKET_MAX];
                for (i = n = 0; i < TCP_SOCKET_MAX; i++) {
                    if (m_fd >= 0) {
                        pfd[n].fd = m_fd;
                        pfd[n].events = POLLIN;
                        n++;
                    }
                }

                // no socket descriptor to receive
                if (n == 0)
                    return 0;

                // Return immediately
                int ret = ::poll(pfd, n, 0);
                if (ret < 0) {
                    LogError(LOG_NET, "Error returned from TCP poll, err: %d", errno);
                    return -1;
                }

                int index;
                for (i = 0; i < n; i++) {
                    // round robin
                    index = (i + m_counter) % n;
                    if (pfd[index].revents & POLLIN)
                        break;
                }
                if (i == n)
                    return -1;

                return ::accept(pfd[index].fd, address, addrlen);
            }

            /// <summary>
            ///
            /// </summary>
            /// <param name="ipAddr"></param>
            /// <param name="port"></param>
            /// <returns></returns>
            bool bind(const std::string& ipAddr, const uint16_t port) noexcept(false)
            {
                m_address = std::string(ipAddr);
                m_port = port;

                sockaddr_in addr = {};
                initAddr(ipAddr, port, addr);
                socklen_t length = sizeof(addr);
                bool retval = true;
                if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), length) < 0)
                    retval = false;

                return retval;
            }

            /// <summary>
            ///
            /// </summary>
            /// <param name="ipAddr"></param>
            /// <param name="port"></param>
            /// <returns></returns>
            virtual bool connect(const std::string& ipAddr, const uint16_t port) noexcept(false)
            {
                sockaddr_in addr = {};
                initAddr(ipAddr, port, addr);
                socklen_t length = sizeof(addr);
                bool retval = true;
                if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), length) < 0)
                    retval = false;
                return retval;
            }

            /// <summary>
            ///
            /// </summary>
            /// <param name="backlog"></param>
            /// <returns></returns>
            ssize_t listen(int backlog) const noexcept
            {
                LogInfoEx(LOG_NET, "Listening TCP port on %u", m_port);
                return ::listen(m_fd, backlog);
            }

            /// <summary>
            /// Read data from the socket.
            /// </summary>
            /// <param name="buffer">Buffer to read data into.</param>
            /// <param name="length">Length of data to read.</param>
            /// <returns></returns>
            [[nodiscard]] ssize_t read(uint8_t* buffer, size_t length) const noexcept
            {
                // check that the read() won't block
                int i, n;
                struct pollfd pfd[TCP_SOCKET_MAX];
                for (i = n = 0; i < TCP_SOCKET_MAX; i++) {
                    if (m_fd >= 0) {
                        pfd[n].fd = m_fd;
                        pfd[n].events = POLLIN;
                        n++;
                    }
                }

                // no socket descriptor to receive
                if (n == 0)
                    return 0;

                // Return immediately
                int ret = ::poll(pfd, n, 0);
                if (ret < 0) {
                    LogError(LOG_NET, "Error returned from TCP poll, err: %d", errno);
                    return -1;
                }

                int index;
                for (i = 0; i < n; i++) {
                    // round robin
                    index = (i + m_counter) % n;
                    if (pfd[index].revents & POLLIN)
                        break;
                }
                if (i == n)
                    return 0;

                return ::read(pfd[index].fd, (char*)buffer, length);
            }

            /// <summary>
            /// Write data to the socket.
            /// </summary>
            /// <param name="buffer">Buffer containing data to write to socket.</param>
            /// <param name="length">Length of data to write.</param>
            /// <returns></returns>
            ssize_t write(const uint8_t* buffer, size_t length) const noexcept
            {
                return ::send(m_fd, buffer, length, 0);
            }

            /// <summary></summary>
            static uint32_t addr(const sockaddr_storage& addr)
            {
                switch (addr.ss_family) {
                case AF_INET:
                {
                    struct sockaddr_in* in;
                    in = (struct sockaddr_in*)& addr;
                    return in->sin_addr.s_addr;
                }
                break;
                case AF_INET6:
                default:
                    return -1;
                    break;
                }
            }
            /// <summary></summary>
            static std::string address(const sockaddr_storage& addr)
            {
                std::string address = std::string();
                char str[INET_ADDRSTRLEN];

                switch (addr.ss_family) {
                case AF_INET:
                {
                    struct sockaddr_in* in;
                    in = (struct sockaddr_in*)& addr;
                    inet_ntop(AF_INET, &(in->sin_addr), str, INET_ADDRSTRLEN);
                    address = std::string(str);
                }
                break;
                case AF_INET6:
                {
                    struct sockaddr_in6* in6;
                    in6 = (struct sockaddr_in6*)& addr;
                    inet_ntop(AF_INET6, &(in6->sin6_addr), str, INET_ADDRSTRLEN);
                    address = std::string(str);
                }
                break;
                default:
                    break;
                }

                return address;
            }
            /// <summary></summary>
            static uint16_t port(const sockaddr_storage& addr)
            {
                uint16_t port = 0U;

                switch (addr.ss_family) {
                case AF_INET:
                {
                    struct sockaddr_in* in;
                    in = (struct sockaddr_in*)& addr;
                    port = ntohs(in->sin_port);
                }
                break;
                case AF_INET6:
                {
                    struct sockaddr_in6* in6;
                    in6 = (struct sockaddr_in6*)& addr;
                    port = ntohs(in6->sin6_port);
                }
                break;
                default:
                    break;
                }

                return port;
            }

            /// <summary></summary>
            static bool isNone(const sockaddr_storage& addr)
            {
                struct sockaddr_in* in = (struct sockaddr_in*)& addr;

                return ((addr.ss_family == AF_INET) && (in->sin_addr.s_addr == htonl(INADDR_NONE)));
            }

        protected:
            int m_fd;
            std::string m_address;
            uint16_t m_port;

            uint32_t m_counter;

            /// <summary>
            /// Helper to lookup a hostname and resolve it to an IP address.
            /// </summary>
            /// <param name="inaddr">String containing hostname to resolve.</param>
            /// <returns></returns>
            [[nodiscard]] static std::string getIpAddress(const in_addr inaddr) noexcept(false)
            {
                char* receivedAddr = ::inet_ntoa(inaddr);
                if (receivedAddr == reinterpret_cast<char*>(INADDR_NONE))
                    throw std::runtime_error("Invalid IP address received on readfrom.");

                return { receivedAddr };
            }

            /// <summary>
            /// Initialize the sockaddr_in structure with the provided IP and port
            /// </summary>
            /// <param name="ipAddr">IP address.</param>
            /// <param name="port">IP address.</param>
            /// <param name="addr"></param>
            static void initAddr(const std::string& ipAddr, const int port, sockaddr_in& addr) noexcept(false)
            {
                addr.sin_family = AF_INET;
                if (ipAddr.empty() || ipAddr == "0.0.0.0")
                    addr.sin_addr.s_addr = INADDR_ANY;
                else
                {
                    if (::inet_pton(AF_INET, ipAddr.c_str(), &addr.sin_addr) <= 0)
                        throw std::runtime_error("Failed to parse IP address");
                }

                addr.sin_port = ::htons(port);
            }
        };
    } // namespace tcp
} // namespace network

#endif // __SOCKET_H__
