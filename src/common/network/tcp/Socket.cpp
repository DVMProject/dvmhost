// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
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

/* Initializes a new instance of the Socket class. */

Socket::Socket() : 
    m_localAddress(),
    m_localPort(0U),
#if defined(_WIN32)
    m_fd(INVALID_SOCKET),
#else
    m_fd(-1),
#endif // defined(_WIN32)
    m_counter(0U)
{
#if defined(_WIN32)
    WSAData data;
    int wsaRet = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (wsaRet != 0) {
        ::LogError(LOG_NET, "Error from WSAStartup, err: %d", wsaRet);
    }
#endif // defined(_WIN32)
}

/* Initializes a new instance of the Socket class. */

#if defined(_WIN32)
Socket::Socket(const SOCKET fd) noexcept :
#else
Socket::Socket(const int fd) noexcept :
#endif // defined(_WIN32)
    m_localAddress(),
    m_localPort(0U),
    m_fd(fd),
    m_counter(0U)
{
#if defined(_WIN32)
    WSAData data;
    int wsaRet = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (wsaRet != 0) {
        ::LogError(LOG_NET, "Error from WSAStartup, err: %d", wsaRet);
    }
#endif // defined(_WIN32)
}

/* Initializes a new instance of the Socket class. */

Socket::Socket(const int domain, const int type, const int protocol) : Socket()
{
    initSocket(domain, type, protocol);
}

/* Finalizes a instance of the Socket class. */

Socket::~Socket()
{
#if defined(_WIN32)
    ::shutdown(m_fd, SD_SEND);
    if (m_fd != INVALID_SOCKET) {
        ::closesocket(m_fd);
        m_fd = INVALID_SOCKET;
    }

    ::WSACleanup();
#else
    static_cast<void>(::shutdown(m_fd, SHUT_RDWR));
    static_cast<void>(::close(m_fd));
#endif // defined(_WIN32)
}

/* Accepts a pending connection request. */

int Socket::accept(sockaddr* address, socklen_t* addrlen) noexcept
{
    // check that the accept() won't block
    struct pollfd pfd;
    pfd.fd = m_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    // return immediately
#if defined(_WIN32)
    int ret = WSAPoll(&pfd, 1, 0);
#else
    int ret = ::poll(&pfd, 1, 0);
#endif // defined(_WIN32)
    if (ret < 0) {
#if defined(_WIN32)
        LogError(LOG_NET, "Error returned from TCP poll, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from TCP poll, err: %d (%s)", errno, strerror(errno));
#endif // defined(_WIN32)
        return -1;
    }

    if ((pfd.revents & POLLIN) == 0)
        return -1;

    return ::accept(pfd.fd, address, addrlen);
}

/* Connects the client to a remote TCP host using the specified host name and port number. */

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

/* Starts listening for incoming connection requests with a maximum number of pending connection. */

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

/* Read data from the socket. */

[[nodiscard]] ssize_t Socket::read(uint8_t* buffer, size_t length) noexcept
{
    assert(buffer != nullptr);
    assert(length > 0U);

    if (m_fd < 0)
        return -1;

    // check that the read() won't block
    struct pollfd pfd;
    pfd.fd = m_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    // return immediately
#if defined(_WIN32)
    int ret = WSAPoll(&pfd, 1, 0);
#else
    int ret = ::poll(&pfd, 1, 0);
#endif // defined(_WIN32)
    if (ret < 0) {
#if defined(_WIN32)
        LogError(LOG_NET, "Error returned from TCP poll, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from TCP poll, err: %d (%s)", errno, strerror(errno));
#endif // defined(_WIN32)
        return -1;
    }

    if ((pfd.revents & POLLIN) == 0)
        return 0;

    m_counter++;
#if defined(_WIN32)
    return ::recv(pfd.fd, (char*)buffer, length, 0);
#else
    return ::read(pfd.fd, (char*)buffer, length);
#endif // defined(_WIN32)
}

/* Write data to the socket. */

ssize_t Socket::write(const uint8_t* buffer, size_t length) noexcept
{
    assert(buffer != nullptr);
    assert(length > 0U);

    if (m_fd < 0)
        return -1;

#if defined(_WIN32)
    return ::send(m_fd, (char*)buffer, length, 0);
#else
    return ::send(m_fd, buffer, length, 0);
#endif // defined(_WIN32)
}

/* Gets the numeric representation of an address from a sockaddr_storage socket address structure. */

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

/* Gets the string representation of an address from a sockaddr_storage socket address structure. */

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

/* Gets the port from a sockaddr_storage socket address structure. */

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

/* Helper to check if the address stored in a sockaddr_storage socket address structure is INADDR_NONE. */

bool Socket::isNone(const sockaddr_storage& addr)
{
    struct sockaddr_in* in = (struct sockaddr_in*)& addr;

    return ((addr.ss_family == AF_INET) && (in->sin_addr.s_addr == htonl(INADDR_NONE)));
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to initialize the socket. */

bool Socket::initSocket(const int domain, const int type, const int protocol)
{
    m_fd = ::socket(domain, type, protocol);
#if defined(_WIN32)
    if (m_fd == INVALID_SOCKET) {
        LogError(LOG_NET, "Cannot create the TCP socket, err: %lu", ::GetLastError());
        return false;
    }
#else
    if (m_fd < 0) {
        LogError(LOG_NET, "Cannot create the TCP socket, err: %d (%s)", errno, strerror(errno));
        return false;
    }
#endif // defined(_WIN32)

    return true;
}

/* Internal helper to bind to a address and port. */

bool Socket::bind(const std::string& ipAddr, const uint16_t port)
{
    m_localAddress = std::string(ipAddr);
    m_localPort = port;

    sockaddr_in addr = {};
    initAddr(ipAddr, port, addr);

    socklen_t length = sizeof(addr);
    bool retval = true;
    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), length) < 0) {
#if defined(_WIN32)
        LogError(LOG_NET, "Cannot bind the TCP address, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Cannot bind the TCP address, err: %d (%s)", errno, strerror(errno));
#endif // defined(_WIN32)
        retval = false;
    }

    return retval;
}

/* Helper to lookup a hostname and resolve it to an IP address. */

[[nodiscard]] std::string Socket::getIpAddress(const in_addr inaddr)
{
    char* receivedAddr = ::inet_ntoa(inaddr);
    if (receivedAddr == reinterpret_cast<char*>(INADDR_NONE))
        throw std::runtime_error("Invalid IP address received on readfrom.");

    return { receivedAddr };
}

/* Initialize the sockaddr_in structure with the provided IP and port */

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

    addr.sin_port = htons(port);
}