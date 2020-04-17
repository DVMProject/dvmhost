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
*   Copyright (C) 2006-2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2020 by Bryan Biedenkapp N2PLL
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
#include "Defines.h"
#include "network/UDPSocket.h"
#include "Log.h"

using namespace network;

#include <cassert>

#if !defined(_WIN32) && !defined(_WIN64)
#include <cerrno>
#include <cstring>
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the UDPSocket class.
/// </summary>
/// <param name="address">Hostname/IP address to connect to.</param>
/// <param name="port">Port number.</param>
UDPSocket::UDPSocket(const std::string& address, uint32_t port) :
    m_address(address),
    m_port(port),
    m_fd(-1)
{
    assert(!address.empty());
#if defined(_WIN32) || defined(_WIN64)
    WSAData data;
    int wsaRet = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (wsaRet != 0)
        LogError(LOG_NET, "Error from WSAStartup");
#endif
}

/// <summary>
/// Initializes a new instance of the UDPSocket class.
/// </summary>
/// <param name="port">Port number.</param>
UDPSocket::UDPSocket(uint32_t port) :
    m_address(),
    m_port(port),
    m_fd(-1)
{
#if defined(_WIN32) || defined(_WIN64)
    WSAData data;
    int wsaRet = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (wsaRet != 0)
        LogError(LOG_NET, "Error from WSAStartup");
#endif
}

/// <summary>
/// Finalizes a instance of the UDPSocket class.
/// </summary>
UDPSocket::~UDPSocket()
{
#if defined(_WIN32) || defined(_WIN64)
    ::WSACleanup();
#endif
}

/// <summary>
/// Opens UDP socket connection.
/// </summary>
/// <returns>True, if UDP socket is opened, otherwise false.</returns>
bool UDPSocket::open()
{
    m_fd = ::socket(PF_INET, SOCK_DGRAM, 0);
    if (m_fd < 0) {
#if defined(_WIN32) || defined(_WIN64)
        LogError(LOG_NET, "Cannot create the UDP socket, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Cannot create the UDP socket, err: %d", errno);
#endif
        return false;
    }

    if (m_port > 0U) {
        sockaddr_in addr;
        ::memset(&addr, 0x00, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (!m_address.empty()) {
#if defined(_WIN32) || defined(_WIN64)
            addr.sin_addr.s_addr = ::inet_addr(m_address.c_str());
#else
            addr.sin_addr.s_addr = ::inet_addr(m_address.c_str());
#endif
            if (addr.sin_addr.s_addr == INADDR_NONE) {
                LogError(LOG_NET, "The local address is invalid - %s", m_address.c_str());
                return false;
            }
        }

        int reuse = 1;
        if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) == -1) {
#if defined(_WIN32) || defined(_WIN64)
            LogError(LOG_NET, "Cannot set the UDP socket option, err: %lu", ::GetLastError());
#else
            LogError(LOG_NET, "Cannot set the UDP socket option, err: %d", errno);
#endif
            return false;
        }

        if (::bind(m_fd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1) {
#if defined(_WIN32) || defined(_WIN64)
            LogError(LOG_NET, "Cannot bind the UDP address, err: %lu", ::GetLastError());
#else
            LogError(LOG_NET, "Cannot bind the UDP address, err: %d", errno);
#endif
            return false;
        }
    }

    return true;
}

/// <summary>
/// Read data from the UDP socket.
/// </summary>
/// <param name="buffer">Buffer to read data into.</param>
/// <param name="length">Length of data to read.</param>
/// <param name="address">IP address to read data from.</param>
/// <param name="port">Port number for remote UDP socket.</param>
/// <returns>Actual length of data read from remote UDP socket.</returns>
int UDPSocket::read(uint8_t* buffer, uint32_t length, in_addr& address, uint32_t& port)
{
    assert(buffer != NULL);
    assert(length > 0U);

    // Check that the readfrom() won't block
    fd_set readFds;
    FD_ZERO(&readFds);
#if defined(_WIN32) || defined(_WIN64)
    FD_SET((uint32_t)m_fd, &readFds);
#else
    FD_SET(m_fd, &readFds);
#endif

    // Return immediately
    timeval tv;
    tv.tv_sec = 0L;
    tv.tv_usec = 0L;

    int ret = ::select(m_fd + 1, &readFds, NULL, NULL, &tv);
    if (ret < 0) {
#if defined(_WIN32) || defined(_WIN64)
        LogError(LOG_NET, "Error returned from UDP select, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from UDP select, err: %d", errno);
#endif
        return -1;
    }

    if (ret == 0)
        return 0;

    sockaddr_in addr;
#if defined(_WIN32) || defined(_WIN64)
    int size = sizeof(sockaddr_in);
#else
    socklen_t size = sizeof(sockaddr_in);
#endif

#if defined(_WIN32) || defined(_WIN64)
    int len = ::recvfrom(m_fd, (char*)buffer, length, 0, (sockaddr *)&addr, &size);
#else
    ssize_t len = ::recvfrom(m_fd, (char*)buffer, length, 0, (sockaddr *)&addr, &size);
#endif
    if (len <= 0) {
#if defined(_WIN32) || defined(_WIN64)
        LogError(LOG_NET, "Error returned from recvfrom, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from recvfrom, err: %d", errno);
#endif
        return -1;
    }

    address = addr.sin_addr;
    port = ntohs(addr.sin_port);

    return len;
}

/// <summary>
/// Write data to the UDP socket.
/// </summary>
/// <param name="buffer">Buffer containing data to write to socket.</param>
/// <param name="length">Length of data to write.</param>
/// <param name="address">IP address to write data to.</param>
/// <param name="port">Port number for remote UDP socket.</param>
/// <returns>Actual length of data written to remote UDP socket.</returns>
bool UDPSocket::write(const uint8_t* buffer, uint32_t length, const in_addr& address, uint32_t port)
{
    assert(buffer != NULL);
    assert(length > 0U);

    sockaddr_in addr;
    ::memset(&addr, 0x00, sizeof(sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_addr = address;
    addr.sin_port = htons(port);

#if defined(_WIN32) || defined(_WIN64)
    int ret = ::sendto(m_fd, (char *)buffer, length, 0, (sockaddr *)&addr, sizeof(sockaddr_in));
#else
    ssize_t ret = ::sendto(m_fd, (char *)buffer, length, 0, (sockaddr *)&addr, sizeof(sockaddr_in));
#endif
    if (ret < 0) {
#if defined(_WIN32) || defined(_WIN64)
        LogError(LOG_NET, "Error returned from sendto, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from sendto, err: %d", errno);
#endif
        return false;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (ret != int(length))
        return false;
#else
    if (ret != ssize_t(length))
        return false;
#endif

    return true;
}

/// <summary>
/// Closes the UDP socket connection.
/// </summary>
void UDPSocket::close()
{
#if defined(_WIN32) || defined(_WIN64)
    ::closesocket(m_fd);
#else
    ::close(m_fd);
#endif
}

/// <summary>
/// Helper to lookup a hostname and resolve it to an IP address.
/// </summary>
/// <param name="hostname">String containing hostname to resolve.</param>
/// <returns>IP address structure for containing resolved hostname.</returns>
in_addr UDPSocket::lookup(const std::string& hostname)
{
    in_addr addr;
#if defined(_WIN32) || defined(_WIN64)
    unsigned long address = ::inet_addr(hostname.c_str());
    if (address != INADDR_NONE && address != INADDR_ANY) {
        addr.s_addr = address;
        return addr;
    }

    struct hostent* hp = ::gethostbyname(hostname.c_str());
    if (hp != NULL) {
        ::memcpy(&addr, hp->h_addr_list[0], sizeof(struct in_addr));
        return addr;
    }

    LogError(LOG_NET, "Cannot find address for host %s", hostname.c_str());

    addr.s_addr = INADDR_NONE;
    return addr;
#else
    in_addr_t address = ::inet_addr(hostname.c_str());
    if (address != in_addr_t(-1)) {
        addr.s_addr = address;
        return addr;
    }

    struct hostent* hp = ::gethostbyname(hostname.c_str());
    if (hp != NULL) {
        ::memcpy(&addr, hp->h_addr_list[0], sizeof(struct in_addr));
        return addr;
    }

    LogError(LOG_NET, "Cannot find address for host %s", hostname.c_str());

    addr.s_addr = INADDR_NONE;
    return addr;
#endif
}
