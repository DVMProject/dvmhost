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
#if !defined(__TCP_SOCKET_H__)
#define __TCP_SOCKET_H__

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
            Socket();
            /// <summary>Initializes a new instance of the Socket class.</summary>
            Socket(const int fd) noexcept;
            /// <summary>Initializes a new instance of the Socket class.</summary>
            Socket(const int domain, const int type, const int protocol);
            /// <summary>Finalizes a instance of the Socket class.</summary>
            virtual ~Socket();

            /// <summary>Accepts a pending connection request.</summary>
            int accept(sockaddr* address, socklen_t* addrlen) noexcept;
            /// <summary>Connects the client to a remote TCP host using the specified host name and port number.</summary>
            virtual bool connect(const std::string& ipAddr, const uint16_t port);
            /// <summary>Starts listening for incoming connection requests with a maximum number of pending connection.</summary>
            ssize_t listen(const std::string& ipAddr, const uint16_t port, int backlog) noexcept;

            /// <summary>Read data from the socket.</summary>
            [[nodiscard]] virtual ssize_t read(uint8_t* buffer, size_t length) noexcept;
            /// <summary>Write data to the socket.</summary>
            virtual ssize_t write(const uint8_t* buffer, size_t length) noexcept;

            /// <summary></summary>
            static uint32_t addr(const sockaddr_storage& addr);
            /// <summary></summary>
            static std::string address(const sockaddr_storage& addr);
            /// <summary></summary>
            static uint16_t port(const sockaddr_storage& addr);

            /// <summary></summary>
            static bool isNone(const sockaddr_storage& addr);

        protected:
            std::string m_localAddress;
            uint16_t m_localPort;

            int m_fd;

            uint32_t m_counter;

            /// <summary></summary>
            bool initSocket(const int domain, const int type, const int protocol);
            /// <summary></summary>
            bool bind(const std::string& ipAddr, const uint16_t port);

            /// <summary>Helper to lookup a hostname and resolve it to an IP address.</summary>
            [[nodiscard]] static std::string getIpAddress(const in_addr inaddr);

            /// <summary>Initialize the sockaddr_in structure with the provided IP and port.</summary>
            static void initAddr(const std::string& ipAddr, const int port, sockaddr_in& addr);
        };
    } // namespace tcp
} // namespace network

#endif // __TCP_SOCKET_H__
