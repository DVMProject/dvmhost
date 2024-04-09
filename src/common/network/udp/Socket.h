// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2006-2016,2020 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
*
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

enum IPMATCHTYPE {
    IMT_ADDRESS_AND_PORT,
    IMT_ADDRESS_ONLY
};

namespace network
{
    namespace udp
    {
#if defined(HAVE_SENDMSG) && !defined(HAVE_SENDMMSG)
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
#endif

        // ---------------------------------------------------------------------------
        //  Structure Declaration
        //      This structure represents a container for a network buffer.
        // ---------------------------------------------------------------------------

        struct UDPDatagram {
            uint8_t* buffer;
            size_t length;

            sockaddr_storage address;
            uint32_t addrLen;
        };

        /* Vector of buffers that contain a full frames */
        typedef std::vector<UDPDatagram*> BufferVector;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      This class implements low-level routines to communicate over a UDP
        //      network socket.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Socket {
        public:
            auto operator=(Socket&) -> Socket& = delete;
            auto operator=(Socket&&) -> Socket& = delete;
            Socket(Socket&) = delete;

            /// <summary>Initializes a new instance of the Socket class.</summary>
            Socket(const std::string& address, uint16_t port = 0U);
            /// <summary>Initializes a new instance of the Socket class.</summary>
            Socket(uint16_t port = 0U);
            /// <summary>Finalizes a instance of the Socket class.</summary>
            virtual ~Socket();

            /// <summary>Opens UDP socket connection.</summary>
            bool open(const sockaddr_storage& address) noexcept;
            /// <summary>Opens UDP socket connection.</summary>
            bool open(uint32_t af = AF_UNSPEC) noexcept;
            /// <summary>Opens UDP socket connection.</summary>
            bool open(const uint32_t af, const std::string& address, const uint16_t port) noexcept;

            /// <summary>Closes the UDP socket connection.</summary>
            void close();

            /// <summary>Read data from the UDP socket.</summary>
            virtual ssize_t read(uint8_t* buffer, uint32_t length, sockaddr_storage& address, uint32_t& addrLen) noexcept;
            /// <summary>Write data to the UDP socket.</summary>
            virtual bool write(const uint8_t* buffer, uint32_t length, const sockaddr_storage& address, uint32_t addrLen, ssize_t* lenWritten = nullptr) noexcept;
            /// <summary>Write data to the UDP socket.</summary>
            virtual bool write(BufferVector& buffers, ssize_t* lenWritten = nullptr) noexcept;

            /// <summary>Sets the preshared encryption key.</summary>
            void setPresharedKey(const uint8_t* presharedKey);

            /// <summary>Helper to lookup a hostname and resolve it to an IP address.</summary>
            static int lookup(const std::string& hostName, uint16_t port, sockaddr_storage& address, uint32_t& addrLen);
            /// <summary>Helper to lookup a hostname and resolve it to an IP address.</summary>
            static int lookup(const std::string& hostName, uint16_t port, sockaddr_storage& address, uint32_t& addrLen, struct addrinfo& hints);

            /// <summary></summary>
            static std::string getLocalAddress();

            /// <summary></summary>
            static bool match(const sockaddr_storage& addr1, const sockaddr_storage& addr2, IPMATCHTYPE type = IMT_ADDRESS_AND_PORT);

            /// <summary></summary>
            static uint32_t addr(const sockaddr_storage& addr);
            /// <summary></summary>
            static std::string address(const sockaddr_storage& addr);
            /// <summary></summary>
            static uint16_t port(const sockaddr_storage& addr);

            /// <summary></summary>
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

            /// <summary></summary>
            bool initSocket(const int domain, const int type, const int protocol);
            /// <summary></summary>
            bool bind(const std::string& ipAddr, const uint16_t port);

            /// <summary>Initialize the sockaddr_in structure with the provided IP and port.</summary>
            static void initAddr(const std::string& ipAddr, const int port, sockaddr_in& addr);
        };
    } // namespace udp
} // namespace network

#endif // __UDP_SOCKET_H__
