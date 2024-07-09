// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2006-2016,2020 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup udp_socket UDP
 * @brief Implementation for the UDP sockets.
 * @ingroup socket
 * 
 * @file Socket.h
 * @ingroup udp_socket
 * @file Socket.cpp
 * @ingroup udp_socket
 */
#if !defined(__UDP_SOCKET_H__)
#define __UDP_SOCKET_H__

#include "common/Defines.h"
#include "common/AESCrypto.h"

#include <string>
#include <vector>

#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#if !defined(UDP_SOCKET_MAX)
#define UDP_SOCKET_MAX	1
#endif

#define AES_WRAPPED_PCKT_MAGIC 0xC0FEU
#define AES_WRAPPED_PCKT_KEY_LEN 32

/**
 * @brief IP Address Match Type
 * @ingroup udp_socket
 */
enum IPMATCHTYPE {
    IMT_ADDRESS_AND_PORT,       //! Address and Port
    IMT_ADDRESS_ONLY            //! Address Only
};

namespace network
{
    namespace udp
    {
#if defined(HAVE_SENDMSG) && !defined(HAVE_SENDMMSG)
        /** \cond */
        /* For `sendmmsg'.  */
        struct mmsghdr {
            struct msghdr msg_hdr;  /* Actual message header.  */
            unsigned int msg_len;   /* Number of received or sent bytes for the entry.  */
        };

        /* Send a VLEN messages as described by VMESSAGES to socket FD.
        Returns the number of datagrams successfully written or -1 for errors.

        This function is a cancellation point and therefore not marked with
        __THROW.  */
        static inline int sendmmsg(int sockfd, struct mmsghdr* msgvec, unsigned int vlen, int flags)
        {
            ssize_t n = 0;
            for (unsigned int i = 0; i < vlen; i++) {
                ssize_t ret = sendmsg(sockfd, &msgvec[i].msg_hdr, flags);
                if (ret < 0)
                    break;
                n += ret;
            }

            if (n == 0)
                return -1;

            return int(n);
        }
        /** \endcond */
#endif

        // ---------------------------------------------------------------------------
        //  Structure Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This structure represents a container for a network buffer.
         * @ingroup udp_socket
         */
        struct UDPDatagram {
            uint8_t* buffer;            //! Message Buffer
            size_t length;              //! Length of Message Buffer

            sockaddr_storage address;   //! Address and Port
            uint32_t addrLen;           //! Length of address structure
        };

        /** @brief Vector of buffers that contain a full frames */
        typedef std::vector<UDPDatagram*> BufferVector;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements low-level routines to communicate over a UDP
         *  network socket.
         * @ingroup udp_socket
         */
        class HOST_SW_API Socket {
        public:
            auto operator=(Socket&) -> Socket& = delete;
            auto operator=(Socket&&) -> Socket& = delete;
            Socket(Socket&) = delete;

            /**
             * @brief Initializes a new instance of the Socket class.
             * @param address Hostname/IP address to connect to.
             * @param port Port number.
             */
            Socket(const std::string& address, uint16_t port = 0U);
            /**
             * @brief Initializes a new instance of the Socket class.
             * @param port Port number.
             */
            Socket(uint16_t port = 0U);
            /**
             * @brief Finalizes a instance of the Socket class.
             */
            virtual ~Socket();

            /**
             * @brief Opens UDP socket connection.
             * @param address Instance of sockaddr_storage socket address structure.
             * @returns bool True, if UDP socket is opened, otherwise false.
             */
            bool open(const sockaddr_storage& address) noexcept;
            /**
             * @brief Opens UDP socket connection.
             * @param af Address FAmily.
             * @returns bool True, if UDP socket is opened, otherwise false.
             */
            bool open(uint32_t af = AF_UNSPEC) noexcept;
            /**
             * @brief Opens UDP socket connection.
             * @param af Address Family.
             * @param address Instance of sockaddr_storage socket address structure.
             * @param port Port.
             * @returns bool True, if UDP socket is opened, otherwise false.
             */
            bool open(const uint32_t af, const std::string& address, const uint16_t port) noexcept;

            /**
             * @brief Closes the UDP socket connection.
             */
            void close();

            /**
             * @brief Read data from the UDP socket.
             * @param[out] buffer Buffer to read data into.
             * @param[out] length Length of data to read.
             * @param[out] address IP address data read from.
             * @param[out] addrLen 
             * @returns ssize_t Actual length of data read from remote UDP socket.
             */
            virtual ssize_t read(uint8_t* buffer, uint32_t length, sockaddr_storage& address, uint32_t& addrLen) noexcept;
            /**
             * @brief Write data to the UDP socket.
             * @param[in] buffer Buffer containing data to write to socket.
             * @param length Length of data to write.
             * @param address IP address to write data to.
             * @param addrLen 
             * @param[out] lenWritten Total number of bytes written.
             * @returns bool True, if message was sent otherwise, false.
             */
            virtual bool write(const uint8_t* buffer, uint32_t length, const sockaddr_storage& address, uint32_t addrLen, ssize_t* lenWritten = nullptr) noexcept;
            /**
             * @brief Write data to the UDP socket.
             * @param[in] buffers Vector of buffers to write to socket.
             * @param[out] lenWritten Total number of bytes written.
             * @returns bool True, if messages were sent otherwise, false.
             */
            virtual bool write(BufferVector& buffers, ssize_t* lenWritten = nullptr) noexcept;

            /**
             * @brief Sets the preshared encryption key.
             * @param[in] Buffer containing the preshared encryption key.
             */
            void setPresharedKey(const uint8_t* presharedKey);

            /**
             * @brief Helper to lookup a hostname and resolve it to an IP address.
             * @param hostname String containing hostname to resolve.
             * @param port Numeric port number of service to resolve.
             * @param address Socket address structure.
             * @param addrLen 
             * @returns int Zero, if no error during lookup, otherwise error.
             */
            static int lookup(const std::string& hostName, uint16_t port, sockaddr_storage& address, uint32_t& addrLen);
            /**
             * @brief Helper to lookup a hostname and resolve it to an IP address.
             * @param hostname String containing hostname to resolve.
             * @param port Numeric port number of service to resolve.
             * @param address Socket address structure.
             * @param addrLen 
             * @param hints 
             * @returns int Zero, if no error during lookup, otherwise error.
             */
            static int lookup(const std::string& hostName, uint16_t port, sockaddr_storage& address, uint32_t& addrLen, struct addrinfo& hints);

            /**
             * @brief Helper to return the local address of the machine the socket is running on.
             * @returns std::string String representation of the local address.
             */
            static std::string getLocalAddress();

            /**
             * @brief 
             * @param addr1 Instance of sockaddr_storage socket address structure.
             * @param addr2 Instance of sockaddr_storage socket address structure.
             * @param type IPMATCHTYPE.
             * @returns bool 
             */
            static bool match(const sockaddr_storage& addr1, const sockaddr_storage& addr2, IPMATCHTYPE type = IMT_ADDRESS_AND_PORT);

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

        private:
            std::string m_localAddress;
            uint16_t m_localPort;

            uint32_t m_af;
            int m_fd;

            crypto::AES* m_aes;
            bool m_isCryptoWrapped;
            uint8_t* m_presharedKey;

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
             * @brief Initialize the sockaddr_in structure with the provided IP and port.
             * @param ipAddr IP address to bind to.
             * @param port Port number to bind to.
             * @param[out] addr Instance of sockaddr_storage socket address structure.
             */
            static void initAddr(const std::string& ipAddr, const int port, sockaddr_in& addr);
        };
    } // namespace udp
} // namespace network

#endif // __UDP_SOCKET_H__
