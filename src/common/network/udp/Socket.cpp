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
#include "Defines.h"
#include "network/udp/Socket.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::udp;

#include <cassert>
#include <cerrno>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define MAX_BUFFER_COUNT 16384

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Socket class.
/// </summary>
/// <param name="address">Hostname/IP address to connect to.</param>
/// <param name="port">Port number.</param>
Socket::Socket(const std::string& address, uint16_t port) :
    m_localAddress(address),
    m_localPort(port),
    m_af(AF_UNSPEC),
    m_fd(-1),
    m_aes(nullptr),
    m_isCryptoWrapped(false),
    m_presharedKey(nullptr),
    m_counter(0U)
{
    m_aes = new crypto::AES(crypto::AESKeyLength::AES_256);
    m_presharedKey = new uint8_t[AES_WRAPPED_PCKT_KEY_LEN];
}

/// <summary>
/// Initializes a new instance of the Socket class.
/// </summary>
/// <param name="port">Port number.</param>
Socket::Socket(uint16_t port) :
    m_localAddress(),
    m_localPort(port),
    m_af(AF_UNSPEC),
    m_fd(-1),
    m_aes(nullptr),
    m_isCryptoWrapped(false),
    m_presharedKey(nullptr),
    m_counter(0U)
{
    m_aes = new crypto::AES(crypto::AESKeyLength::AES_256);
    m_presharedKey = new uint8_t[AES_WRAPPED_PCKT_KEY_LEN];
}

/// <summary>
/// Finalizes a instance of the Socket class.
/// </summary>
Socket::~Socket()
{
    if (m_aes != nullptr)
        delete m_aes;
    if (m_presharedKey != nullptr)
        delete[] m_presharedKey;
}

/// <summary>
/// Opens UDP socket connection.
/// </summary>
/// <param name="address"></param>
/// <returns>True, if UDP socket is opened, otherwise false.</returns>
bool Socket::open(const sockaddr_storage& address) noexcept
{
    return open(address.ss_family);
}

/// <summary>
/// Opens UDP socket connection.
/// </summary>
/// <param name="af"></param>
/// <returns>True, if UDP socket is opened, otherwise false.</returns>
bool Socket::open(uint32_t af) noexcept
{
    return open(af, m_localAddress, m_localPort);
}

/// <summary>
/// Opens UDP socket connection.
/// </summary>
/// <param name="af"></param>
/// <param name="address"></param>
/// <param name="port"></param>
/// <returns>True, if UDP socket is opened, otherwise false.</returns>
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
            LogError(LOG_NET, "Cannot set the UDP socket option, err: %d", errno);
            return false;
        }

        if (!bind(address, port)) {
            return false;
        }

        LogInfoEx(LOG_NET, "Opening UDP port on %u", port);
    }

    return true;
}

/// <summary>
/// Closes the UDP socket connection.
/// </summary>
void Socket::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

/// <summary>
/// Read data from the UDP socket.
/// </summary>
/// <param name="buffer">Buffer to read data into.</param>
/// <param name="length">Length of data to read.</param>
/// <param name="address">IP address data read from.</param>
/// <param name="addrLen"></param>
/// <returns>Actual length of data read from remote UDP socket.</returns>
ssize_t Socket::read(uint8_t* buffer, uint32_t length, sockaddr_storage& address, uint32_t& addrLen) noexcept
{
    assert(buffer != nullptr);
    assert(length > 0U);

    if (m_fd < 0)
        return -1;

    // Check that the readfrom() won't block
    int i, n;
    struct pollfd pfd[UDP_SOCKET_MAX];
    for (i = n = 0; i < UDP_SOCKET_MAX; i++) {
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
        LogError(LOG_NET, "Error returned from UDP poll, err: %d", errno);
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

    socklen_t size = sizeof(sockaddr_storage);
    ssize_t len = ::recvfrom(pfd[index].fd, (char*)buffer, length, 0, (sockaddr*)& address, &size);
    if (len <= 0) {
        LogError(LOG_NET, "Error returned from recvfrom, err: %d", errno);

        if (len == -1 && errno == ENOTSOCK) {
            LogMessage(LOG_NET, "Re-opening UDP port on %u", m_localPort);
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
        uint16_t magic = __GET_UINT16B(buffer, 0U);
        if (magic == AES_WRAPPED_PCKT_MAGIC) {
            uint32_t cryptedLen = (len - 2U) * sizeof(uint8_t);
            // Utils::dump(1U, "Socket::read() crypted", buffer + 2U, cryptedLen);

            // decrypt
            uint8_t* decrypted = m_aes->decryptECB(buffer + 2U, cryptedLen, m_presharedKey);

            // Utils::dump(1U, "Socket::read() decrypted", decrypted, cryptedLen);

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

/// <summary>
/// Write data to the UDP socket.
/// </summary>
/// <param name="buffer">Buffer containing data to write to socket.</param>
/// <param name="length">Length of data to write.</param>
/// <param name="address">IP address to write data to.</param>
/// <param name="addrLen"></param>
/// <param name="lenWritten">Total number of bytes written.</param>
/// <returns>True if message was sent, otherwise false.</returns>
bool Socket::write(const uint8_t* buffer, uint32_t length, const sockaddr_storage& address, uint32_t addrLen, ssize_t* lenWritten) noexcept
{
    assert(buffer != nullptr);
    assert(length > 0U);

    if (m_fd < 0) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        return false;
    }

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
            cryptoBuffer = new uint8_t[cryptedLen];
            ::memset(cryptoBuffer, 0x00U, cryptedLen);
            ::memcpy(cryptoBuffer, buffer, length);
        }

        // encrypt
        uint8_t* crypted = m_aes->encryptECB(cryptoBuffer, cryptedLen, m_presharedKey);

        // Utils::dump(1U, "Socket::write() crypted", crypted, cryptedLen);

        // finalize, cleanup buffers and replace with new
        out = std::unique_ptr<uint8_t[]>(new uint8_t[cryptedLen + 2U]);
        delete[] cryptoBuffer;
        if (crypted != nullptr) {
            ::memcpy(out.get() + 2U, crypted, cryptedLen);
            __SET_UINT16B(AES_WRAPPED_PCKT_MAGIC, out.get(), 0U);
            delete[] crypted;
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
        LogError(LOG_NET, "Error returned from sendto, err: %d", errno);

        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }
    }
    else {
        if (sent == ssize_t(length))
            result = true;

        if (lenWritten != nullptr) {
            *lenWritten = sent;
        }
    }

    return result;
}

/// <summary>
/// Write data to the UDP socket.
/// </summary>
/// <param name="buffers">Vector of buffers to write to socket.</param>
/// <param name="address">IP address to write data to.</param>
/// <param name="addrLen"></param>
/// <param name="lenWritten">Total number of bytes written.</param>
/// <returns>True if messages were sent, otherwise false.</returns>
bool Socket::write(BufferVector& buffers, ssize_t* lenWritten) noexcept
{
    bool result = false;
    if (m_fd < 0) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        return false;
    }

    if (buffers.empty()) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        return false;
    }

    // bryanb: this is the same as above -- but for some assinine reason prevents
    // weirdness
    if (buffers.size() == 0U) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        return false;
    }

    // LogDebug(LOG_NET, "buffers len = %u", buffers.size());

    if (buffers.size() > UINT16_MAX) {
        LogError(LOG_NET, "Trying to send too many buffers?");

        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        return false;
    }

    // LogDebug(LOG_NET, "Sending message(s) (to %s:%u) addrLen %u", Socket::address(address).c_str(), Socket::port(address), addrLen);

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

    int sent = 0;
    struct mmsghdr headers[MAX_BUFFER_COUNT];
    struct iovec chunks[MAX_BUFFER_COUNT];

    // create mmsghdrs from input buffers and send them at once
    int size = buffers.size();
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i] == nullptr) {
            --size;
            continue;
        }

        uint32_t length = buffers[i]->length;
        if (buffers[i]->buffer == nullptr) {
            LogError(LOG_NET, "discarding buffered message with len = %u, but deleted buffer?", length);
            --size;
            continue;
        }

        // are we crypto wrapped?
        if (m_isCryptoWrapped && m_presharedKey != nullptr) {
            uint32_t cryptedLen = length * sizeof(uint8_t);
            uint8_t* cryptoBuffer = buffers[i]->buffer;

            // do we need to pad the original buffer to be block aligned?
            if (cryptedLen % crypto::AES::BLOCK_BYTES_LEN != 0) {
                uint32_t alignment = crypto::AES::BLOCK_BYTES_LEN - (cryptedLen % crypto::AES::BLOCK_BYTES_LEN);
                cryptedLen += alignment;

                // reallocate buffer and copy
                cryptoBuffer = new uint8_t[cryptedLen];
                ::memset(cryptoBuffer, 0x00U, cryptedLen);
                ::memcpy(cryptoBuffer, buffers.at(i)->buffer, length);
            }

            // encrypt
            uint8_t* crypted = m_aes->encryptECB(cryptoBuffer, cryptedLen, m_presharedKey);
            delete[] cryptoBuffer;

            if (crypted == nullptr) {
                --size;
                continue;
            }

            // Utils::dump(1U, "Socket::write() crypted", crypted, cryptedLen);

            // finalize
            uint8_t out[cryptedLen + 2U];
            ::memcpy(out + 2U, crypted, cryptedLen);
            __SET_UINT16B(AES_WRAPPED_PCKT_MAGIC, out, 0U);

            // cleanup buffers and replace with new
            delete[] crypted;
            //delete buffers[i]->buffer;

            // this should never happen...
            if (buffers[i] == nullptr) {
                --size;
                continue;
            }

            buffers[i]->buffer = new uint8_t[cryptedLen + 2U];
            ::memcpy(buffers[i]->buffer, out, cryptedLen + 2U);
            buffers[i]->length = cryptedLen + 2U;
        }

        chunks[i].iov_len = buffers.at(i)->length;
        chunks[i].iov_base = buffers.at(i)->buffer;
        sent += buffers.at(i)->length;

        headers[i].msg_hdr.msg_name = (void*)&buffers.at(i)->address;
        headers[i].msg_hdr.msg_namelen = buffers.at(i)->addrLen;
        headers[i].msg_hdr.msg_iov = &chunks[i];
        headers[i].msg_hdr.msg_iovlen = 1;
        headers[i].msg_hdr.msg_control = 0;
        headers[i].msg_hdr.msg_controllen = 0;
    }

    bool skip = false;
    for (auto& buffer : buffers) {
        if (m_af != buffer->address.ss_family) {
            skip = true;
            break;
        }
    }

    if (skip) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        return false;
    }

    if (sendmmsg(m_fd, headers, size, 0) < 0) {
        LogError(LOG_NET, "Error returned from sendmmsg, err: %d", errno);
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }
    }

    if (sent < 0) {
        LogError(LOG_NET, "Error returned from sendmmsg, err: %d", errno);
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

    return result;
}

/// <summary>
/// Sets the preshared encryption key.
/// </summary>
/// <param name="presharedKey"></param>
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

/// <summary>
/// Helper to lookup a hostname and resolve it to an IP address.
/// </summary>
/// <param name="hostname">String containing hostname to resolve.</param>
/// <param name="port">Numeric port number of service to resolve.</param>
/// <param name="addr">Socket address structure.</param>
/// <param name="addrLen"></param>
/// <returns>Zero if no error during lookup, otherwise error.</returns>
int Socket::lookup(const std::string& hostname, uint16_t port, sockaddr_storage& addr, uint32_t& addrLen)
{
    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));

    return lookup(hostname, port, addr, addrLen, hints);
}

/// <summary>
/// Helper to lookup a hostname and resolve it to an IP address.
/// </summary>
/// <param name="hostname">String containing hostname to resolve.</param>
/// <param name="port">Numeric port number of service to resolve.</param>
/// <param name="addr">Socket address structure.</param>
/// <param name="addrLen"></param>
/// <param name="hints"></param>
/// <returns>Zero if no error during lookup, otherwise error.</returns>
int Socket::lookup(const std::string& hostname, uint16_t port, sockaddr_storage& addr, uint32_t& addrLen, struct addrinfo& hints)
{
    std::string portstr = std::to_string(port);
    struct addrinfo* res;

    // port is always digits, no needs to lookup service
    hints.ai_flags |= AI_NUMERICSERV;

    int err = getaddrinfo(hostname.empty() ? NULL : hostname.c_str(), portstr.c_str(), &hints, &res);
    if (err != 0) {
        sockaddr_in* paddr = (sockaddr_in*)& addr;
        ::memset(paddr, 0x00U, addrLen = sizeof(sockaddr_in));
        paddr->sin_family = AF_INET;
        paddr->sin_port = htons(port);
        paddr->sin_addr.s_addr = htonl(INADDR_NONE);
        LogError(LOG_NET, "Cannot find address for host %s", hostname.c_str());
        return err;
    }

    ::memcpy(&addr, res->ai_addr, addrLen = res->ai_addrlen);

    freeaddrinfo(res);

    return 0;
}

/// <summary>
///
/// </summary>
/// <param name="addr1"></param>
/// <param name="addr2"></param>
/// <param name="type"></param>
/// <returns></returns>
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
bool Socket::initSocket(const int domain, const int type, const int protocol) noexcept(false)
{
    m_fd = ::socket(domain, type, protocol);
    if (m_fd < 0) {
        LogError(LOG_NET, "Cannot create the UDP socket, err: %d", errno);
        return false;
    }

    m_af = domain;
    return true;
}

/// <summary>
///
/// </summary>
/// <param name="ipAddr"></param>
/// <param name="port"></param>
/// <returns></returns>
bool Socket::bind(const std::string& ipAddr, const uint16_t port) noexcept(false)
{
    m_localAddress = std::string(ipAddr);
    m_localPort = port;

    sockaddr_in addr = {};
    initAddr(ipAddr, port, addr);

    socklen_t length = sizeof(addr);
    bool retval = true;
    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), length) < 0) {
        LogError(LOG_NET, "Cannot bind the UDP address, err: %d", errno);
        retval = false;
    }

    return retval;
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