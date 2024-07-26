// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup tcp_socket TCP
 * @brief Implementation for the TCP sockets.
 * @ingroup socket
 * 
 * @file Socket.h
 * @ingroup tcp_socket
 * @file Socket.cpp
 * @ingroup tcp_socket
 */
#if !defined(__TCP_SOCKET_H__)
#define __TCP_SOCKET_H__

#include "Defines.h"
#include "common/Log.h"

#if defined(_WIN32)
#include <ws2tcpip.h>
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#endif // defined(_WIN32)

#include <cstdlib>
#include <ctime>

#if defined(_WIN32)
#ifndef _SSIZE_T_DECLARED
typedef SSIZE_T ssize_t;
#define _SSIZE_T_DECLARED
#endif
#endif // defined(_WIN32)

namespace network
{
    namespace tcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements low-level routines to communicate over a TCP
         *  network socket.
         * @ingroup tcp_socket
         */
        class HOST_SW_API Socket
        {
        public:
            auto operator=(Socket&) -> Socket& = delete;
            auto operator=(Socket&&) -> Socket& = delete;
            Socket(Socket&) = delete;

            /**
             * @brief Initializes a new instance of the Socket class.
             */
            Socket();
            /**
             * @brief Initializes a new instance of the Socket class.
             * @param fd File descriptor for existing socket.
             */
#if defined(_WIN32)
            Socket(const SOCKET fd) noexcept;
#else
            Socket(const int fd) noexcept;
#endif // defined(_WIN32)
            /**
             * @brief Initializes a new instance of the Socket class.
             * @param domain Address family type.
             * @param type Socket type (for TCP always SOCK_STREAM).
             * @param protocol Protocol.
             */
            Socket(const int domain, const int type, const int protocol);
            /**
             * @brief Finalizes a instance of the Socket class.
             */
            virtual ~Socket();

            /**
             * @brief Accepts a pending connection request.
             * @param address Socket address structure.
             * @param addrLen 
             * @returns int File descriptor for the accepted connection.
             */
            int accept(sockaddr* address, socklen_t* addrlen) noexcept;
            /**
             * @brief Connects the client to a remote TCP host using the specified host name and port number.
             * @param ipAddr IP address.
             * @param port Port.
             * @returns True, if connected to remote TCP host, otherwise false.
             */
            virtual bool connect(const std::string& ipAddr, const uint16_t port);
            /**
             * @brief Starts listening for incoming connection requests with a maximum number of pending connection.
             * @param ipAddr IP address.
             * @param port Port.
             * @param backlog 
             * @returns ssize_t Zero, if no error during listen, otherwise error.
             */
            ssize_t listen(const std::string& ipAddr, const uint16_t port, int backlog) noexcept;

            /**
             * @brief Read data from the socket.
             * @param[out] buffer Buffer to read data into.
             * @param[out] length Length of data to read.
             * @returns ssize_t Actual length of data read from remote TCP socket.
             */
            [[nodiscard]] virtual ssize_t read(uint8_t* buffer, size_t length) noexcept;
            /**
             * @brief Write data to the socket.
             * @param[in] buffer Buffer containing data to write to socket.
             * @param length Length of data to write.
             * @returns ssize_t Length of data written.
             */
            virtual ssize_t write(const uint8_t* buffer, size_t length) noexcept;

            /**
             * @brief Gets the numeric representation of an address from a sockaddr_storage socket address structure.
             * @param addr Instance of sockaddr_storage socket address structure.
             * @returns uint32_t Numeric representation of an address.
             */
            static uint32_t addr(const sockaddr_storage& addr);
            /**
             * @brief Gets the string representation of an address from a sockaddr_storage socket address structure.
             * @param addr Instance of sockaddr_storage socket address structure.
             * @returns std::string String representation of an address.
             */
            static std::string address(const sockaddr_storage& addr);
            /**
             * @brief Gets the port from a sockaddr_storage socket address structure.
             * @param addr Instance of sockaddr_storage socket address structure.
             * @returns uint16_t Port.
             */
            static uint16_t port(const sockaddr_storage& addr);

            /**
             * @brief Helper to check if the address stored in a sockaddr_storage socket address structure is INADDR_NONE.
             * @param addr Instance of sockaddr_storage socket address structure.
             * @returns bool True, if address is INADDR_NONE, otherwise false.
             */
            static bool isNone(const sockaddr_storage& addr);

        protected:
            std::string m_localAddress;
            uint16_t m_localPort;

#if defined(_WIN32)
            SOCKET m_fd;
#else
            int m_fd;
#endif // defined(_WIN32)

            uint32_t m_counter;

            /**
             * @brief Internal helper to initialize the socket.
             * @param domain Address family type.
             * @param type Socket type (for UDP always SOCK_DGRAM).
             * @param protocol Protocol.
             * @returns True, if socket initialized, otherwise false.
             */
            bool initSocket(const int domain, const int type, const int protocol);
            /**
             * @brief Internal helper to bind to a address and port.
             * @param ipAddr IP address to bind to.
             * @param port Port number to bind to.
             * @returns True, if bound, otherwise false.
             */
            bool bind(const std::string& ipAddr, const uint16_t port);

            /**
             * @brief Helper to lookup a hostname and resolve it to an IP address.
             * @param inaddr Instance of a in_addr socket structure.
             * @returns std::string String representation of an IP address.
             */
            [[nodiscard]] static std::string getIpAddress(const in_addr inaddr);

            /**
             * @brief Initialize the sockaddr_in structure with the provided IP and port.
             * @param ipAddr IP address to bind to.
             * @param port Port number to bind to.
             * @param[out] addr Instance of sockaddr_storage socket address structure.
             */
            static void initAddr(const std::string& ipAddr, const int port, sockaddr_in& addr);
        };
    } // namespace tcp
} // namespace network

#endif // __TCP_SOCKET_H__
