// SPDX-License-Identifier: MIT-only
/*
 * Digital Voice Modem - Converged FNE Software
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2010-2018 <http://ez8.co> <orca.zhang@yahoo.com>
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
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
#include "common/ThreadPool.h"

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

        #define MAX_INFLUXQL_THREAD_CNT 16U
        #define MAX_INFLUXQL_QUEUED_CNT 256U

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
                 * @param si Server Information.
                 * @returns int 
                 */
                static int request(const char* method, const char* uri, const std::string& queryString, const std::string& body, 
                    const ServerInfo& si);
            
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
                int request(const ServerInfo& si)  { return detail::inner::request("POST", "write", "", m_lines.str(), si); }
                int requestAsync(const ServerInfo& si) 
                {
                    TSCallerRequest* req = new TSCallerRequest();
                    req->obj = this;

                    req->si = ServerInfo(si.host(), si.port(), si.org(), si.token(), si.bucket());
                    req->lines = std::string(m_lines.str());

                    // enqueue the task
                    if (!m_fluxReqThreadPool.enqueue(new_pooltask(taskFluxRequest, req))) {
                        LogError(LOG_NET, "Failed to task enqueue Influx query request");
                        if (req != nullptr)
                            delete req;
                        return 1;
                    }
            
                    return 0; 
                }

                static void start() 
                { 
                    m_fluxReqThreadPool.setMaxQueuedTasks(MAX_INFLUXQL_QUEUED_CNT);
                    m_fluxReqThreadPool.start(); 
                }
                static void stop() { m_fluxReqThreadPool.stop(); }
                static void wait() { m_fluxReqThreadPool.wait(); }

            private:
                static ThreadPool m_fluxReqThreadPool;

                /**
                 * @brief 
                 */
                static void taskFluxRequest(TSCallerRequest* req)
                {
                    if (req == nullptr) {
                        return;
                    }

                    TSCaller* caller = static_cast<TSCaller*>(req->obj);
                    if (caller == nullptr) {
                        if (req != nullptr) {
                            delete req;
                        }

                        return;
                    }

                    const ServerInfo& si = req->si;
                    detail::inner::request("POST", "write", "", req->lines, si);

                    delete req;
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