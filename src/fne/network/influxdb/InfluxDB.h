// SPDX-License-Identifier: MIT-only
/*
 * Digital Voice Modem - Converged FNE Software
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2010-2018 <http://ez8.co> <orca.zhang@yahoo.com>
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup fne_influx FNE InfluxDB
 * @brief Implementation for the FNE InfluxDB support.
 * @ingroup fne
 * 
 * @file InfluxDB.h
 * @ingroup fne_influx
 */
#if !defined(__INFLUXDB_H__)
#define __INFLUXDB_H__

#include "fne/Defines.h"
#include "common/Log.h"
#include "common/Thread.h"

#include <sstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#define DEFAULT_PRECISION 5

#ifdef _WIN32
    #define NOMINMAX
    #include <ws2tcpip.h>
    #include <Winsock2.h>
    #include <windows.h>
    #include <algorithm>

    #pragma comment(lib, "ws2_32")
    typedef struct iovec { void* iov_base; size_t iov_len; } iovec;

    inline __int64 writev(int sock, struct iovec* iov, int cnt) {
        __int64 r = send(sock, (const char*)iov->iov_base, iov->iov_len, 0);
        return (r < 0 || cnt == 1) ? r : r + writev(sock, iov + 1, cnt - 1);
    }
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/uio.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #define closesocket close
#endif

namespace network
{
    namespace influxdb
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        #define MAX_INFLUXQL_THREAD_CNT 75U // this is a really extreme number of pending queries...

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements the diagnostic/activity log networking logic.
         * @ingroup fne_influx
         */
        class HOST_SW_API ServerInfo {
        public:
            /**
             * @brief Initializes a new instance of the ServerInfo class.
             */
            ServerInfo() :
                m_host(),
                m_port(8086U),
                m_org(),
                m_bucket(),
                m_token()
            {
                /* stub */
            }

            /**
             * @brief Initializes a new instance of the ServerInfo class.
             * @param host Hostname/IP Address.
             * @param port Port number.
             * @param org Organization.
             * @param token Token.
             * @param bucket Bucket.
             */
            ServerInfo(const std::string& host, uint16_t port, const std::string& org, const std::string& token, const std::string& bucket = "") :
                m_host(host),
                m_port(port),
                m_org(org),
                m_bucket(bucket),
                m_token(token)
            {
                /* stub */
            }

        public:
            /**
             * @brief Hostname/IP Address.
             */
            __PROPERTY_PLAIN(std::string, host);
            /**
             * @brief Port.
             */
            __PROPERTY_PLAIN(uint16_t, port);
            /**
             * @brief Organization.
             */
            __PROPERTY_PLAIN(std::string, org);
            /**
             * @brief Bucket.
             */
            __PROPERTY_PLAIN(std::string, bucket);
            /**
             * @brief Token.
             */
            __PROPERTY_PLAIN(std::string, token);
        };

        namespace detail
        {
            struct MeasCaller;
            struct TagCaller;
            struct FieldCaller;
            struct TSCaller;

            /**
             * @brief 
             * @ingroup fne_influx
             */
            struct HOST_SW_API inner {
                /**
                 * @brief Generates a InfluxDB REST API request.
                 * @param method HTTP Method.
                 * @param uri URI.
                 * @param queryString Query.
                 * @param body Content body.
                 * @param si 
                 * @param resp 
                 * @returns int 
                 */
                static int request(const char* method, const char* uri, const std::string& queryString, const std::string& body, 
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
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
                        closesocket(fd);
                        return 1;
                    }

                    // set SO_REUSEADDR option
                    const int sockOptVal = 1;
#if defined(_WIN32)
                    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockOptVal, sizeof(int)) < 0) {
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
                        closesocket(fd);
                        return 1;
                    }
#else
                    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockOptVal, sizeof(int)) < 0) {
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
                        closesocket(fd);
                        return 1;
                    }
#endif
                    // connect to the server
                    ret = connect(fd, addr->ai_addr, addr->ai_addrlen);
                    if (ret < 0) {
                        ::LogError(LOG_HOST, "Failed to connect to InfluxDB server, err: %d", errno);
                        closesocket(fd);
                        return 1;
                    }

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
            
            private:
                /**
                 * @brief Helper to convert a value to hexadecimal.
                 * @param x 
                 * @returns uint8_t 
                 */
                static inline uint8_t toHex(uint8_t x) { return  x > 9 ? x + 55 : x + 48; }

                /**
                 * @brief Helper to properly HTTP encode a URL.
                 * @param out 
                 * @param src 
                 */
                static void urlEncode(std::string& out, const std::string& src)
                {
                    size_t pos = 0, start = 0;
                    while((pos = src.find_first_not_of("abcdefghijklmnopqrstuvwxyqABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~", start)) != std::string::npos) {
                        out.append(src.c_str() + start, pos - start);

                        if(src[pos] == ' ')
                            out += "+";
                        else {
                            out += '%';
                            out += toHex((uint8_t)src[pos] >> 4);
                            out += toHex((uint8_t)src[pos] & 0xF);
                        }

                        start = ++pos;
                    }

                    out.append(src.c_str() + start, src.length() - start);
                }
            };

            /**
             * @brief Helper to generate a InfluxDB query.
             * @param resp 
             * @param query 
             * @param si 
             */
            inline int fluxQL(std::string& resp, const std::string& query, const ServerInfo& si) 
            {
                // query JSON body
                std::stringstream body;
                body << "{\"query\": \"";
                body << query;
                body << "\", \"type\": \"flux\" }";

                return detail::inner::request("POST", "query", "", body.str(), si, &resp);
            }
        } // namespace detail

        // ---------------------------------------------------------------------------
        //  Structure Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief 
         * @ingroup fne_influx
         */
        struct HOST_SW_API QueryBuilder {
        public:
            /**
             * @brief
             * @param m 
             * @return 
             */
            detail::TagCaller& meas(const std::string& m) {
                m_lines.imbue(std::locale("C"));
                m_lines.clear();
                return this->m(m);
            }

        protected:
            /**
             * @brief
             * @param m 
             * @return 
             */
            detail::TagCaller& m(const std::string& m) {
                escape(m, ", ");
                return (detail::TagCaller&)*this;
            }

            /**
             * @brief
             * @param k 
             * @param v 
             * @return 
             */
            detail::TagCaller& t(const std::string& k, const std::string& v) {
                m_lines << ",";

                escape(k, ",= ");
                m_lines << '=';
                escape(std::string(v.c_str()), ",= ");

                return (detail::TagCaller&)*this;
            }

            /**
             * @brief
             * @param delim 
             * @param k 
             * @param v 
             * @return 
             */
            detail::FieldCaller& f_s(char delim, const std::string& k, const std::string& v) {
                m_lines << delim;
                m_lines << std::fixed;

                escape(k, ",= ");
                m_lines << "=\"";
                escape(std::string(v.c_str()), "\"");
                m_lines << "\"";

                return (detail::FieldCaller&)*this;
            }

            /**
             * @brief
             * @param delim 
             * @param k 
             * @param v 
             * @return 
             */
            detail::FieldCaller& f_i(char delim, const std::string& k, long long v) {
                m_lines << delim;
                m_lines << std::fixed;

                escape(k, ",= ");
                m_lines << "=";
                m_lines << v << "i";

                return (detail::FieldCaller&)*this;
            }

            /**
             * @brief
             * @param delim 
             * @param k 
             * @param v 
             * @return 
             */
            detail::FieldCaller& f_ui(char delim, const std::string& k, unsigned long long v) {
                m_lines << delim;
                m_lines << std::fixed;

                escape(k, ",= ");
                m_lines << "=";
                m_lines << v << "i";

                return (detail::FieldCaller&)*this;
            }

            /**
             * @brief
             * @param delim 
             * @param k 
             * @param v 
             * @param prec 
             * @return 
             */
            detail::FieldCaller& f_f(char delim, const std::string& k, double v, int prec) {
                m_lines << delim;

                escape(k, ",= ");
                m_lines << std::fixed;
                m_lines.precision(prec);
                m_lines << "=" << v;

                return (detail::FieldCaller&)*this;
            }

            /**
             * @brief
             * @param delim 
             * @param k 
             * @param v 
             * @return 
             */
            detail::FieldCaller& f_b(char delim, const std::string& k, bool v) {
                m_lines << delim;

                escape(k, ",= ");
                m_lines << std::fixed;
                m_lines << "=" << (v ? "t" : "f");

                return (detail::FieldCaller&)*this;
            }

            /**
             * @brief
             * @param ts 
             * @return 
             */
            detail::TSCaller& ts(uint64_t ts) {
                m_lines << " " << ts;
                return (detail::TSCaller&)*this;
            }

            /**
             * @brief
             * @param src 
             * @param escapeSeq 
             */
            void escape(const std::string& src, const char* escapeSeq) 
            {
                size_t pos = 0, start = 0;

                while ((pos = src.find_first_of(escapeSeq, start)) != std::string::npos) {
                    m_lines.write(src.c_str() + start, pos - start);
                    m_lines << "\\" << src[pos];
                    start = ++pos;
                }

                m_lines.write(src.c_str() + start, src.length() - start);
            }

            std::stringstream m_lines;
        };

        namespace detail {
            // ---------------------------------------------------------------------------
            //  Structure Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief 
             * @ingroup fne_influx
             */
            struct HOST_SW_API TagCaller : public QueryBuilder 
            {
                detail::TagCaller& tag(const std::string& k, const std::string& v)       { return t(k, v); }
                detail::FieldCaller& field(const std::string& k, const std::string& v)   { return f_s(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, bool v)                 { return f_b(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, short v)                { return f_i(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, int v)                  { return f_i(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, long v)                 { return f_i(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, uint16_t v)             { return f_ui(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, uint32_t v)             { return f_ui(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, uint64_t v)             { return f_ui(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, long long v)            { return f_i(' ', k, v); }
                detail::FieldCaller& field(const std::string& k, double v, int prec = DEFAULT_PRECISION) { return f_f(' ', k, v, prec); }

            private:
                detail::TagCaller& meas(const std::string& m);
            };

            // ---------------------------------------------------------------------------
            //  Structure Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Represents the data required for a voice service packet handler thread.
             * @ingroup rc_network
             */
            struct TSCallerRequest : thread_t {
                ServerInfo si;         //!
                std::string lines;     //!
            };

            // ---------------------------------------------------------------------------
            //  Structure Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief 
             * @ingroup fne_influx
             */
            struct HOST_SW_API TSCaller : public QueryBuilder
            {                
                detail::TagCaller& meas(const std::string& m)                   { m_lines << '\n'; return this->m(m); }
                int request(const ServerInfo& si, std::string* resp = nullptr)  { return detail::inner::request("POST", "write", "", m_lines.str(), si, resp); }
                int requestAsync(const ServerInfo& si) 
                {
                    if (m_currThreadCnt >= MAX_INFLUXQL_THREAD_CNT) {
                        ::LogError(LOG_HOST, "Maximum concurrent FluxQL thread count reached, dropping request!");
                        return 1;
                    }
            
                    TSCallerRequest* req = new TSCallerRequest();
                    req->obj = this;

                    req->si = ServerInfo(si.host(), si.port(), si.org(), si.token(), si.bucket());
                    req->lines = std::string(m_lines.str());

                    if (!Thread::runAsThread(this, threadedRequest, req)) {
                        delete req;
                        return 1;
                    } else {
                        m_currThreadCnt++;
                    }
            
                    return 0; 
                }

            private:
                static uint32_t m_currThreadCnt;

                /**
                 * @brief 
                 */
                static void* threadedRequest(void* arg)
                {
                    TSCallerRequest* req = (TSCallerRequest*)arg;
                    if (req != nullptr) {
                #if defined(_WIN32)
                        ::CloseHandle(req->thread);
                #else
                        ::pthread_detach(req->thread);
                #endif // defined(_WIN32)

                #ifdef _GNU_SOURCE
                        ::pthread_setname_np(req->thread, "fluxql:request");
                #endif // _GNU_SOURCE

                        if (req == nullptr) {
                            m_currThreadCnt--;
                            return nullptr;
                        }

                        TSCaller* caller = static_cast<TSCaller*>(req->obj);
                        if (caller == nullptr) {
                            if (req != nullptr) {
                                delete req;
                            }

                            m_currThreadCnt--;
                            return nullptr;
                        }

                        const ServerInfo& si = req->si;
                        detail::inner::request("POST", "write", "", req->lines, si, nullptr);

                        delete req;
                    }

                    m_currThreadCnt--;
                    return nullptr;
                }
            };

            // ---------------------------------------------------------------------------
            //  Structure Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief 
             * @ingroup fne_influx
             */

            struct HOST_SW_API FieldCaller : public TSCaller 
            {
                detail::FieldCaller& field(const std::string& k, const std::string& v)   { return f_s(',', k, v); }
                detail::FieldCaller& field(const std::string& k, bool v)                 { return f_b(',', k, v); }
                detail::FieldCaller& field(const std::string& k, short v)                { return f_i(',', k, v); }
                detail::FieldCaller& field(const std::string& k, int v)                  { return f_i(',', k, v); }
                detail::FieldCaller& field(const std::string& k, long v)                 { return f_i(',', k, v); }
                detail::FieldCaller& field(const std::string& k, uint16_t v)             { return f_ui(',', k, v); }
                detail::FieldCaller& field(const std::string& k, uint32_t v)             { return f_ui(',', k, v); }
                detail::FieldCaller& field(const std::string& k, uint64_t v)             { return f_ui(',', k, v); }
                detail::FieldCaller& field(const std::string& k, long long v)            { return f_i(',', k, v); }
                detail::FieldCaller& field(const std::string& k, double v, int prec = 2) { return f_f(',', k, v, prec); }
                detail::TSCaller& timestamp(uint64_t ts)                                 { return this->ts(ts); }
            };
        }
    } // namespace influxdb
} // namespace network

#endif // __INFLUXDB_H__