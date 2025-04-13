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

uint32_t detail::TSCaller::m_currThreadCnt = 0U;

/* Generates a InfluxDB REST API request. */

int detail::inner::request(const char* method, const char* uri, const std::string& queryString, const std::string& body, 
    const ServerInfo& si, std::string* resp) 
{
    std::string header;
    struct iovec iv[2];
    int fd, contentLength = 0, len = 0;
    char ch;
    unsigned char chunked = 0;

    if (resp)
        resp->clear();

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
        return 1;
    }

    // set SO_REUSEADDR option
    const int sockOptVal = 1;
#if defined(_WIN32)
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockOptVal, sizeof(int)) != 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %lu", ::GetLastError());
        closesocket(fd);
        return 1;
    }
#else
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockOptVal, sizeof(int)) < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
        closesocket(fd);
        return 1;
    }
#endif // defined(_WIN32)

    // setup socket for non-blocking operations
#if defined(_WIN32)
    u_long flags = 1;
    if (ioctlsocket(fd, FIONBIO, &flags) != 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed ioctlsocket, err: %d", errno);
        closesocket(fd);
        return 1;
    }
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed fcntl(F_GETFL), err: %d", errno);
        closesocket(fd);
        return 1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed fcntl(F_SETFL), err: %d", errno);
        closesocket(fd);
        return 1;
    }
#endif // defined(_WIN32)

    fd_set fdset;
    struct timeval tv;

    // connect to the server
    ret = connect(fd, addr->ai_addr, addr->ai_addrlen);
    if (ret < 0) {
        if (errno == EINPROGRESS) {
            do {
                tv.tv_sec = SOCK_CONNECT_TIMEOUT;
                tv.tv_usec = 0;

                FD_ZERO(&fdset);
                FD_SET(fd, &fdset);

                ret = select(fd + 1, NULL, &fdset, NULL, &tv);
                if (ret < 0 && errno != EINTR) {
#if defined(_WIN32)
                    ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %lu", ::GetLastError());
#else
                    ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
#endif // defined(_WIN32)
                    closesocket(fd);
                    return 1;
                } else if (ret > 0) {
#if !defined(_WIN32)
                    // socket selected for write
                    int valopt;
                    socklen_t slen = sizeof(int);

                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)(&valopt), &slen) < 0) {
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
                        closesocket(fd);
                        return 1;
                    }

                    if (valopt) {
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", valopt);
                        closesocket(fd);
                        return 1;
                    }
#endif // !defined(_WIN32)
                    break;
                } else {
                    ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, timed out while connecting");
                    closesocket(fd);
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
        return 1;
    }
#else
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed fcntl(F_GETFL), err: %d", errno);
        closesocket(fd);
        return 1;
    }

    if (fcntl(fd, F_SETFL, flags & (~O_NONBLOCK)) < 0) {
        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, failed fcntl(F_SETFL), err: %d", errno);
        closesocket(fd);
        return 1;
    }
#endif // defined(_WIN32)

    // ensure the remaining TCP operations timeout
#if defined(_WIN32)
    int sendTimeout = SOCK_CONNECT_TIMEOUT;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sendTimeout, sizeof(sendTimeout));
#else
    tv.tv_sec = SOCK_CONNECT_TIMEOUT;
    tv.tv_usec = 0;
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

    if (writev(fd, iv, 2) < (int)(iv[0].iov_len + iv[1].iov_len)) {
        ret = -6;
        goto END;
    }

    iv[0].iov_len = len;

#define _NO_MORE() (len >= (int)iv[0].iov_len && (iv[0].iov_len = recv(fd, &header[0], header.length(), len = 0)) == size_t(-1))
#define _GET_NEXT_CHAR() (ch = _NO_MORE() ? 0 : header[len++])
#define _LOOP_NEXT(statement) for(;;) { if(!(_GET_NEXT_CHAR())) { ret = -7; goto END; } statement }
#define _UNTIL(c) _LOOP_NEXT( if(ch == c) break; )
#define _GET_NUMBER(n) _LOOP_NEXT( if(ch >= '0' && ch <= '9') n = n * 10 + (ch - '0'); else break; )
#define _GET_CHUNKED_LEN(n, c) _LOOP_NEXT( if(ch >= '0' && ch <= '9') n = n * 16 + (ch - '0'); \
else if(ch >= 'A' && ch <= 'F') n = n * 16 + (ch - 'A') + 10; \
else if(ch >= 'a' && ch <= 'f') n = n * 16 + (ch - 'a') + 10; else {if(ch != c) { ret = -8; goto END; } break;} )
#define _(c) if((_GET_NEXT_CHAR()) != c) break;
#define __(c) if((_GET_NEXT_CHAR()) != c) { ret = -9; goto END; }

    if (resp)
        resp->clear();

    _UNTIL(' ')_GET_NUMBER(ret)
    while (true) {
        _UNTIL('\n')
        switch (_GET_NEXT_CHAR()) {
            case 'C':_('o')_('n')_('t')_('e')_('n')_('t')_('-')
                _('L')_('e')_('n')_('g')_('t')_('h')_(':')_(' ')
                _GET_NUMBER(contentLength)
                break;
            case 'T':_('r')_('a')_('n')_('s')_('f')_('e')_('r')_('-')
                _('E')_('n')_('c')_('o')_('d')_('i')_('n')_('g')_(':')
                _(' ')_('c')_('h')_('u')_('n')_('k')_('e')_('d')
                chunked = 1;
                break;
            case '\r':__('\n')
                switch (chunked) {
                    do {__('\r')__('\n')
                    case 1:
                        _GET_CHUNKED_LEN(contentLength, '\r')__('\n')
                        if (!contentLength) {
                            __('\r')__('\n')
                            goto END;
                        }
                    case 0:
                        while (contentLength > 0 && !_NO_MORE()) {
                            //contentLength -= (iv[1].iov_len = std::min(contentLength, (int)iv[0].iov_len - len));
                            contentLength -= (iv[1].iov_len = (((contentLength) < ((int)iv[0].iov_len - len)) ? (contentLength) : ((int)iv[0].iov_len - len)));
                            if (resp)
                                resp->append(&header[len], iv[1].iov_len);
                            len += iv[1].iov_len;
                        }
                    } while(chunked);
                }
                goto END;
        }

        if (!ch) {
            ret = -10;
            goto END;
        }
    }

    ret = -11;
END:
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
    return ret / 100 == 2 ? 0 : ret;
#undef _NO_MORE
#undef _GET_NEXT_CHAR
#undef _LOOP_NEXT
#undef _UNTIL
#undef _GET_NUMBER
#undef _GET_CHUNKED_LEN
#undef _
#undef __
}
