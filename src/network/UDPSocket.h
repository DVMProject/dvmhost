/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2006-2016,2020 by Jonathan Naylor G4KLX
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
#if !defined(__UDP_SOCKET_H__)
#define __UDP_SOCKET_H__

#include "Defines.h"

#include <string>

#if !defined(_WIN32) && !defined(_WIN64)
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#if !defined(UDP_SOCKET_MAX)
#define UDP_SOCKET_MAX	1
#endif

enum IPMATCHTYPE {
    IMT_ADDRESS_AND_PORT,
    IMT_ADDRESS_ONLY
};

namespace network
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements low-level routines to communicate over a UDP
    //      network socket.
    // ---------------------------------------------------------------------------

    class HOST_SW_API UDPSocket {
    public:
        /// <summary>Initializes a new instance of the UDPSocket class.</summary>
        UDPSocket(const std::string& address, uint16_t port = 0U);
        /// <summary>Initializes a new instance of the UDPSocket class.</summary>
        UDPSocket(uint16_t port = 0U);
        /// <summary>Initializes a new instance of the UDPSocket class.</summary>
        ~UDPSocket();

        /// <summary>Opens UDP socket connection.</summary>
        bool open(uint32_t af = AF_UNSPEC);
        /// <summary>Opens UDP socket connection.</summary>
        bool open(const sockaddr_storage& address);
        /// <summary>Opens UDP socket connection.</summary>
        bool open(const uint32_t index, const uint32_t af, const std::string& address, const uint16_t port);

        /// <summary>Read data from the UDP socket.</summary>
        int read(uint8_t* buffer, uint32_t length, sockaddr_storage& address, uint32_t& addrLen);
        /// <summary>Write data to the UDP socket.</summary>
        bool write(const uint8_t* buffer, uint32_t length, const sockaddr_storage& address, uint32_t addrLen);

        /// <summary>Closes the UDP socket connection.</summary>
        void close();
        /// <summary>Closes the UDP socket connection.</summary>
        void close(const uint32_t index);

        /// <summary></summary>
        static void startup();
        /// <summary></summary>
        static void shutdown();

        /// <summary>Helper to lookup a hostname and resolve it to an IP address.</summary>
        static int lookup(const std::string& hostName, uint16_t port, sockaddr_storage& address, uint32_t& addrLen);
        /// <summary>Helper to lookup a hostname and resolve it to an IP address.</summary>
        static int lookup(const std::string& hostName, uint16_t port, sockaddr_storage& address, uint32_t& addrLen, struct addrinfo& hints);

        /// <summary></summary>
        static bool match(const sockaddr_storage& addr1, const sockaddr_storage& addr2, IPMATCHTYPE type = IMT_ADDRESS_AND_PORT);
        /// <summary></summary>
        static std::string address(const sockaddr_storage& addr);

        /// <summary></summary>
        static bool isNone(const sockaddr_storage& addr);

    private:
        std::string m_address_save;
        uint16_t m_port_save;
        std::string m_address[UDP_SOCKET_MAX];
        uint16_t m_port[UDP_SOCKET_MAX];

        uint32_t m_af[UDP_SOCKET_MAX];
        int m_fd[UDP_SOCKET_MAX];
        
        uint32_t m_counter;
    };
} // namespace network

#endif // __UDP_SOCKET_H__
