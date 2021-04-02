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
        UDPSocket(const std::string& address, unsigned int port = 0U);
        /// <summary>Initializes a new instance of the UDPSocket class.</summary>
        UDPSocket(unsigned int port = 0U);
        /// <summary>Initializes a new instance of the UDPSocket class.</summary>
        ~UDPSocket();

        /// <summary>Opens UDP socket connection.</summary>
        bool open(unsigned int af = AF_UNSPEC);
        /// <summary>Opens UDP socket connection.</summary>
        bool open(const sockaddr_storage& address);
        /// <summary>Opens UDP socket connection.</summary>
        bool open(const unsigned int index, const unsigned int af, const std::string& address, const unsigned int port);

        /// <summary>Read data from the UDP socket.</summary>
        int read(unsigned char* buffer, unsigned int length, sockaddr_storage& address, unsigned int& addrLen);
        /// <summary>Write data to the UDP socket.</summary>
        bool write(const unsigned char* buffer, unsigned int length, const sockaddr_storage& address, unsigned int addrLen);

        /// <summary>Closes the UDP socket connection.</summary>
        void close();
        /// <summary>Closes the UDP socket connection.</summary>
        void close(const unsigned int index);

        /// <summary></summary>
        static void startup();
        /// <summary></summary>
        static void shutdown();

        /// <summary>Helper to lookup a hostname and resolve it to an IP address.</summary>
        static int lookup(const std::string& hostName, unsigned int port, sockaddr_storage& address, unsigned int& addrLen);
        /// <summary>Helper to lookup a hostname and resolve it to an IP address.</summary>
        static int lookup(const std::string& hostName, unsigned int port, sockaddr_storage& address, unsigned int& addrLen, struct addrinfo& hints);

        /// <summary></summary>
        static bool match(const sockaddr_storage& addr1, const sockaddr_storage& addr2, IPMATCHTYPE type = IMT_ADDRESS_AND_PORT);

        /// <summary></summary>
        static bool isNone(const sockaddr_storage& addr);

    private:
        std::string    m_address_save;
        unsigned short m_port_save;
        std::string    m_address[UDP_SOCKET_MAX];
        unsigned short m_port[UDP_SOCKET_MAX];
        unsigned int   m_af[UDP_SOCKET_MAX];
        int            m_fd[UDP_SOCKET_MAX];
        unsigned int   m_counter;
    };
} // namespace network

#endif // __UDP_SOCKET_H__
