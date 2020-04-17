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
*   Copyright (C) 2009-2011,2013,2015,2016 by Jonathan Naylor G4KLX
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
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#else
#include <winsock.h>
#endif

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
        UDPSocket(const std::string& address, uint32_t port = 0U);
        /// <summary>Initializes a new instance of the UDPSocket class.</summary>
        UDPSocket(uint32_t port = 0U);
        /// <summary>Finalizes a instance of the UDPSocket class.</summary>
        ~UDPSocket();

        /// <summary>Opens UDP socket connection.</summary>
        bool open();

        /// <summary>Read data from the UDP socket.</summary>
        int  read(uint8_t* buffer, uint32_t length, in_addr& address, uint32_t& port);
        /// <summary>Write data to the UDP socket.</summary>
        bool write(const uint8_t* buffer, uint32_t length, const in_addr& address, uint32_t port);

        /// <summary>Closes the UDP socket connection.</summary>
        void close();

        /// <summary>Helper to lookup a hostname and resolve it to an IP address.</summary>
        static in_addr lookup(const std::string& hostName);

    private:
        std::string m_address;
        uint16_t m_port;
        int m_fd;
    };
} // namespace Net

#endif // __UDP_SOCKET_H__
