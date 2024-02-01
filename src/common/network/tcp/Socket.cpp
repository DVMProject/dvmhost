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
#include "Defines.h"
#include "network/tcp/Socket.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::tcp;

#include <cassert>
#include <cerrno>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Socket class.
/// </summary>
Socket::Socket() : 
    m_localAddress(),
    m_localPort(0U),
    m_fd(-1),
    m_counter(0U)
{
    /* stub */
}

/// <summary>
/// Initializes a new instance of the Socket class.
/// </summary>
/// <param name="fd"></param>
Socket::Socket(const int fd) noexcept :
    m_localAddress(),
    m_localPort(0U),
    m_fd(fd),
    m_counter(0U)
{
    /* stub */
}

/// <summary>
/// Initializes a new instance of the Socket class.
/// </summary>
/// <param name="domain"></param>
/// <param name="type"></param>
/// <param name="protocol"></param>
Socket::Socket(const int domain, const int type, const int protocol) : Socket()
{
    initSocket(domain, type, protocol);
}

/// <summary>
/// Finalizes a instance of the Socket class.
/// </summary>
Socket::~Socket()
{
    static_cast<void>(::shutdown(m_fd, SHUT_RDWR));
    static_cast<void>(::close(m_fd));
}

/// <summary>
/// Accepts a pending connection request.
/// </summary>
/// <param name="address"></param>
/// <param name="addrlen"></param>
/// <returns></returns>
int Socket::accept(sockaddr* address, socklen_t* addrlen) noexcept
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
/// Connects the client to a remote TCP host using the specified host name and port number.
/// </summary>
/// <param name="ipAddr"></param>
/// <param name="port"></param>
/// <returns></returns>
bool Socket::connect(const std::string& ipAddr, const uint16_t port)
{
    sockaddr_in addr = {};
    initAddr(ipAddr, port, addr);

    m_localAddress = ipAddr;
    m_localPort = port;

    socklen_t length = sizeof(addr);
    bool retval = true;
    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), length) < 0)
        retval = false;

    return retval;
}

/// <summary>
/// Starts listening for incoming connection requests with a maximum number of pending connection.
/// </summary>
/// <param name="ipAddr"></param>
/// <param name="port"></param>
/// <param name="backlog"></param>
/// <returns></returns>
ssize_t Socket::listen(const std::string& ipAddr, const uint16_t port, int backlog) noexcept
{
    m_localAddress = ipAddr;
    m_localPort = port;

    if (!bind(m_localAddress, m_localPort)) {
        return -1;
    }

    LogInfoEx(LOG_NET, "Listening TCP port on %u", m_localPort);
    return ::listen(m_fd, backlog);
}

/// <summary>
/// Read data from the socket.
/// </summary>
/// <param name="buffer">Buffer to read data into.</param>
/// <param name="length">Length of data to read.</param>
/// <returns></returns>
[[nodiscard]] ssize_t Socket::read(uint8_t* buffer, size_t length) noexcept
{
    assert(buffer != nullptr);
    assert(length > 0U);

    if (m_fd < 0)
        return -1;

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

    m_counter++;
    return ::read(pfd[index].fd, (char*)buffer, length);
}

/// <summary>
/// Write data to the socket.
/// </summary>
/// <param name="buffer">Buffer containing data to write to socket.</param>
/// <param name="length">Length of data to write.</param>
/// <returns></returns>
ssize_t Socket::write(const uint8_t* buffer, size_t length) noexcept
{
    assert(buffer != nullptr);
    assert(length > 0U);

    if (m_fd < 0)
        return -1;

    return ::send(m_fd, buffer, length, 0);
}

/// <summary>
///
/// </summary>
/// <param name="addr"></param>
/// <returns></returns>
uint32_t Socket::addr(const sockaddr_storage& addr)
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

/// <summary>
///
/// </summary>
/// <param name="addr"></param>
/// <returns></returns>
std::string Socket::address(const sockaddr_storage& addr)
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

/// <summary>
///
/// </summary>
/// <param name="addr"></param>
/// <returns></returns>
uint16_t Socket::port(const sockaddr_storage& addr)
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

/// <summary>
///
/// </summary>
/// <param name="addr"></param>
/// <returns></returns>
bool Socket::isNone(const sockaddr_storage& addr)
{
    struct sockaddr_in* in = (struct sockaddr_in*)& addr;

    return ((addr.ss_family == AF_INET) && (in->sin_addr.s_addr == htonl(INADDR_NONE)));
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="domain"></param>
/// <param name="type"></param>
/// <param name="protocol"></param>
bool Socket::initSocket(const int domain, const int type, const int protocol)
{
    m_fd = ::socket(domain, type, protocol);
    if (m_fd < 0) {
        LogError(LOG_NET, "Cannot create the TCP socket, err: %d", errno);
        return false;
    }

    return true;
}

/// <summary>
///
/// </summary>
/// <param name="ipAddr"></param>
/// <param name="port"></param>
/// <returns></returns>
bool Socket::bind(const std::string& ipAddr, const uint16_t port)
{
    m_localAddress = std::string(ipAddr);
    m_localPort = port;

    sockaddr_in addr = {};
    initAddr(ipAddr, port, addr);

    socklen_t length = sizeof(addr);
    bool retval = true;
    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), length) < 0) {
        LogError(LOG_NET, "Cannot bind the TCP address, err: %d", errno);
        retval = false;
    }

    return retval;
}

/// <summary>
/// Helper to lookup a hostname and resolve it to an IP address.
/// </summary>
/// <param name="inaddr">String containing hostname to resolve.</param>
/// <returns></returns>
[[nodiscard]] std::string Socket::getIpAddress(const in_addr inaddr)
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
void Socket::initAddr(const std::string& ipAddr, const int port, sockaddr_in& addr) noexcept(false)
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