// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Converged FNE Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "fne/Defines.h"
#include "fne/network/influxdb/InfluxDB.h"

#if defined(_WIN32)
#include <ws2tcpip.h>
#include <Winsock2.h>
#endif

#include <fcntl.h>

using namespace network::influxdb;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define SOCK_CONNECT_TIMEOUT 30

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

ThreadPool detail::TSCaller::m_fluxReqThreadPool{MAX_INFLUXQL_THREAD_CNT, "fluxql"};

/* Generates a InfluxDB REST API request. */

int detail::inner::request(const char* method, const char* uri, const std::string& queryString, const std::string& body, 
    const ServerInfo& si) 
{
    std::string header;
    struct iovec iv[2];
    int fd, len = 0;

    struct addrinfo hints, *addr = nullptr;
    struct in6_addr serverAddr;
    memset(&hints, 0x00, sizeof(hints));
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // check to see if the address is a valid IPv4 address
    int ret = inet_pton(AF_INET, si.host().c_str(), &serverAddr);
    if (ret == 1) {
        hints.ai_family = AF_INET; // IPv4
        hints.ai_flags |= AI_NUMERICHOST;
        // not a valid IPv4 -> check to see if address is a valid IPv6 address
    } else {
        ret = inet_pton(AF_INET6, si.host().c_str(), &serverAddr);
        if (ret == 1) {
            hints.ai_family = AF_INET6;  // IPv6
            hints.ai_flags |= AI_NUMERICHOST;
        }
    }

    ret = getaddrinfo(si.host().c_str(), std::to_string(si.port()).c_str(), &hints, &addr);
    if (ret != 0) {
        ::LogError(LOG_HOST, "Failed to determine InfluxDB server host, err: %d", errno);
        return 1;
    }

    // open the socket
    fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (fd < 0) {
#if defined(_WIN32)
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %lu", ::GetLastError());
#else
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
#endif // defined(_WIN32)
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }

    // set SO_REUSEADDR option
    const int sockOptVal = 1;
#if defined(_WIN32)
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockOptVal, sizeof(int)) != 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %lu", ::GetLastError());
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }
#else
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockOptVal, sizeof(int)) < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }
#endif // defined(_WIN32)

    // setup socket for non-blocking operations
#if defined(_WIN32)
    u_long flags = 1;
    if (ioctlsocket(fd, FIONBIO, &flags) != 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed ioctlsocket, err: %d", errno);
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed fcntl(F_GETFL), err: %d", errno);
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed fcntl(F_SETFL), err: %d", errno);
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }
#endif // defined(_WIN32)

    fd_set fdset;
    struct timeval tv;

    // connect to the server
    uint8_t retryCnt = 0U;
    ret = connect(fd, addr->ai_addr, addr->ai_addrlen);
    if (ret < 0) {
        if (errno == EINPROGRESS) {
            do {
                tv.tv_sec = SOCK_CONNECT_TIMEOUT;
                tv.tv_usec = 0;

                FD_ZERO(&fdset);
                FD_SET(fd, &fdset);

                ret = select(fd + 1, NULL, &fdset, NULL, &tv);
                if (errno == EINTR) {
                    ++retryCnt;

                    if (retryCnt > 5U) {
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, timed out while connecting");
                        closesocket(fd);
                        if (addr != nullptr)
                            free(addr);
                        return 1;
                    }

                    Thread::sleep(1U);
                }

                if (ret < 0 && errno != EINTR) {
#if defined(_WIN32)
                    ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %lu", ::GetLastError());
#else
                    ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
#endif // defined(_WIN32)
                    closesocket(fd);
                    if (addr != nullptr)
                        free(addr);
                    return 1;
                } else if (ret > 0) {
#if !defined(_WIN32)
                    // socket selected for write
                    int valopt;
                    socklen_t slen = sizeof(int);

                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)(&valopt), &slen) < 0) {
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
                        closesocket(fd);
                        if (addr != nullptr)
                            free(addr);
                        return 1;
                    }

                    if (valopt) {
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", valopt);
                        closesocket(fd);
                        if (addr != nullptr)
                            free(addr);
                        return 1;
                    }
#endif // !defined(_WIN32)
                    break;
                } else {
                    ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, timed out while connecting");
                    closesocket(fd);
                    if (addr != nullptr)
                        free(addr);
                    return 1;
                }
            } while (true);
        }
    }

    // reset socket blocking operations
#if defined(_WIN32)
    flags = 0;
    if (ioctlsocket(fd, FIONBIO, &flags) != 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed ioctlsocket, err: %d", errno);
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }
#else
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed fcntl(F_GETFL), err: %d", errno);
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }

    if (fcntl(fd, F_SETFL, flags & (~O_NONBLOCK)) < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed fcntl(F_SETFL), err: %d", errno);
        closesocket(fd);
        if (addr != nullptr)
            free(addr);
        return 1;
    }
#endif // defined(_WIN32)

    // ensure the remaining TCP operations timeout
#if defined(_WIN32)
    int timeout = SOCK_CONNECT_TIMEOUT;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    tv.tv_sec = SOCK_CONNECT_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif // defined(_WIN32)

    header.resize(len = 0x100);
    while (true) {
        if (!si.token().empty()) {
            iv[0].iov_len = snprintf(&header[0], len,
                "%s /api/v2/%s?org=%s&bucket=%s%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nAuthorization: Token %s\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\n\r\n",
                method, uri, si.org().c_str(), si.bucket().c_str(), queryString.c_str(), si.host().c_str(), si.token().c_str(), (int)body.length());
        } else {
            iv[0].iov_len = snprintf(&header[0], len,
                "%s /api/v2/%s?org=%s&bucket=%s%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\n\r\n",
                method, uri, si.org().c_str(), si.bucket().c_str(), queryString.c_str(), si.host().c_str(), (int)body.length());
        }
#ifdef INFLUX_DEBUG
        LogDebug(LOG_HOST, "InfluxDB Request: %s\n%s", &header[0], body.c_str());
#endif
        if ((int)iv[0].iov_len >= len)
            header.resize(len *= 2);
        else
            break;
    }

    iv[0].iov_base = &header[0];
    iv[1].iov_base = (void*)&body[0];
    iv[1].iov_len = body.length();

    ret = 0;

    if (writev(fd, iv, 2) < (int)(iv[0].iov_len + iv[1].iov_len)) {
        ::LogError(LOG_HOST, "Failed to write statistical data to InfluxDB server, err: %d", errno);
        ret = -6;
    }

    // set SO_LINGER option
    struct linger sl;
    sl.l_onoff = 1;     /* non-zero value enables linger option in kernel */
    sl.l_linger = 0;    /* timeout interval in seconds */
#if defined(_WIN32)
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&sl, sizeof(sl));
#else
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));
#endif
    // close socket
    closesocket(fd);
    if (addr != nullptr)
        free(addr);
    return ret;
}
