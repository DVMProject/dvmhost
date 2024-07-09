// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
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

#include <cassert>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <ifaddrs.h>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define DEFAULT_MTU_SIZE 496

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

uint32_t VIFace::m_idSeq = 0U;

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
            LogError(LOG_NET, "Unable to create the Tx/Rx socket channel %s, queue: %d, err: %d, error: %s", name.c_str(), i, errno, strerror(errno));
            goto hookErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));

        strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

        // obtains the network index number
        if (ioctl(fd, SIOCGIFINDEX, &ifr) != 0) {
            LogError(LOG_NET, "Unable to get network index number %s, queue: %d, err: %d, error: %s", name.c_str(), i, errno, strerror(errno));
            goto hookErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        struct sockaddr_ll socket_addr;
        memset(&socket_addr, 0, sizeof(struct sockaddr_ll));

        socket_addr.sll_family = AF_PACKET;
        socket_addr.sll_protocol = htons(ETH_P_ALL);
        socket_addr.sll_ifindex = ifr.ifr_ifindex;

        // binds the socket to the 'socket_addr' address
        if (bind(fd, (struct sockaddr*) &socket_addr, sizeof(socket_addr)) != 0) {
            LogError(LOG_NET, "Unable to bind the Tx/Rx socket channel to the network interface %s, queue: %d, err: %d, error: %s", name.c_str(), i, errno, strerror(errno));
            goto hookErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        ((int *)queues)[i] = fd;
    }

    return;

hookErr:
    // Rollback close file descriptors
    for (--i; i >= 0; i--) {
        if (close(((int *)queues)[i]) < 0) {
            LogError(LOG_NET, "Unable to close a Rx/Tx socket %s, queue: %d, err: %d, error: %s", name.c_str(), i, errno, strerror(errno));
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
            LogError(LOG_NET, "Unable to open TUN/TAP device %s, queue: %d, err: %d, error: %s", name.c_str(), i, errno, strerror(errno));
            goto allocErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
        }

        // register a network device with the kernel
        if (ioctl(fd, TUNSETIFF, (void *)&ifr) != 0) {
            LogError(LOG_NET, "Unable to register a TUN/TAP device %s, queue: %d, err: %d, error: %s", name.c_str(), i, errno, strerror(errno));
            if (close(fd) < 0) {
                LogError(LOG_NET, "Unable to close a TUN/TAP device %s, queue: %d, err: %d, error: %s", name.c_str(), i, errno, strerror(errno));
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
            LogError(LOG_NET, "Unable to close a TUN/TAP device %s, queue: %d, err: %d, error: %s", name.c_str(), i, errno, strerror(errno));
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
        LogError(LOG_NET, "Unable to read virtual interface flags %s, err: %d, error: %s", name.c_str(), errno, strerror(errno));
    }
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
        LogError(LOG_NET, "Unable to open MTU file for virtual interface %s, err: %d, error: %s", name.c_str(), errno, strerror(errno));
        goto readMTUErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
    }

    // reads MTU value
    nread = read(fd, buffer, size);
    buffer[size] = '\0';

    // Handles errors
    if (nread == -1) {
        LogError(LOG_NET, "Unable to read MTU for virtual interface %s, err: %d, error: %s", name.c_str(), errno, strerror(errno));
        goto readMTUErr; // bryanb: no good very bad way to handle this -- but if its good enough for the Linux kernel its good enough for us right? this is easiset way to clean up quickly...
    }

    if (close(fd) < 0) {
        LogError(LOG_NET, "Unable to close MTU file for virtual interface %s, err: %d, error: %s", name.c_str(), errno, strerror(errno));
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

    // create socket channels to the NET kernel for later ioctl
    m_ksFd = -1;
    m_ksFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_ksFd < 0) {
        LogError(LOG_NET, "Unable to create IPv4 socket channel to the NET kernel %s, err: %d, error: %s", name.c_str(), errno, strerror(errno));
        throw std::runtime_error("Unable to create IPv4 socket channel to the NET kernel.");
    }

    // Set id
    if (id < 0) {
        m_id = m_idSeq;
        m_idSeq++;
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
            LogError(LOG_NET, "Unable to set MAC address %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
            return;
        }
    }

    // set IPv4 related
    struct sockaddr_in* addr = (struct sockaddr_in*) &ifr.ifr_addr;
    addr->sin_family = AF_INET;

    // address
    if (!m_ipv4Address.empty()) {
        if (!inet_pton(AF_INET, m_ipv4Address.c_str(), &addr->sin_addr)) {
            LogError(LOG_NET, "Invalid cached IPv4 address %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
            return;
        }

        if (ioctl(m_ksFd, SIOCSIFADDR, &ifr) != 0) {
            LogError(LOG_NET, "Unable to set IPv4 address %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
            return;
        }
    }

    // netmask
    if (!m_ipv4Netmask.empty()) {
        if (!inet_pton(AF_INET, m_ipv4Netmask.c_str(), &addr->sin_addr)) {
            LogError(LOG_NET, "Invalid cached IPv4 netmask %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
            return;
        }

        if (ioctl(m_ksFd, SIOCSIFNETMASK, &ifr) != 0) {
            LogError(LOG_NET, "Unable to set IPv4 netmask %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
            return;
        }
    }

    // broadcast
    if (!m_ipv4Broadcast.empty()) {
        if (!inet_pton(AF_INET, m_ipv4Broadcast.c_str(), &addr->sin_addr)) {
            LogError(LOG_NET, "Invalid cached IPv4 broadcast %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
            return;
        }

        if (ioctl(m_ksFd, SIOCSIFBRDADDR, &ifr) != 0) {
            LogError(LOG_NET, "Unable to set IPv4 broadcast %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
            return;
        }
    }

    // MTU
    ifr.ifr_mtu = m_mtu;
    if (ioctl(m_ksFd, SIOCSIFMTU, &ifr) != 0) {
        LogError(LOG_NET, "Unable to set MTU %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
        return;
    }

    // bring-up interface
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(m_ksFd, SIOCSIFFLAGS, &ifr) != 0) {
        LogError(LOG_NET, "Unable to bring-up virtual interface %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
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
        LogError(LOG_NET, "Unable to bring-down virtual interface %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
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

    int _errno = 0;
    
    // read packet into our buffer
    ssize_t len = ::read(m_queues.rxFd, buffer, m_mtu);
    _errno = errno;

    // kernel writes incoming packets to BOTH queues, so we
    // need to read from both
    if (((len < 0) && (_errno == EAGAIN)) || (len == 0)) {
        len = ::read(m_queues.txFd, buffer, m_mtu);
        _errno = errno;
    }

    if (len == -1) {
        LogError(LOG_NET, "Error returned from read, err: %d, error: %s", errno, strerror(errno));
    }

    return len;
}

/* Write a packet to this virtual interface. */

bool VIFace::write(const uint8_t* buffer, uint32_t length, ssize_t* lenWritten)
{
    assert(buffer != nullptr);

    if (length < ETH_HLEN) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        LogError(LOG_NET, "Packet is too small, err: %d, error: %s", errno, strerror(errno));
        return false;
    }

    if (length > m_mtu) {
        if (lenWritten != nullptr) {
            *lenWritten = -1;
        }

        LogError(LOG_NET, "Packet is too large, err: %d, error: %s", errno, strerror(errno));
        return false;
    }

    // write packet to TX queue
    bool result = false;
    ssize_t sent = ::write(m_queues.txFd, buffer, length);
    if (sent < 0) {
        LogError(LOG_NET, "Error returned from write, err: %d, error: %s", errno, strerror(errno));

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
        LogError(LOG_NET, "Unable to get MAC address for virtual interface %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
        return std::string();
    }

    // convert binary MAC address to string
    std::ostringstream addr;
    addr << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++) {
        addr << std::setw(2) << (uint8_t)(0xFF & ifr.ifr_hwaddr.sa_data[i]);
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
        LogError(LOG_NET, "Invalid IPv4 address %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
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
        LogError(LOG_NET, "Invalid IPv4 address %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
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
        LogError(LOG_NET, "Invalid IPv4 address %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
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
        LogError(LOG_NET, "MTU %d is too small %s, err: %d, error: %s", mtu, m_name.c_str(), errno, strerror(errno));
        return;
    }

    // are we sure about this upper validation?
    // lo interface reports this number for its MTU
    if (mtu > 65536) {
        LogError(LOG_NET, "MTU %d is too large %s, err: %d, error: %s", mtu, m_name.c_str(), errno, strerror(errno));
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
        LogError(LOG_NET, "Unable to get MTU address for virtual interface %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
        return 0U;
    }

    return ifr.ifr_mtu;
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
        LogError(LOG_NET, "Unable to get IPv4 address for virtual interface %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
        return std::string();
    }

    // convert binary IP address to string
    char addr[INET_ADDRSTRLEN];
    ::memset(&addr, 0, sizeof(addr));

    struct sockaddr_in* ipaddr = (struct sockaddr_in*) &ifr.ifr_addr;
    if (inet_ntop(AF_INET, &(ipaddr->sin_addr), addr, sizeof(addr)) == NULL) {
        LogError(LOG_NET, "Unable to convert IPv4 address for virtual interface %s, err: %d, error: %s", m_name.c_str(), errno, strerror(errno));
        return std::string();
    }

    return std::string(addr);
}
