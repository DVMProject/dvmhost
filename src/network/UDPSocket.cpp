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
*   Copyright (C) 2017-2020,2023 by Bryan Biedenkapp N2PLL
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
#include "Utils.h"

using namespace network;

#include <cassert>
#include <cerrno>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_BUFFER_COUNT 256

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the UDPSocket class.
/// </summary>
/// <param name="address">Hostname/IP address to connect to.</param>
/// <param name="port">Port number.</param>
UDPSocket::UDPSocket(const std::string& address, uint16_t port) :
    m_address_save(address),
    m_port_save(port),
    m_isOpen(false),
    m_counter(0U)
{
    for (int i = 0; i < UDP_SOCKET_MAX; i++) {
        m_address[i] = "";
        m_port[i] = 0U;
        m_af[i] = 0U;
        m_fd[i] = -1;
    }
}

/// <summary>
/// Initializes a new instance of the UDPSocket class.
/// </summary>
/// <param name="port">Port number.</param>
UDPSocket::UDPSocket(uint16_t port) :
    m_address_save(),
    m_port_save(port),
    m_isOpen(false),
    m_counter(0U)
{
    for (int i = 0; i < UDP_SOCKET_MAX; i++) {
        m_address[i] = "";
        m_port[i] = 0U;
        m_af[i] = 0U;
        m_fd[i] = -1;
    }
}

/// <summary>
/// Finalizes a instance of the UDPSocket class.
/// </summary>
UDPSocket::~UDPSocket()
{
    /* stub */
}

/// <summary>
/// Opens UDP socket connection.
/// </summary>
/// <param name="address"></param>
/// <returns>True, if UDP socket is opened, otherwise false.</returns>
bool UDPSocket::open(const sockaddr_storage& address)
{
    return open(address.ss_family);
}

/// <summary>
/// Opens UDP socket connection.
/// </summary>
/// <param name="af"></param>
/// <returns>True, if UDP socket is opened, otherwise false.</returns>
bool UDPSocket::open(uint32_t af)
{
    return open(0, af, m_address_save, m_port_save);
}

/// <summary>
/// Opens UDP socket connection.
/// </summary>
/// <param name="index"></param>
/// <param name="af"></param>
/// <param name="address"></param>
/// <param name="port"></param>
/// <returns>True, if UDP socket is opened, otherwise false.</returns>
bool UDPSocket::open(const uint32_t index, const uint32_t af, const std::string& address, const uint16_t port)
{
    sockaddr_storage addr;
    uint32_t addrlen;
    struct addrinfo hints;

    ::memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = af;

    /* to determine protocol family, call lookup() first. */
    int err = lookup(address, port, addr, addrlen, hints);
    if (err != 0) {
        LogError(LOG_NET, "The local address is invalid - %s", address.c_str());
        m_isOpen = false;
        return false;
    }

    close(index);

    int fd = ::socket(addr.ss_family, SOCK_DGRAM, 0);
    if (fd < 0) {
        LogError(LOG_NET, "Cannot create the UDP socket, err: %d", errno);
        m_isOpen = false;
        return false;
    }

    m_address[index] = address;
    m_port[index] = port;
    m_af[index] = addr.ss_family;
    m_fd[index] = fd;

    if (port > 0U) {
        int reuse = 1;
        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)& reuse, sizeof(reuse)) == -1) {
            LogError(LOG_NET, "Cannot set the UDP socket option, err: %d", errno);
            m_isOpen = false;
            return false;
        }

        if (::bind(fd, (sockaddr*)& addr, addrlen) == -1) {
            LogError(LOG_NET, "Cannot bind the UDP address, err: %d", errno);
            m_isOpen = false;
            return false;
        }

        LogInfoEx(LOG_NET, "Opening UDP port on %u", port);
    }

    m_isOpen = true;
    return true;
}

/// <summary>
/// Read data from the UDP socket.
/// </summary>
/// <param name="buffer">Buffer to read data into.</param>
/// <param name="length">Length of data to read.</param>
/// <param name="address">IP address data read from.</param>
/// <param name="addrLen"></param>
/// <returns>Actual length of data read from remote UDP socket.</returns>
int UDPSocket::read(uint8_t* buffer, uint32_t length, sockaddr_storage& address, uint32_t& addrLen)
{
    assert(buffer != nullptr);
    assert(length > 0U);

    // Check that the readfrom() won't block
    int i, n;
    struct pollfd pfd[UDP_SOCKET_MAX];
    for (i = n = 0; i < UDP_SOCKET_MAX; i++) {
        if (m_fd[i] >= 0) {
            pfd[n].fd = m_fd[i];
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
        LogError(LOG_NET, "Error returned from UDP poll, err: %d", errno);
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

    socklen_t size = sizeof(sockaddr_storage);
    ssize_t len = ::recvfrom(pfd[index].fd, (char*)buffer, length, 0, (sockaddr*)& address, &size);
    if (len <= 0) {
        LogError(LOG_NET, "Error returned from recvfrom, err: %d", errno);

        if (len == -1 && errno == ENOTSOCK) {
            LogMessage(LOG_NET, "Re-opening UDP port on %u", m_port[index]);
            close();
            open();
        }

        return -1;
    }

    m_counter++;
    addrLen = size;
    return len;
}

/// <summary>
/// Write data to the UDP socket.
/// </summary>
/// <param name="buffer">Buffer containing data to write to socket.</param>
/// <param name="length">Length of data to write.</param>
/// <param name="address">IP address to write data to.</param>
/// <param name="addrLen"></param>
/// <param name="lenWritten">Total number of bytes written.</param>
/// <returns>True if message was sent, otherwise false.</returns>
bool UDPSocket::write(const uint8_t* buffer, uint32_t length, const sockaddr_storage& address, uint32_t addrLen, int* lenWritten)
{
    assert(buffer != nullptr);
    assert(length > 0U);

    bool result = false;

    for (int i = 0; i < UDP_SOCKET_MAX; i++) {
        if (m_fd[i] < 0 || m_af[i] != address.ss_family)
            continue;

        ssize_t sent = ::sendto(m_fd[i], (char*)buffer, length, 0, (sockaddr*)& address, addrLen);
        if (sent < 0) {
            LogError(LOG_NET, "Error returned from sendto, err: %d", errno);

            if (lenWritten != nullptr) {
                *lenWritten = -1;
            }
        }
        else {
            if (sent == ssize_t(length))
                result = true;

            if (lenWritten != nullptr) {
                *lenWritten = sent;
            }
        }
    }

    return result;
}

/// <summary>
/// Write data to the UDP socket.
/// </summary>
/// <param name="buffers">Vector of buffers to write to socket.</param>
/// <param name="address">IP address to write data to.</param>
/// <param name="addrLen"></param>
/// <param name="lenWritten">Total number of bytes written.</param>
/// <returns>True if messages were sent, otherwise false.</returns>
bool UDPSocket::write(BufferVector& buffers, int* lenWritten)
{
    bool result = false;
    if (buffers.empty()) {
        LogError(LOG_NET, "Trying to send empty buffers?");
        return false;
    }

    if (buffers.size() > UINT16_MAX) {
        LogError(LOG_NET, "Trying to send too many buffers?");
        return false;
    }

    // LogDebug(LOG_NET, "Sending message(s) (to %s:%u) addrLen %u", UDPSocket::address(address).c_str(), UDPSocket::port(address), addrLen);

    int sent = 0;
    struct mmsghdr headers[MAX_BUFFER_COUNT];
    struct iovec chunks[MAX_BUFFER_COUNT];

    // create mmsghdrs from input buffers and send them at once
    int size = buffers.size();
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers.at(i) == nullptr) {
            --size;
            continue;
        }

        chunks[i].iov_len = buffers.at(i)->length;
        chunks[i].iov_base = buffers.at(i)->buffer;
        sent += buffers.at(i)->length;

        headers[i].msg_hdr.msg_name = (void*)&buffers.at(i)->address;
        headers[i].msg_hdr.msg_namelen = buffers.at(i)->addrLen;
        headers[i].msg_hdr.msg_iov = &chunks[i];
        headers[i].msg_hdr.msg_iovlen = 1;
        headers[i].msg_hdr.msg_control = 0;
        headers[i].msg_hdr.msg_controllen = 0;
    }

    for (int i = 0; i < UDP_SOCKET_MAX; i++) {
        if (m_fd[i] < 0)
            continue;

        bool skip = false;
        for (auto& buffer : buffers) {
            if (m_af[i] != buffer->address.ss_family) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;

        if (sendmmsg(m_fd[i], headers, size, 0) < 0) {
            LogError(LOG_NET, "Error returned from sendmmsg, err: %d", errno);
            if (lenWritten != nullptr) {
                *lenWritten = -1;
            }
        }

        if (sent < 0) {
            LogError(LOG_NET, "Error returned from sendmmsg, err: %d", errno);
            if (lenWritten != nullptr) {
                *lenWritten = -1;
            }
        }
        else {
            result = true;
            if (lenWritten != nullptr) {
                *lenWritten = sent;
            }
        }
    }

    return result;
}

/// <summary>
/// Closes the UDP socket connection.
/// </summary>
void UDPSocket::close()
{
    for (int i = 0; i < UDP_SOCKET_MAX; i++)
        close(i);
    m_isOpen = false;
}

/// <summary>
/// Closes the UDP socket connection.
/// </summary>
/// <param name="index"></param>
void UDPSocket::close(const uint32_t index)
{
    if ((index < UDP_SOCKET_MAX) && (m_fd[index] >= 0)) {
        ::close(m_fd[index]);
        m_fd[index] = -1;
    }
}

/// <summary>
/// Helper to lookup a hostname and resolve it to an IP address.
/// </summary>
/// <param name="hostname">String containing hostname to resolve.</param>
/// <param name="port">Numeric port number of service to resolve.</param>
/// <param name="addr">Socket address structure.</param>
/// <param name="addrLen"></param>
/// <returns>Zero if no error during lookup, otherwise error.</returns>
int UDPSocket::lookup(const std::string& hostname, uint16_t port, sockaddr_storage& addr, uint32_t& addrLen)
{
    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));

    return lookup(hostname, port, addr, addrLen, hints);
}

/// <summary>
/// Helper to lookup a hostname and resolve it to an IP address.
/// </summary>
/// <param name="hostname">String containing hostname to resolve.</param>
/// <param name="port">Numeric port number of service to resolve.</param>
/// <param name="addr">Socket address structure.</param>
/// <param name="addrLen"></param>
/// <param name="hints"></param>
/// <returns>Zero if no error during lookup, otherwise error.</returns>
int UDPSocket::lookup(const std::string& hostname, uint16_t port, sockaddr_storage& addr, uint32_t& addrLen, struct addrinfo& hints)
{
    std::string portstr = std::to_string(port);
    struct addrinfo* res;

    // port is always digits, no needs to lookup service
    hints.ai_flags |= AI_NUMERICSERV;

    int err = getaddrinfo(hostname.empty() ? NULL : hostname.c_str(), portstr.c_str(), &hints, &res);
    if (err != 0) {
        sockaddr_in* paddr = (sockaddr_in*)& addr;
        ::memset(paddr, 0x00U, addrLen = sizeof(sockaddr_in));
        paddr->sin_family = AF_INET;
        paddr->sin_port = htons(port);
        paddr->sin_addr.s_addr = htonl(INADDR_NONE);
        LogError(LOG_NET, "Cannot find address for host %s", hostname.c_str());
        return err;
    }

    ::memcpy(&addr, res->ai_addr, addrLen = res->ai_addrlen);

    freeaddrinfo(res);

    return 0;
}

/// <summary>
///
/// </summary>
/// <param name="addr1"></param>
/// <param name="addr2"></param>
/// <param name="type"></param>
/// <returns></returns>
bool UDPSocket::match(const sockaddr_storage& addr1, const sockaddr_storage& addr2, IPMATCHTYPE type)
{
    if (addr1.ss_family != addr2.ss_family)
        return false;

    if (type == IMT_ADDRESS_AND_PORT) {
        switch (addr1.ss_family) {
        case AF_INET:
            struct sockaddr_in* in_1, * in_2;
            in_1 = (struct sockaddr_in*) & addr1;
            in_2 = (struct sockaddr_in*) & addr2;
            return (in_1->sin_addr.s_addr == in_2->sin_addr.s_addr) && (in_1->sin_port == in_2->sin_port);
        case AF_INET6:
            struct sockaddr_in6* in6_1, *in6_2;
            in6_1 = (struct sockaddr_in6*) & addr1;
            in6_2 = (struct sockaddr_in6*) & addr2;
            return IN6_ARE_ADDR_EQUAL(&in6_1->sin6_addr, &in6_2->sin6_addr) && (in6_1->sin6_port == in6_2->sin6_port);
        default:
            return false;
        }
    }
    else if (type == IMT_ADDRESS_ONLY) {
        switch (addr1.ss_family) {
        case AF_INET:
            struct sockaddr_in* in_1, * in_2;
            in_1 = (struct sockaddr_in*) & addr1;
            in_2 = (struct sockaddr_in*) & addr2;
            return in_1->sin_addr.s_addr == in_2->sin_addr.s_addr;
        case AF_INET6:
            struct sockaddr_in6* in6_1, * in6_2;
            in6_1 = (struct sockaddr_in6*) & addr1;
            in6_2 = (struct sockaddr_in6*) & addr2;
            return IN6_ARE_ADDR_EQUAL(&in6_1->sin6_addr, &in6_2->sin6_addr);
        default:
            return false;
        }
    }
    else {
        return false;
    }
}

/// <summary>
///
/// </summary>
/// <param name="addr"></param>
/// <returns></returns>
std::string UDPSocket::address(const sockaddr_storage& addr)
{
    std::string address = std::string();
    char str[INET_ADDRSTRLEN];

    switch (addr.ss_family) {
    case AF_INET:
    {
        struct sockaddr_in* in;
        in = (struct sockaddr_in*) & addr;
        inet_ntop(AF_INET, &(in->sin_addr), str, INET_ADDRSTRLEN);
        address = std::string(str);
    }
    break;
    case AF_INET6:
    {
        struct sockaddr_in6* in6;
        in6 = (struct sockaddr_in6*) & addr;
        inet_ntop(AF_INET6, &(in6->sin6_addr), str, INET_ADDRSTRLEN);
        address = std::string(str);
    }
    break;
    default:
        break;
    }

    return address;
}

/// <summary>
///
/// </summary>
/// <param name="addr"></param>
/// <returns></returns>
uint16_t UDPSocket::port(const sockaddr_storage& addr)
{
    uint16_t port = 0U;

    switch (addr.ss_family) {
    case AF_INET:
    {
        struct sockaddr_in* in;
        in = (struct sockaddr_in*) & addr;
        port = ntohs(in->sin_port);
    }
    break;
    case AF_INET6:
    {
        struct sockaddr_in6* in6;
        in6 = (struct sockaddr_in6*) & addr;
        port = ntohs(in6->sin6_port);
    }
    break;
    default:
        break;
    }

    return port;
}

/// <summary>
///
/// </summary>
/// <param name="addr"></param>
/// <returns></returns>
bool UDPSocket::isNone(const sockaddr_storage& addr)
{
    struct sockaddr_in* in = (struct sockaddr_in*) & addr;

    return ((addr.ss_family == AF_INET) && (in->sin_addr.s_addr == htonl(INADDR_NONE)));
}
