// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2006-2016,2020 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "network/udp/Socket.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::udp;

#include <cassert>
#include <cerrno>
#include <cstring>

#if !defined(_WIN32)
#include <ifaddrs.h>
#endif // !defined(_WIN32)

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_BUFFER_COUNT 16384

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the Socket class. */

Socket::Socket(const std::string& address, uint16_t port) :
    m_localAddress(address),
    m_localPort(port),
    m_af(AF_UNSPEC),
#if defined(_WIN32)
    m_fd(INVALID_SOCKET),
#else
    m_fd(-1),
#endif // defined(_WIN32)
    m_aes(nullptr),
    m_isCryptoWrapped(false),
    m_presharedKey(nullptr),
    m_counter(0U)
{
    m_aes = new crypto::AES(crypto::AESKeyLength::AES_256);
    m_presharedKey = new uint8_t[AES_WRAPPED_PCKT_KEY_LEN];

#if defined(_WIN32)
    WSAData data;
    int wsaRet = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (wsaRet != 0) {
        ::LogError(LOG_NET, "Error from WSAStartup, err: %d", wsaRet);
    }
#endif // defined(_WIN32)
}

/* Initializes a new instance of the Socket class. */

Socket::Socket(uint16_t port) :
    m_localAddress(),
    m_localPort(port),
    m_af(AF_UNSPEC),
#if defined(_WIN32)
    m_fd(INVALID_SOCKET),
#else
    m_fd(-1),
#endif // defined(_WIN32)
    m_aes(nullptr),
    m_isCryptoWrapped(false),
    m_presharedKey(nullptr),
    m_counter(0U)
{
    m_aes = new crypto::AES(crypto::AESKeyLength::AES_256);
    m_presharedKey = new uint8_t[AES_WRAPPED_PCKT_KEY_LEN];

#if defined(_WIN32)
    WSAData data;
    int wsaRet = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (wsaRet != 0) {
        ::LogError(LOG_NET, "Error from WSAStartup, err: %d", wsaRet);
    }
#endif // defined(_WIN32)
}

/* Finalizes a instance of the Socket class. */

Socket::~Socket()
{
    if (m_aes != nullptr)
        delete m_aes;
    if (m_presharedKey != nullptr)
        delete[] m_presharedKey;

#if defined(_WIN32)
    ::WSACleanup();
#endif // defined(_WIN32)
}

/* Opens UDP socket connection. */

bool Socket::open(const sockaddr_storage& address) noexcept
{
    return open(address.ss_family);
}

/* Opens UDP socket connection. */

bool Socket::open(uint32_t af) noexcept
{
    return open(af, m_localAddress, m_localPort);
}

/* Opens UDP socket connection. */

bool Socket::open(const uint32_t af, const std::string& address, const uint16_t port) noexcept
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
        return false;
    }

    close();

    if (!initSocket(addr.ss_family, SOCK_DGRAM, 0))
        return false;

    if (port > 0U) {
        int reuse = 1;
        if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, (char*)& reuse, sizeof(reuse)) == -1) {
#if defined(_WIN32)
            LogError(LOG_NET, "Cannot bind the UDP socket option, err: %lu", ::GetLastError());
#else
            LogError(LOG_NET, "Cannot set the UDP socket option, err: %d (%s)", errno, strerror(errno));
#endif // _WIN32
            return false;
        }

        if (!bind(address, port)) {
            return false;
        }
    }

    return true;
}

/* Sets the socket receive buffer size. */

bool Socket::recvBufSize(ssize_t bufSize)
{
    int optVal = -1;
    socklen_t optValLen = sizeof(optVal);

    // resize buffer
    if (::setsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (char*)& bufSize, sizeof(bufSize)) == -1) {
#if defined(_WIN32)
        LogError(LOG_NET, "Cannot resize the receive buffer size, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Cannot resize the receive buffer size, err: %d (%s)", errno, strerror(errno));
#endif // _WIN32
        return false;
    }

    // get the buffer size after alteration
    if (::getsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, (char*)&optVal, &optValLen) == -1) {
#if defined(_WIN32)
        LogError(LOG_NET, "Cannot get the receive buffer size, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Cannot get the receive buffer size, err: %d (%s)", errno, strerror(errno));
#endif // _WIN32
        return false;
    }

    /*
    ** bryanb: this check may seem strange, but on Linux the kernel doubles the
    **   requested buffer size for its own overhead, so we just need to ensure
    **   that the returned buffer size is at least what we requested
    */
    if (optVal >= bufSize)
        return true;
    else
        LogWarning(LOG_NET, "Could not resize socket recv buffer, %u != %u. This is suboptimal and may result in lost packets.", optVal, bufSize);

    return false;
}

/* Sets the socket send buffer size. */

bool Socket::sendBufSize(ssize_t bufSize)
{
    int optVal = -1;
    socklen_t optValLen = sizeof(optVal);

    // resize buffer
    if (::setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, sizeof(bufSize)) == -1) {
#if defined(_WIN32)
        LogError(LOG_NET, "Cannot resize the send buffer size, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Cannot resize the send buffer size, err: %d (%s)", errno, strerror(errno));
#endif // _WIN32
        return false;
    }

    // get the buffer size after alteration
    if (::getsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, (char*)&optVal, &optValLen) == -1) {
#if defined(_WIN32)
        LogError(LOG_NET, "Cannot get the send buffer size, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Cannot get the send buffer size, err: %d (%s)", errno, strerror(errno));
#endif // _WIN32
        return false;
    }

    /*
    ** bryanb: this check may seem strange, but on Linux the kernel doubles the
    **   requested buffer size for its own overhead, so we just need to ensure
    **   that the returned buffer size is at least what we requested
    */
    if (optVal >= bufSize)
        return true;
    else
        LogWarning(LOG_NET, "Could not resize socket send buffer, %u != %u. This is suboptimal and may result in lost packets.", optVal, bufSize);

    return false;
}

/* Closes the UDP socket connection. */

void Socket::close()
{
#if defined(_WIN32)
    if (m_fd != INVALID_SOCKET) {
        ::closesocket(m_fd);
        m_fd = INVALID_SOCKET;
    }
#else
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif // defined(_WIN32)
}

/* Read data from the UDP socket. */

ssize_t Socket::read(uint8_t* buffer, uint32_t length, sockaddr_storage& address, uint32_t& addrLen) noexcept
{
    assert(buffer != nullptr);
    assert(length > 0U);

#if defined(_WIN32)
    if (m_fd == INVALID_SOCKET)
        return -1;
#else
    if (m_fd < 0)
        return -1;
#endif // defined(_WIN32)

    // check that the readfrom() won't block
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
        LogError(LOG_NET, "Error returned from UDP poll, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from UDP poll, err: %d (%s)", errno, strerror(errno));
#endif // defined(_WIN32)
        return -1;
    }

    if ((pfd.revents & POLLIN) == 0)
        return 0;

    socklen_t size = sizeof(sockaddr_storage);
    ssize_t len = ::recvfrom(pfd.fd, (char*)buffer, length, 0, (sockaddr*)& address, &size);
    if (len <= 0) {
#if defined(_WIN32)
        LogError(LOG_NET, "Error returned from recvfrom, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from recvfrom, err: %d (%s)", errno, strerror(errno));
#endif // defined(_WIN32)

        if (len == -1 && errno == ENOTSOCK) {
            LogInfoEx(LOG_NET, "Re-opening UDP port on %u", m_localPort);
            close();
            open();
        }

        return -1;
    }

    // are we crypto wrapped?
    if (m_isCryptoWrapped) {
        if (m_presharedKey == nullptr) {
            LogError(LOG_NET, "tried to read datagram encrypted with no key? this shouldn't happen BUGBUG");
            return -1;
        }

        // does the network packet contain the appropriate magic leader?
        uint16_t magic = GET_UINT16(buffer, 0U);
        if (magic == AES_WRAPPED_PCKT_MAGIC) {
            // prevent malicious packets that are too short
            if (len < 2U + crypto::AES::BLOCK_BYTES_LEN) {
                LogError(LOG_NET, "Encrypted packet too short");
                return -1;
            }

            uint32_t cryptedLen = (len - 2U) * sizeof(uint8_t);
            uint8_t* cryptoBuffer = buffer + 2U;

            // do we need to pad the original buffer to be block aligned?
            if (cryptedLen % crypto::AES::BLOCK_BYTES_LEN != 0) {
                uint32_t alignment = crypto::AES::BLOCK_BYTES_LEN - (cryptedLen % crypto::AES::BLOCK_BYTES_LEN);
                cryptedLen += alignment;

                // reallocate buffer and copy
                cryptoBuffer = new uint8_t[cryptedLen];
                ::memset(cryptoBuffer, 0x00U, cryptedLen);
                ::memcpy(cryptoBuffer, buffer + 2U, len - 2U);
            }

            // Utils::dump(1U, "Socket::read(), crypted", cryptoBuffer, cryptedLen);

            // decrypt
            uint8_t* decrypted = m_aes->decryptECB(cryptoBuffer, cryptedLen, m_presharedKey);

            // Utils::dump(1U, "Socket::read(), decrypted", decrypted, cryptedLen);

            // finalize, cleanup buffers and replace with new
            if (decrypted != nullptr) {
                ::memset(buffer, 0x00U, len);
                ::memcpy(buffer, decrypted, len - 2U);

                delete[] decrypted;
                len -= 2U;
            } else {
                delete[] decrypted;
                return 0;
            }
        }
        else {
            return 0; // this will effectively discard packets without the packet magic
        }
    }

    m_counter++;
    addrLen = size;
    return len;
}

/* Write data to the UDP socket. */

bool Socket::write(const uint8_t* buffer, uint32_t length, const sockaddr_storage& address, uint32_t addrLen, ssize_t* lenWritten) noexcept
{
    assert(buffer != nullptr);
    assert(length > 0U);

#if defined(_WIN32)
    if (m_fd == INVALID_SOCKET) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        //LogError(LOG_NET, "tried to write datagram with no file descriptor? this shouldn't happen BUGBUG");
        return false;
    }
#else
    if (m_fd < 0) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        //LogError(LOG_NET, "tried to write datagram with no file descriptor? this shouldn't happen BUGBUG");
        return false;
    }
#endif // defined(_WIN32)

    bool result = false;
    UInt8Array out = nullptr;

    // are we crypto wrapped?
    if (m_isCryptoWrapped) {
        if (m_presharedKey == nullptr) {
            LogError(LOG_NET, "tried to write datagram encrypted with no key? this shouldn't happen BUGBUG");
            return false;
        }

        uint32_t cryptedLen = length * sizeof(uint8_t);
        uint8_t* cryptoBuffer = new uint8_t[length];
        ::memcpy(cryptoBuffer, buffer, length);

        // do we need to pad the original buffer to be block aligned?
        if (cryptedLen % crypto::AES::BLOCK_BYTES_LEN != 0) {
            uint32_t alignment = crypto::AES::BLOCK_BYTES_LEN - (cryptedLen % crypto::AES::BLOCK_BYTES_LEN);
            cryptedLen += alignment;

            // reallocate buffer and copy
            delete[] cryptoBuffer;
            cryptoBuffer = new uint8_t[cryptedLen];
            ::memset(cryptoBuffer, 0x00U, cryptedLen);
            ::memcpy(cryptoBuffer, buffer, length);
        }

        // encrypt
        uint8_t* crypted = m_aes->encryptECB(cryptoBuffer, cryptedLen, m_presharedKey);

        // Utils::dump(1U, "Socket::write(), crypted", crypted, cryptedLen);

        // finalize, cleanup buffers and replace with new
        out = std::unique_ptr<uint8_t[]>(new uint8_t[cryptedLen + 2U]);
        delete[] cryptoBuffer;
        if (crypted != nullptr) {
            ::memcpy(out.get() + 2U, crypted, cryptedLen);
            SET_UINT16(AES_WRAPPED_PCKT_MAGIC, out.get(), 0U);
            delete[] crypted;
            length = cryptedLen + 2U;
        } else {
            if (lenWritten != nullptr) {
                *lenWritten = -1;
            }

            delete[] crypted;
            return false;
        }
    } else {
        out = std::unique_ptr<uint8_t[]>(new uint8_t[length]);
        ::memcpy(out.get(), buffer, length);
    }

    ssize_t sent = ::sendto(m_fd, (char*)out.get(), length, 0, (sockaddr*)& address, addrLen);
    if (sent < 0) {
        if (errno == ENETUNREACH || errno == EHOSTUNREACH) {
            // if we were not able to send a frame and the network logging is enabled -- disable network logging
            if (!g_disableNetworkLog)
                g_disableNetworkLog = true;
        }

#if defined(_WIN32)
        LogError(LOG_NET, "Error returned from sendto, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from sendto, err: %d (%s)", errno, strerror(errno));
#endif // defined(_WIN32)

        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }
    }
    else {
        // if we were able to send a frame and the network logging is disabled -- reenable network logging
        if (g_disableNetworkLog)
            g_disableNetworkLog = false;

        if (sent == ssize_t(length))
            result = true;

        if (lenWritten != nullptr) {
            *lenWritten = sent;
        }
    }

    return result;
}

/* Write data to the UDP socket. */

bool Socket::write(BufferQueue* buffers, ssize_t* lenWritten) noexcept
{
    if (buffers == nullptr) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        LogError(LOG_NET, "tried to write datagram with buffers? this shouldn't happen BUGBUG");
        return false;
    }

    size_t currentQueueSize = buffers->size();

    bool result = false;
    if (m_fd < 0) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        LogError(LOG_NET, "tried to write datagram with no file descriptor? this shouldn't happen BUGBUG");
        return false;
    }

    if (buffers->empty()) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        return false;
    }

    // bryanb: this is the same as above -- but for some assinine reason prevents
    // weirdness
    if (currentQueueSize == 0U) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        return false;
    }

    // LogDebugEx(LOG_NET, "Socket::write()", "buffers len = %u", currentQueueSize);
    if (currentQueueSize > UINT16_MAX)
        currentQueueSize = UINT16_MAX; // only send up to this many buffers

    // are we crypto wrapped?
    if (m_isCryptoWrapped) {
        if (m_presharedKey == nullptr) {
            LogError(LOG_NET, "tried to write datagram encrypted with no key? this shouldn't happen BUGBUG");

            if (lenWritten != nullptr) {
                *lenWritten = -1;
            }

            return false;
        }
    }

    int sent = 0, msgs = 0;
    struct sockaddr_storage* addresses[MAX_BUFFER_COUNT];
    struct mmsghdr headers[MAX_BUFFER_COUNT];
    struct iovec chunks[MAX_BUFFER_COUNT];

    // create mmsghdrs from input buffers and send them at once
    for (size_t i = 0U; i < currentQueueSize; ++i) {
        UDPDatagram* packet = buffers->front();
        buffers->pop();
        if (packet == nullptr) {
            continue;
        }

        uint32_t length = packet->length;
        if (packet->buffer == nullptr) {
            LogError(LOG_NET, "discarding buffered message with len = %u, but deleted buffer?", length);
            delete packet;
            continue;
        }

        if (m_af != packet->address.ss_family) {
            LogError(LOG_NET, "Socket::write() mismatched network address family? this isn't normal, aborting");
            if (packet->buffer != nullptr)
                delete[] packet->buffer;
            delete packet;
            continue;
        }

        uint8_t* iov_buffer = new uint8_t[packet->length];
        size_t iov_length = packet->length;
        sockaddr_storage address;
        ::memcpy(&address, &packet->address, sizeof(sockaddr_storage));
        uint32_t addrLen = packet->addrLen;

        ::memcpy(iov_buffer, packet->buffer, packet->length);

        // cleanup buffered packet
        if (packet != nullptr) {
            if (packet->buffer != nullptr) {
                delete[] packet->buffer;
                packet->length = 0;
            }

            delete packet;
        }

        // are we crypto wrapped?
        if (m_isCryptoWrapped && m_presharedKey != nullptr) {
            uint32_t cryptedLen = length * sizeof(uint8_t);
            uint8_t* cryptoBuffer = new uint8_t[iov_length];
            ::memcpy(cryptoBuffer, iov_buffer, iov_length);

            // do we need to pad the original buffer to be block aligned?
            if (cryptedLen % crypto::AES::BLOCK_BYTES_LEN != 0) {
                uint32_t alignment = crypto::AES::BLOCK_BYTES_LEN - (cryptedLen % crypto::AES::BLOCK_BYTES_LEN);
                cryptedLen += alignment;

                // reallocate buffer and copy
                cryptoBuffer = new uint8_t[cryptedLen];
                ::memset(cryptoBuffer, 0x00U, cryptedLen);
                ::memcpy(cryptoBuffer, iov_buffer, length);
            }

            // encrypt
            uint8_t* crypted = m_aes->encryptECB(cryptoBuffer, cryptedLen, m_presharedKey);
            delete[] cryptoBuffer;

            if (crypted == nullptr) {
                if (iov_buffer != nullptr) {
                    delete[] iov_buffer;
                    iov_buffer = nullptr;
                }
                continue;
            }

            // Utils::dump(1U, "Socket::write(), crypted", crypted, cryptedLen);

            // finalize
            DECLARE_UINT8_ARRAY(out, cryptedLen + 2U);
            ::memcpy(out + 2U, crypted, cryptedLen);
            SET_UINT16(AES_WRAPPED_PCKT_MAGIC, out, 0U);

            // cleanup buffers and replace with new
            delete[] crypted;
            crypted = nullptr;
            delete[] iov_buffer;
            iov_buffer = nullptr;
            iov_buffer = new uint8_t[cryptedLen + 2U];
            ::memcpy(iov_buffer, out, cryptedLen + 2U);
            iov_length = cryptedLen + 2U;
        }

        // skip if no IOV buffer
        if (iov_buffer == nullptr) {
            continue;
        }

        addresses[i] = new sockaddr_storage;
        ::memcpy(addresses[i], &address, sizeof(sockaddr_storage));

        chunks[i].iov_len = iov_length;
        chunks[i].iov_base = iov_buffer;
        sent += iov_length;

        headers[i].msg_hdr.msg_name = (void*)addresses[i];
        headers[i].msg_hdr.msg_namelen = addrLen;
        headers[i].msg_hdr.msg_iov = &chunks[i];
        headers[i].msg_hdr.msg_iovlen = 1;
        headers[i].msg_hdr.msg_control = 0;
        headers[i].msg_hdr.msg_controllen = 0;

        ++msgs;
    }

    if (sendmmsg(m_fd, headers, msgs, 0) < 0) {
#if defined(_WIN32)
        LogError(LOG_NET, "Error returned from sendmmsg, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from sendmmsg, err: %d (%s)", errno, strerror(errno));
#endif // _WIN32
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }
    }

    if (sent < 0) {
#if defined(_WIN32)
        LogError(LOG_NET, "Error returned from sendmmsg, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Error returned from sendmmsg, err: %d (%s)", errno, strerror(errno));
#endif // _WIN32
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

    // cleanup buffers
    for (size_t i = 0U; i < currentQueueSize; i++) {
        if (addresses[i] != nullptr) {
            delete addresses[i];
            addresses[i] = nullptr;
        }

        if (chunks[i].iov_base != nullptr) {
            uint8_t* iov_buffer = (uint8_t*)chunks[i].iov_base;
            delete[] iov_buffer;
            chunks[i].iov_base = nullptr;
        }
    }

    return result;
}

/* Sets the preshared encryption key. */

void Socket::setPresharedKey(const uint8_t* presharedKey)
{
    if (presharedKey != nullptr) {
        ::memset(m_presharedKey, 0x00U, AES_WRAPPED_PCKT_KEY_LEN);
        ::memcpy(m_presharedKey, presharedKey, AES_WRAPPED_PCKT_KEY_LEN);
        m_isCryptoWrapped = true;
    } else {
        ::memset(m_presharedKey, 0x00U, AES_WRAPPED_PCKT_KEY_LEN);
        m_isCryptoWrapped = false;
    }
}

/* Helper to lookup a hostname and resolve it to an IP address. */

int Socket::lookup(const std::string& hostname, uint16_t port, sockaddr_storage& address, uint32_t& addrLen)
{
    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));

    return lookup(hostname, port, address, addrLen, hints);
}

/* Helper to lookup a hostname and resolve it to an IP address. */

int Socket::lookup(const std::string& hostname, uint16_t port, sockaddr_storage& address, uint32_t& addrLen, struct addrinfo& hints)
{
    std::string portstr = std::to_string(port);
    struct addrinfo* res;

    // port is always digits, no needs to lookup service
    hints.ai_flags |= AI_NUMERICSERV;

    int err = getaddrinfo(hostname.empty() ? NULL : hostname.c_str(), portstr.c_str(), &hints, &res);
    if (err != 0) {
        sockaddr_in* paddr = (sockaddr_in*)& address;
        ::memset(paddr, 0x00U, addrLen = sizeof(sockaddr_in));
        paddr->sin_family = AF_INET;
        paddr->sin_port = htons(port);
        paddr->sin_addr.s_addr = htonl(INADDR_NONE);
        LogError(LOG_NET, "Cannot find address for host %s", hostname.c_str());
        return err;
    }

    ::memcpy(&address, res->ai_addr, addrLen = res->ai_addrlen);

    freeaddrinfo(res);

    return 0;
}

/* Helper to return the local address of the machine the socket is running on. */

std::string Socket::getLocalAddress()
{
#if defined(_WIN32)
    return std::string(); // this breaks shit...
#else
    struct ifaddrs *ifaddr, *ifa;
    int n;
    char host[NI_MAXHOST];

    std::string address = std::string();

    int err = -1;
    if ((err = getifaddrs(&ifaddr)) == -1) {
        LogError(LOG_NET, "Cannot retreive system network interfaces, err: %d", err);
        return "0.0.0.0";
    }

    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == NULL)
            continue;

        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            err = getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (err != 0) {
                LogError(LOG_NET, "Cannot retreive system network interfaces, err: %d (%s)", errno, strerror(errno));
                break;
            }

            address = std::string(host);
            if (address == "127.0.0.1" || address == "::1")
                continue;
            else
                break;
        }
    }

    freeifaddrs(ifaddr);
    return address;
#endif // defined(_WIN32)
}

/* */

bool Socket::match(const sockaddr_storage& addr1, const sockaddr_storage& addr2, IPMATCHTYPE type)
{
    if (addr1.ss_family != addr2.ss_family)
        return false;

    if (type == IMT_ADDRESS_AND_PORT) {
        switch (addr1.ss_family) {
        case AF_INET:
            struct sockaddr_in* in_1, *in_2;
            in_1 = (struct sockaddr_in*)& addr1;
            in_2 = (struct sockaddr_in*)& addr2;
            return (in_1->sin_addr.s_addr == in_2->sin_addr.s_addr) && (in_1->sin_port == in_2->sin_port);
        case AF_INET6:
            struct sockaddr_in6* in6_1, *in6_2;
            in6_1 = (struct sockaddr_in6*)& addr1;
            in6_2 = (struct sockaddr_in6*)& addr2;
            return IN6_ARE_ADDR_EQUAL(&in6_1->sin6_addr, &in6_2->sin6_addr) && (in6_1->sin6_port == in6_2->sin6_port);
        default:
            return false;
        }
    }
    else if (type == IMT_ADDRESS_ONLY) {
        switch (addr1.ss_family) {
        case AF_INET:
            struct sockaddr_in* in_1, *in_2;
            in_1 = (struct sockaddr_in*)& addr1;
            in_2 = (struct sockaddr_in*)& addr2;
            return in_1->sin_addr.s_addr == in_2->sin_addr.s_addr;
        case AF_INET6:
            struct sockaddr_in6* in6_1, *in6_2;
            in6_1 = (struct sockaddr_in6*)& addr1;
            in6_2 = (struct sockaddr_in6*)& addr2;
            return IN6_ARE_ADDR_EQUAL(&in6_1->sin6_addr, &in6_2->sin6_addr);
        default:
            return false;
        }
    }
    else {
        return false;
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

bool Socket::initSocket(const int domain, const int type, const int protocol) noexcept(false)
{
    m_fd = ::socket(domain, type, protocol);
#if defined(_WIN32)
    if (m_fd == INVALID_SOCKET) {
        LogError(LOG_NET, "Cannot create the UDP socket, err: %lu", ::GetLastError());
        return false;
    }
#else
    if (m_fd < 0) {
        LogError(LOG_NET, "Cannot create the UDP socket, err: %d (%s)", errno, strerror(errno));
        return false;
    }
#endif // defined(_WIN32)

    m_af = domain;
    return true;
}

/* Internal helper to bind to a address and port. */

bool Socket::bind(const std::string& ipAddr, const uint16_t port) noexcept(false)
{
    m_localAddress = std::string(ipAddr);
    m_localPort = port;

    sockaddr_in addr = {};
    initAddr(ipAddr, port, addr);

    socklen_t length = sizeof(addr);
    bool retval = true;
    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), length) < 0) {
#if defined(_WIN32)
        LogError(LOG_NET, "Cannot bind the UDP address, err: %lu", ::GetLastError());
#else
        LogError(LOG_NET, "Cannot bind the UDP address, err: %d (%s)", errno, strerror(errno));
#endif // defined(_WIN32)
        retval = false;
    }

    return retval;
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