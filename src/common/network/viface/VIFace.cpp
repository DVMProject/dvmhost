// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024,2026 Bryan Biedenkapp, N2PLL
 *
 */
#if !defined(_WIN32)
#include "Defines.h"
#include "network/viface/VIFace.h"
#include "Log.h"
#include "Utils.h"

using namespace network;
using namespace network::viface;

#include <stdexcept>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <random>
#include <unordered_map>

#include <cassert>
#include <cstring>
#include <cerrno>
#include <cctype>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <ifaddrs.h>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DEFAULT_MTU_SIZE 496

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

uint32_t VIFace::s_idSeq = 0U;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief Parses a string MAC address into bytes.
 * @param[out] buffer Containing MAC address bytes.
 * @param mac MAC address.
 * @returns uint8_t*
 */
void parseMAC(uint8_t* buffer, std::string const& mac)
{
    assert(buffer != nullptr);

    uint32_t bytes[6U];
    int scans = sscanf(mac.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &bytes[0U], &bytes[1U], &bytes[2U], &bytes[3U], &bytes[4U], &bytes[5U]);

    if (scans != 6) {
        return;
    }

    for (uint8_t i = 0U; i < 6U; i++)
        buffer[i] = (uint8_t)(bytes[i] & 0xFFU);
}

/**
 * @brief Helper routine to hook the virtual interface.
 * @param name Name of the virtual interface.
 * @param queues 
 */
void hookVirtualInterface(std::string name, struct viface_queues* queues)
{
    int fd = -1;

    // creates Tx/Rx sockets and allocates queues
    int i = 0;
    for (i = 0; i < 2; i++) {
        // creates the socket
        fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (fd < 0) {
            LogError(LOG_NET, "Unable to create the Tx/Rx socket channel %s, queue: %d, err: %d (%s)", name.c_str(), i, errno, strerror(errno));
            goto hookErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));

        strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

        // obtains the network index number
        if (ioctl(fd, SIOCGIFINDEX, &ifr) != 0) {
            LogError(LOG_NET, "Unable to get network index number %s, queue: %d, err: %d (%s)", name.c_str(), i, errno, strerror(errno));
            goto hookErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        struct sockaddr_ll socket_addr;
        memset(&socket_addr, 0, sizeof(struct sockaddr_ll));

        socket_addr.sll_family = AF_PACKET;
        socket_addr.sll_protocol = htons(ETH_P_ALL);
        socket_addr.sll_ifindex = ifr.ifr_ifindex;

        // binds the socket to the 'socket_addr' address
        if (bind(fd, (struct sockaddr*) &socket_addr, sizeof(socket_addr)) != 0) {
            LogError(LOG_NET, "Unable to bind the Tx/Rx socket channel to the network interface %s, queue: %d, err: %d (%s)", name.c_str(), i, errno, strerror(errno));
            goto hookErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        ((int *)queues)[i] = fd;
    }

    return;

hookErr:
    // Rollback close file descriptors
    for (--i; i >= 0; i--) {
        if (close(((int *)queues)[i]) < 0) {
            LogError(LOG_NET, "Unable to close a Rx/Tx socket %s, queue: %d, err: %d (%s)", name.c_str(), i, errno, strerror(errno));
        }
    }

    throw std::runtime_error("Failed to hook virtual network interface.");
}

/**
 * @brief Helper routine to allocate and create a virtual network inteface.
 * @param name Name of the virtual interface.
 * @param tap Tap device (default, true) or Tun device (false).
 * @param queues 
 * @returns std::string 
 */
std::string allocateVirtualInterface(std::string name, bool tap, struct viface_queues* queues)
{
    int fd = -1;

    /* 
     * create structure for ioctl call
     * Flags: IFF_TAP   - TAP device (layer 2, ethernet frame)
     *        IFF_TUN   - TUN device (layer 3, IP packet)
     *        IFF_NO_PI - Do not provide packet information
     *        IFF_MULTI_QUEUE - Create a queue of multiqueue device
     */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_NO_PI | IFF_MULTI_QUEUE;
    if (tap) {
        ifr.ifr_flags |= IFF_TAP;
    } else {
        ifr.ifr_flags |= IFF_TUN;
    }

    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

    // allocate queues
    int i = 0;
    for (i = 0; i < 2; i++) {
        // open TUN/TAP device
        fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            LogError(LOG_NET, "Unable to open TUN/TAP device %s, queue: %d, err: %d (%s)", name.c_str(), i, errno, strerror(errno));
            goto allocErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        // register a network device with the kernel
        if (ioctl(fd, TUNSETIFF, (void *)&ifr) != 0) {
            LogError(LOG_NET, "Unable to register a TUN/TAP device %s, queue: %d, err: %d (%s)", name.c_str(), i, errno, strerror(errno));
            if (close(fd) < 0) {
                LogError(LOG_NET, "Unable to close a TUN/TAP device %s, queue: %d, err: %d (%s)", name.c_str(), i, errno, strerror(errno));
            }

            goto allocErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        ((int *)queues)[i] = fd;
    }

    return std::string(ifr.ifr_name);

allocErr:
    // rollback close file descriptors
    for (--i; i >= 0; i--) {
        if (close(((int *)queues)[i]) < 0) {
            LogError(LOG_NET, "Unable to close a TUN/TAP device %s, queue: %d, err: %d (%s)", name.c_str(), i, errno, strerror(errno));
        }
    }

    throw std::runtime_error("Failed to allocate virtual network interface.");
}

/**
 * @brief 
 * @param sockfd 
 * @param name 
 * @param ifr 
 */
void readVIFlags(int sockfd, std::string name, struct ifreq& ifr)
{
    // prepare communication structure
    ::memset(&ifr, 0, sizeof(struct ifreq));

    // set interface name
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

    // read interface flags
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) != 0) {
        LogError(LOG_NET, "Unable to read virtual interface flags %s, err: %d (%s)", name.c_str(), errno, strerror(errno));
    }
}

/**
 * @brief Helper to append a netlink attribute.
 * @param n Netlink message header.
 * @param maxLen Maximum message buffer length.
 * @param type Attribute type.
 * @param data Attribute payload.
 * @param len Attribute payload length.
 * @returns bool True, if attribute appended, otherwise false.
 */
bool nlAddAttr(struct nlmsghdr* n, size_t maxLen, int type, const void* data, size_t len)
{
    assert(n != nullptr);

    const size_t attrLen = RTA_LENGTH(len);
    const size_t newLen = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(attrLen);
    if (newLen > maxLen) {
        return false;
    }

    struct rtattr* rta = (struct rtattr*)(((uint8_t*)n) + NLMSG_ALIGN(n->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = (uint16_t)attrLen;
    if (len > 0U && data != nullptr) {
        ::memcpy(RTA_DATA(rta), data, len);
    }

    n->nlmsg_len = (uint32_t)newLen;
    return true;
}

/**
 * @brief Helper to start a nested netlink attribute.
 * @param n Netlink message header.
 * @param maxLen Maximum message buffer length.
 * @param type Attribute type.
 * @returns rtattr* Nested attribute pointer or nullptr on failure.
 */
struct rtattr* nlNestStart(struct nlmsghdr* n, size_t maxLen, int type)
{
    struct rtattr* nest = (struct rtattr*)(((uint8_t*)n) + NLMSG_ALIGN(n->nlmsg_len));
    if (!nlAddAttr(n, maxLen, type, nullptr, 0U)) {
        return nullptr;
    }
    return nest;
}

/**
 * @brief Helper to finalize a nested netlink attribute.
 * @param n Netlink message header.
 * @param nest Nested attribute pointer.
 */
void nlNestEnd(struct nlmsghdr* n, struct rtattr* nest)
{
    if (n == nullptr || nest == nullptr) {
        return;
    }

    nest->rta_len = (uint16_t)(((uint8_t*)n + NLMSG_ALIGN(n->nlmsg_len)) - (uint8_t*)nest);
}

/**
 * @brief Helper to lowercase a string.
 * @param value Input string.
 * @returns std::string Lowercased string.
 */
std::string toLowerStr(const std::string& value)
{
    std::string out = value;
    for (char& c : out) {
        c = (char)std::tolower((unsigned char)c);
    }
    return out;
}

/**
 * @brief Helper to parse a size string with optional k/m/g suffix into bytes.
 * @param value Input string.
 * @param outBytes Parsed byte value.
 * @returns bool True on success.
 */
bool parseSizeBytes(const std::string& value, uint32_t& outBytes)
{
    if (value.empty()) {
        return false;
    }

    std::string lower = toLowerStr(value);
    uint64_t mult = 1U;
    if (lower.size() > 1U && lower.back() == 'b') {
        lower.pop_back();
    }

    if (!lower.empty()) {
        char sfx = lower.back();
        if (sfx == 'k' || sfx == 'm' || sfx == 'g') {
            lower.pop_back();
            if (sfx == 'k') mult = 1024ULL;
            if (sfx == 'm') mult = 1024ULL * 1024ULL;
            if (sfx == 'g') mult = 1024ULL * 1024ULL * 1024ULL;
        }
    }

    if (lower.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    unsigned long long base = strtoull(lower.c_str(), &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0') {
        return false;
    }

    uint64_t total = base * mult;
    if (total > 0xFFFFFFFFULL) {
        return false;
    }

    outBytes = (uint32_t)total;
    return true;
}

/**
 * @brief Helper to parse a duration string into microseconds.
 * @param value Input value (supports us, ms, s).
 * @param outUsec Parsed microseconds.
 * @returns bool True on success.
 */
bool parseDurationUsec(const std::string& value, uint32_t& outUsec)
{
    if (value.empty()) {
        return false;
    }

    std::string lower = toLowerStr(value);
    uint64_t mult = 1U;

    if (lower.size() >= 2U && lower.substr(lower.size() - 2U) == "us") {
        mult = 1U;
        lower = lower.substr(0U, lower.size() - 2U);
    }
    else if (lower.size() >= 2U && lower.substr(lower.size() - 2U) == "ms") {
        mult = 1000U;
        lower = lower.substr(0U, lower.size() - 2U);
    }
    else if (!lower.empty() && lower.back() == 's') {
        mult = 1000000U;
        lower.pop_back();
    }

    if (lower.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    unsigned long long base = strtoull(lower.c_str(), &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0') {
        return false;
    }

    uint64_t total = base * mult;
    if (total > 0xFFFFFFFFULL) {
        return false;
    }

    outUsec = (uint32_t)total;
    return true;
}

/**
 * @brief Parses qdisc args string into key/value pairs.
 * Supports "key=value" and "key value" forms.
 * @param args Input args string.
 * @returns unordered_map Parsed key/value map.
 */
std::unordered_map<std::string, std::string> parseQDiscArgs(const std::string& args)
{
    std::unordered_map<std::string, std::string> out;
    if (args.empty()) {
        return out;
    }

    std::vector<std::string> tokens;
    std::string token;
    for (char c : args) {
        if (std::isspace((unsigned char)c)) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
        else {
            token.push_back(c);
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }

    for (size_t i = 0U; i < tokens.size(); i++) {
        std::string t = tokens[i];
        size_t eq = t.find('=');
        if (eq != std::string::npos) {
            std::string k = toLowerStr(t.substr(0U, eq));
            std::string v = t.substr(eq + 1U);
            if (!k.empty() && !v.empty()) {
                out[k] = v;
            }
            continue;
        }

        if ((i + 1U) < tokens.size()) {
            std::string k = toLowerStr(t);
            std::string v = tokens[i + 1U];
            out[k] = v;
            i++;
        }
    }

    return out;
}

/**
 * @brief 
 * @param name 
 * @param size 
 * @return uint 
 */
uint32_t readMTU(std::string name, size_t size)
{
    int fd = -1, nread = -1;
    char buffer[size + 1];

    // opens MTU file
    fd = open(("/sys/class/net/" + name + "/mtu").c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        LogError(LOG_NET, "Unable to open MTU file for virtual interface %s, err: %d (%s)", name.c_str(), errno, strerror(errno));
        goto readMTUErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
    }

    // reads MTU value
    nread = read(fd, buffer, size);
    buffer[size] = '\0';

    // Handles errors
    if (nread == -1) {
        LogError(LOG_NET, "Unable to read MTU for virtual interface %s, err: %d (%s)", name.c_str(), errno, strerror(errno));
        goto readMTUErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
    }

    if (close(fd) < 0) {
        LogError(LOG_NET, "Unable to close MTU file for virtual interface %s, err: %d (%s)", name.c_str(), errno, strerror(errno));
        goto readMTUErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
    }

    return strtoul(buffer, nullptr, 10);

readMTUErr:
    // rollback close file descriptor
    close(fd);

    throw std::runtime_error("Failed to read virtual network interface MTU.");
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the VIFace class. */

VIFace::VIFace(std::string name, bool tap, int id) :
    m_ksFd(-1),
    m_epollFd(-1),
    m_mac(),
    m_ipv4Address("192.168.1.254"),
    m_ipv4Netmask("255.255.255.0"),
    m_ipv4Broadcast("192.168.1.255"),
    m_mtu(DEFAULT_MTU_SIZE)
{
    // check name length
    if (name.length() >= IFNAMSIZ) {
        throw std::invalid_argument("Virtual interface name too long.");
    }

    // create queues
    struct viface_queues queues;
    ::memset(&queues, 0, sizeof(struct viface_queues));

    /* 
     * checks if the path name can be accessed. if so,
     * it means that the network interface is already defined
     */
    if (access(("/sys/class/net/" + name).c_str(), F_OK) == 0) {
        hookVirtualInterface(name, &queues);
        m_name = name;

        // read MTU value and resize buffer
        m_mtu = readMTU(name, sizeof(m_mtu));
    } else {
        m_name = allocateVirtualInterface(name, tap, &queues);
        m_mtu = DEFAULT_MTU_SIZE;
    }

    m_queues = queues;

    // epoll create
    m_epollFd = epoll_create1(0);
    if (m_epollFd == -1) {
        LogError(LOG_NET, "Unable to initialize epoll %s, err: %d (%s)", name.c_str(), errno, strerror(errno));
        throw std::runtime_error("Unable to initialize epoll.");
    }

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data = { .fd = m_queues.rxFd },
    };

    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
        LogError(LOG_NET, "Unable to configure epoll %s, err: %d (%s)", name.c_str(), errno, strerror(errno));
        throw std::runtime_error("Unable to configure epoll.");
    }

    ev.data.fd = m_queues.txFd;
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
        LogError(LOG_NET, "Unable to configure epoll %s, err: %d (%s)", name.c_str(), errno, strerror(errno));
        throw std::runtime_error("Unable to configure epoll.");
    }
    // create socket channels to the NET kernel for later ioctl
    m_ksFd = -1;
    m_ksFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_ksFd < 0) {
        LogError(LOG_NET, "Unable to create IPv4 socket channel to the NET kernel %s, err: %d (%s)", name.c_str(), errno, strerror(errno));
        throw std::runtime_error("Unable to create IPv4 socket channel to the NET kernel.");
    }

    // set id
    if (id < 0) {
        m_id = s_idSeq;
        s_idSeq++;
    } else {
        m_id = id;
    }
}

/* Finalizes a instance of the VIFace class. */

VIFace::~VIFace()
{
    close(m_queues.rxFd);
    close(m_queues.txFd);
    close(m_ksFd);
}

/* Bring up the virtual interface. */

void VIFace::up()
{
    if (isUp()) {
        LogError(LOG_NET, "Virtual interface %s is already up.", m_name.c_str());
        return;
    }

    // read interface flags
    struct ifreq ifr;
    readVIFlags(m_ksFd, m_name, ifr);

    // set MAC address
    if (!m_mac.empty()) {
        uint8_t mac[6U];
        ::memset(mac, 0x00U, 6U);

        parseMAC(mac, m_mac);

        ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
        for (int i = 0; i < 6; i++) {
            ifr.ifr_hwaddr.sa_data[i] = mac[i];
        }

        if (ioctl(m_ksFd, SIOCSIFHWADDR, &ifr) != 0) {
            LogError(LOG_NET, "Unable to set MAC address %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
            return;
        }
    }

    // set IPv4 related
    struct sockaddr_in* addr = (struct sockaddr_in*) &ifr.ifr_addr;
    addr->sin_family = AF_INET;

    // address
    if (!m_ipv4Address.empty()) {
        if (!inet_pton(AF_INET, m_ipv4Address.c_str(), &addr->sin_addr)) {
            LogError(LOG_NET, "Invalid cached IPv4 address %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
            return;
        }

        if (ioctl(m_ksFd, SIOCSIFADDR, &ifr) != 0) {
            LogError(LOG_NET, "Unable to set IPv4 address %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
            return;
        }
    }

    // netmask
    if (!m_ipv4Netmask.empty()) {
        if (!inet_pton(AF_INET, m_ipv4Netmask.c_str(), &addr->sin_addr)) {
            LogError(LOG_NET, "Invalid cached IPv4 netmask %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
            return;
        }

        if (ioctl(m_ksFd, SIOCSIFNETMASK, &ifr) != 0) {
            LogError(LOG_NET, "Unable to set IPv4 netmask %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
            return;
        }
    }

    // broadcast
    if (!m_ipv4Broadcast.empty()) {
        if (!inet_pton(AF_INET, m_ipv4Broadcast.c_str(), &addr->sin_addr)) {
            LogError(LOG_NET, "Invalid cached IPv4 broadcast %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
            return;
        }

        if (ioctl(m_ksFd, SIOCSIFBRDADDR, &ifr) != 0) {
            LogError(LOG_NET, "Unable to set IPv4 broadcast %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
            return;
        }
    }

    // MTU
    ifr.ifr_mtu = m_mtu;
    if (ioctl(m_ksFd, SIOCSIFMTU, &ifr) != 0) {
        LogError(LOG_NET, "Unable to set MTU %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
        return;
    }

    // bring-up interface
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(m_ksFd, SIOCSIFFLAGS, &ifr) != 0) {
        LogError(LOG_NET, "Unable to bring-up virtual interface %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
    }
}

/* Bring down the virtual interface. */

void VIFace::down() const
{
    // read interface flags
    struct ifreq ifr;
    readVIFlags(m_ksFd, m_name, ifr);

    // bring-down interface
    ifr.ifr_flags &= ~IFF_UP;
    if (ioctl(m_ksFd, SIOCSIFFLAGS, &ifr) != 0) {
        LogError(LOG_NET, "Unable to bring-down virtual interface %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
    }
}

/* Flag indicating wether or not the virtual interface is up. */

bool VIFace::isUp() const
{
    // read interface flags
    struct ifreq ifr;
    readVIFlags(m_ksFd, m_name, ifr);

    return (ifr.ifr_flags & IFF_UP) != 0;
}

/* Read a packet from the virtual interface. */

ssize_t VIFace::read(uint8_t* buffer)
{
    assert(buffer != nullptr);
    ::memset(buffer, 0x00U, m_mtu);

    struct epoll_event wait_event;

    int ret = epoll_wait(m_epollFd, &wait_event, 1, 0);
    if ((ret < 0) && (errno != EINTR)) {
        LogError(LOG_NET, "Error returned from epoll_wait, err: %d (%s)", errno, strerror(errno));
        return -1;
    }

    if (ret == 0) {
        return -1;
    }
    
    // read packet into our buffer
    if (ret > 0) {
        // Read packet into our buffer
        ssize_t len = ::read(wait_event.data.fd, buffer, m_mtu);
        if (len == -1) {
            LogError(LOG_NET, "Error returned from read, err: %d (%s)", errno, strerror(errno));
        }
    
       return len;
    }

    return -1;
}

/* Write a packet to this virtual interface. */

bool VIFace::write(const uint8_t* buffer, uint32_t length, ssize_t* lenWritten)
{
    assert(buffer != nullptr);

    if (length < ETH_HLEN) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        LogError(LOG_NET, "Packet is too small, err: %d (%s)", errno, strerror(errno));
        return false;
    }

    if (length > m_mtu) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        LogError(LOG_NET, "Packet is too large, err: %d (%s)", errno, strerror(errno));
        return false;
    }

    // write packet to TX queue
    bool result = false;
    ssize_t sent = ::write(m_queues.txFd, buffer, length);
    if (sent < 0) {
        LogError(LOG_NET, "Error returned from write, err: %d (%s)", errno, strerror(errno));

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

/* Set the MAC address of the virtual interface to a random value. */

void VIFace::setRandomMAC()
{
    // generate random MAC
    std::random_device rd;
    std::mt19937 mt(rd());

    std::ostringstream addr;
    addr << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++) {
        uint8_t macByte = 0U;
        std::uniform_int_distribution<uint8_t> dist(1U, 254U);
        macByte = dist(mt);

        addr << std::setw(2) << (uint8_t)(0xFFU & macByte);
        if (i != 5) {
            addr << ":";
        }
    }

    m_mac = addr.str();
}

/* Set the MAC address of the virtual interface. */

void VIFace::setMAC(std::string mac)
{
    m_mac = mac;
}

/* Gets the virtual interfaces associated MAC address. */

std::string VIFace::getMAC() const
{
    // read interface flags
    struct ifreq ifr;
    readVIFlags(m_ksFd, m_name, ifr);

    if (ioctl(m_ksFd, SIOCGIFHWADDR, &ifr) != 0) {
        LogError(LOG_NET, "Unable to get MAC address for virtual interface %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
        return std::string();
    }

    // convert binary MAC address to string
    std::ostringstream addr;
    addr << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++) {
        addr << std::setw(2) << (uint8_t)(0xFFU & ifr.ifr_hwaddr.sa_data[i]);
        if (i != 5) {
            addr << ":";
        }
    }

    return addr.str();
}

/* Set the IPv4 address of the virtual interface. */

void VIFace::setIPv4(std::string address)
{
    struct in_addr addr;
    if (!inet_pton(AF_INET, address.c_str(), &addr)) {
        LogError(LOG_NET, "Invalid IPv4 address %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
        return;
    }

    m_ipv4Address = address;
}

/* Gets the IPV4 address of the virtual interface. */

std::string VIFace::getIPv4() const
{
    return ioctlGetIPv4(SIOCGIFADDR);
}

/* Set the IPv4 netmask of the virtual interface. */

void VIFace::setIPv4Netmask(std::string netmask)
{
    struct in_addr addr;
    if (!inet_pton(AF_INET, netmask.c_str(), &addr)) {
        LogError(LOG_NET, "Invalid IPv4 address %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
        return;
    }

    m_ipv4Netmask = netmask;
}

/* Gets the IPv4 netmask of the virtual interface. */

std::string VIFace::getIPv4Netmask() const
{
    return ioctlGetIPv4(SIOCGIFNETMASK);
}

/* Set the IPv4 broadcast address of the virtual interface. */

void VIFace::setIPv4Broadcast(std::string broadcast)
{
    struct in_addr addr;
    if (!inet_pton(AF_INET, broadcast.c_str(), &addr)) {
        LogError(LOG_NET, "Invalid IPv4 address %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
        return;
    }

    m_ipv4Broadcast = broadcast;
}

/* Gets the IPv4 broadcast address of the virtual interface. */

std::string VIFace::getIPv4Broadcast() const
{
    return ioctlGetIPv4(SIOCGIFBRDADDR);
}

/* Sets the MTU of the virtual interface. */

void VIFace::setMTU(uint32_t mtu)
{
    if (mtu < ETH_HLEN) {
        LogError(LOG_NET, "MTU %d is too small %s, err: %d (%s)", mtu, m_name.c_str(), errno, strerror(errno));
        return;
    }

    // are we sure about this upper validation?
    // lo interface reports this number for its MTU
    if (mtu > 65536) {
        LogError(LOG_NET, "MTU %d is too large %s, err: %d (%s)", mtu, m_name.c_str(), errno, strerror(errno));
        return;
    }

    m_mtu = mtu;
}

/* Gets the MTU of the virtual interface. */

uint32_t VIFace::getMTU() const
{
    // read interface flags
    struct ifreq ifr;
    readVIFlags(m_ksFd, m_name, ifr);

    if (ioctl(m_ksFd, SIOCGIFMTU, &ifr) != 0) {
        LogError(LOG_NET, "Unable to get MTU address for virtual interface %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
        return 0U;
    }

    return ifr.ifr_mtu;
}

/* Applies a root qdisc to the virtual interface using rtnetlink. */

bool VIFace::setQDisc(std::string kind, std::string args)
{
    if (kind.empty()) {
        LogError(LOG_NET, "Invalid qdisc kind for virtual interface %s", m_name.c_str());
        return false;
    }

    kind = toLowerStr(kind);

    struct ifreq ifr;
    ::memset(&ifr, 0x00U, sizeof(struct ifreq));
    ::strncpy(ifr.ifr_name, m_name.c_str(), IFNAMSIZ - 1);

    if (::ioctl(m_ksFd, SIOCGIFINDEX, &ifr) != 0) {
        LogError(LOG_NET, "Unable to get interface index for qdisc on %s, err: %d (%s)",
            m_name.c_str(), errno, strerror(errno));
        return false;
    }

    int nl = ::socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (nl < 0) {
        LogError(LOG_NET, "Unable to open rtnetlink socket for qdisc on %s, err: %d (%s)",
            m_name.c_str(), errno, strerror(errno));
        return false;
    }

    struct {
        struct nlmsghdr n;
        struct tcmsg t;
        uint8_t buf[512U];
    } req;
    ::memset(&req, 0x00U, sizeof(req));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
    req.n.nlmsg_type = RTM_NEWQDISC;
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_REPLACE;
    req.n.nlmsg_seq = 1U;

    req.t.tcm_family = AF_UNSPEC;
    req.t.tcm_ifindex = ifr.ifr_ifindex;
    req.t.tcm_parent = TC_H_ROOT;
    req.t.tcm_handle = 0U;

    if (!nlAddAttr(&req.n, sizeof(req), TCA_KIND, kind.c_str(), kind.length() + 1U)) {
        LogError(LOG_NET, "Unable to append qdisc kind attribute for %s", m_name.c_str());
        ::close(nl);
        return false;
    }

    if (!args.empty() && kind == "fq_codel") {
        std::unordered_map<std::string, std::string> opts = parseQDiscArgs(args);
        struct rtattr* optsNest = nlNestStart(&req.n, sizeof(req), TCA_OPTIONS);
        if (optsNest == nullptr) {
            LogError(LOG_NET, "Unable to append qdisc options for %s", m_name.c_str());
            ::close(nl);
            return false;
        }

        for (const auto& kv : opts) {
            const std::string& key = kv.first;
            const std::string& value = kv.second;

            if (key == "limit") {
                uint32_t v = 0U;
                if (parseSizeBytes(value, v)) {
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_LIMIT, &v, sizeof(v));
                }
            }
            else if (key == "flows") {
                uint32_t v = 0U;
                if (parseSizeBytes(value, v)) {
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_FLOWS, &v, sizeof(v));
                }
            }
            else if (key == "quantum") {
                uint32_t v = 0U;
                if (parseSizeBytes(value, v)) {
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_QUANTUM, &v, sizeof(v));
                }
            }
            else if (key == "memory_limit") {
                uint32_t v = 0U;
                if (parseSizeBytes(value, v)) {
#if defined(TCA_FQ_CODEL_MEMORY_LIMIT)
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_MEMORY_LIMIT, &v, sizeof(v));
#else
                    LogWarning(LOG_NET, "fq_codel memory_limit not supported by kernel headers, interface %s", m_name.c_str());
#endif
                }
            }
            else if (key == "drop_batch_size") {
                uint32_t v = 0U;
                if (parseSizeBytes(value, v)) {
#if defined(TCA_FQ_CODEL_DROP_BATCH_SIZE)
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_DROP_BATCH_SIZE, &v, sizeof(v));
#else
                    LogWarning(LOG_NET, "fq_codel drop_batch_size not supported by kernel headers, interface %s", m_name.c_str());
#endif
                }
            }
            else if (key == "target") {
                uint32_t v = 0U;
                if (parseDurationUsec(value, v)) {
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_TARGET, &v, sizeof(v));
                }
            }
            else if (key == "interval") {
                uint32_t v = 0U;
                if (parseDurationUsec(value, v)) {
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_INTERVAL, &v, sizeof(v));
                }
            }
            else if (key == "ce_threshold") {
                uint32_t v = 0U;
                if (parseDurationUsec(value, v)) {
#if defined(TCA_FQ_CODEL_CE_THRESHOLD)
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_CE_THRESHOLD, &v, sizeof(v));
#else
                    LogWarning(LOG_NET, "fq_codel ce_threshold not supported by kernel headers, interface %s", m_name.c_str());
#endif
                }
            }
            else if (key == "ecn") {
                uint32_t v = 0U;
                if (parseSizeBytes(value, v)) {
                    (void)nlAddAttr(&req.n, sizeof(req), TCA_FQ_CODEL_ECN, &v, sizeof(v));
                }
            }
        }

        nlNestEnd(&req.n, optsNest);
    }
    else if (!args.empty()) {
        LogWarning(LOG_NET, "qdisc args are currently supported only for fq_codel, interface %s", m_name.c_str());
    }

    ssize_t sent = ::send(nl, &req, req.n.nlmsg_len, 0);
    if (sent < 0) {
        LogError(LOG_NET, "Unable to send rtnetlink qdisc request for %s, err: %d (%s)",
            m_name.c_str(), errno, strerror(errno));
        ::close(nl);
        return false;
    }

    uint8_t replyBuf[512U];
    ssize_t recvd = ::recv(nl, replyBuf, sizeof(replyBuf), 0);
    if (recvd < 0) {
        LogError(LOG_NET, "Unable to receive rtnetlink qdisc response for %s, err: %d (%s)",
            m_name.c_str(), errno, strerror(errno));
        ::close(nl);
        return false;
    }

    struct nlmsghdr* nh = (struct nlmsghdr*)replyBuf;
    for (; NLMSG_OK(nh, (uint32_t)recvd); nh = NLMSG_NEXT(nh, recvd)) {
        if (nh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr* err = (struct nlmsgerr*)NLMSG_DATA(nh);
            if (err->error == 0) {
                ::close(nl);
                return true;
            }

            LogError(LOG_NET, "Kernel rejected qdisc %s on %s, err: %d (%s)",
                kind.c_str(), m_name.c_str(), -err->error, strerror(-err->error));
            ::close(nl);
            return false;
        }
    }

    ::close(nl);
    LogError(LOG_NET, "Unexpected rtnetlink response while setting qdisc on %s", m_name.c_str());
    return false;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper that performs a kernel IOCTL to get the IPv4 address by request. */

std::string VIFace::ioctlGetIPv4(uint64_t request) const
{
    // read interface flags
    struct ifreq ifr;
    readVIFlags(m_ksFd, m_name, ifr);

    if (ioctl(m_ksFd, request, &ifr) != 0) {
        LogError(LOG_NET, "Unable to get IPv4 address for virtual interface %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
        return std::string();
    }

    // convert binary IP address to string
    char addr[INET_ADDRSTRLEN];
    ::memset(&addr, 0, sizeof(addr));

    struct sockaddr_in* ipaddr = (struct sockaddr_in*) &ifr.ifr_addr;
    if (inet_ntop(AF_INET, &(ipaddr->sin_addr), addr, sizeof(addr)) == NULL) {
        LogError(LOG_NET, "Unable to convert IPv4 address for virtual interface %s, err: %d (%s)", m_name.c_str(), errno, strerror(errno));
        return std::string();
    }

    return std::string(addr);
}
#endif // !defined(_WIN32)
