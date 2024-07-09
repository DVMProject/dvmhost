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
 * @defgroup viface Virtual Interface TUN/TAP
 * @brief Implementation for handling, manipulating and interfacing with TUN/TAP interfaces.
 * @ingroup socket
 * 
 * @file VIFace.h
 * @ingroup viface
 * @file VIFace.cpp
 * @ingroup viface
 */
#if !defined(__VIFACE_H__)
#define __VIFACE_H__

#include "common/Defines.h"

#include <vector>

namespace network
{
    namespace viface
    {
        // ---------------------------------------------------------------------------
        //  Structure Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief 
         * @ingroup viface 
         */
        struct viface_queues
        {
            int rxFd;               //! Receive Packet File Descriptor
            int txFd;               //! Transmit Packet File Descriptor
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief This class implements virtual network interface routines to communicate using a virtual
         *  network interface.
         * @ingroup viface
         */
        class HOST_SW_API VIFace {
        public:
            /**
             * @brief Initializes a new instance of the VIFace class.
             * @param name Name of the virtual interface. The placeholder %d can be used and a number will be assigned to it.
             * @param tap Tap device (default, true) or Tun device (false).
             * @param id Optional numeric ID. If given id < 0 a sequential number will be given.
             */
            explicit VIFace(std::string name = "viface%d", bool tap = true, int id = -1);
            /**
             * @brief Finalizes a instance of the VIFace class.
             */
            ~VIFace();

            /**
             * @brief Helper to get the unique associated numerical ID for the interface.
             * @returns uint32_t The numeric id of the virtual interface.
             */
            uint32_t getID() const { return m_id; }

            /**
             * @brief Bring up the virtual interface.
             *
             *  This call will configure and bring up the interface.
             *  Exceptions are thrown in case of configuration or bring-up
             *  failures.
             */
            void up();
            /**
             * @brief Bring down the virtual interface.
             * 
             *  Exceptions are thrown in case of misbehaviours.
             */
            void down() const;

            /**
             * @brief Flag indicating wether or not the virtual interface is up.
             * @returns bool True, if virtual interface is up, otherwise false.
             */
            bool isUp() const;

            /**
             * @brief Read a packet from the virtual interface.
             *
             * Note: Reading a packet from a virtual interface means that another
             * userspace program (or the kernel) sent a packet to the network
             * interface with the name of the instance of this class. If not packet
             * was available, and empty vector is returned.
             *
             * @param[out] buffer The packet (if tun) or frame (if tap) as a binary blob
             *  (array of bytes).
             * @returns ssize_t Actual length of data read from remote UDP socket.
             */
            ssize_t read(uint8_t* buffer);
            /**
             * @brief Write a packet to this virtual interface.
             *
             * Note: Writing a packet to this virtual interface means that it
             * will reach any userspace program (or the kernel) listening for
             * packets in the interface with the name of the instance of this
             * class.
             *
             * @param[in] buffer Packet (if tun) or frame (if tap) to send as a
             *  binary blob (array of bytes).
             * @param length Length of data to write.
             * @param[out] lenWritten Total number of bytes written.
             * @returns bool True, if message was sent otherwise, false.
             */
            bool write(const uint8_t* buffer, uint32_t length, ssize_t* lenWritten = nullptr);

            /**
             * @brief Set the MAC address of the virtual interface.
             *
             *  The format of the MAC address is verified, but is just until up()
             *  is called that the library will try to attempt to write it.
             *  If you don't provide a MAC address (the default) one will be
             *  automatically assigned when bringing up the interface.
             *
             * @param mac New MAC address for this virtual interface in the form "d8:9d:67:d3:65:1f".
             */
            void setMAC(std::string mac);
            /**
             * @brief Gets the virtual interfaces associated MAC address.
             * @returns std::string The current MAC address of the virtual interface.
             *  An empty string means no associated MAC address.
             */
            std::string getMAC() const;
            
            /**
             * @brief Set the IPv4 address of the virtual interface.
             *
             *  The format of the IPv4 address is verified, but is just until up()
             *  is called that the library will try to attempt to write it.
             *
             * @param addr New IPv4 address for this virtual interface in the form "172.17.42.1".
             */
            void setIPv4(std::string address);

            /**
             * @brief Gets the IPV4 address of the virtual interface.
             * @returns std::string The current IPv4 address of the virtual interface.
             *  An empty string means no associated IPv4 address.
             */
            std::string getIPv4() const;

            /**
             * @brief Set the IPv4 netmask of the virtual interface.
             *
             *  The format of the IPv4 netmask is verified, but is just until up()
             *  is called that the library will try to attempt to write it.
             *
             * @param netmask New IPv4 netmask for this virtual interface in the form "255.255.255.0".
             */
            void setIPv4Netmask(std::string netmask);
            /**
             * @brief Gets the IPv4 netmask of the virtual interface.
             * @return std::string The current IPv4 netmask of the virtual interface.
             *  An empty string means no associated IPv4 netmask.
             */
            std::string getIPv4Netmask() const;

            /**
             * @brief Set the IPv4 broadcast address of the virtual interface.
             *
             *  The format of the IPv4 broadcast address is verified, but is just
             *  until up() is called that the library will try to attempt to write
             *  it.
             *
             * @param broadcast New IPv4 broadcast address for this virtual interface in the form "172.17.42.255".
             */
            void setIPv4Broadcast(std::string broadcast);
            /**
             * @brief Gets the IPv4 broadcast address of the virtual interface.
             * @return std::string The current IPv4 broadcast address of the virtual interface.
             *  An empty string means no associated IPv4 broadcast address.
             */
            std::string getIPv4Broadcast() const;

            /**
             * @brief Sets the MTU of the virtual interface.
             *
             *  The range of the MTU is verified, but is just until up() is called
             *  that the library will try to attempt to write it.
             *
             * @param mtu New MTU for this virtual interface.
             */
            void setMTU(uint32_t mtu);
            /**
             * @brief Gets the MTU of the virtual interface.
             *
             *  MTU is the size of the largest packet or frame that can be sent
             *  using this interface.
             *
             * @returns uint32_t The current MTU of the virtual interface.
             */
            uint32_t getMTU() const;

        public:
            /** 
             * @brief Virtual Interface associated name.
             */
            __PROPERTY(std::string, name, Name);

        private:            
            struct viface_queues m_queues;

            int m_ksFd;

            std::string m_mac;
            std::string m_ipv4Address;
            std::string m_ipv4Netmask;
            std::string m_ipv4Broadcast;

            uint32_t m_mtu;

            uint32_t m_id;
            static uint32_t m_idSeq;

            /**
             * @brief Internal helper that performs a kernel IOCTL to get the IPv4 address by request.
             * @param request IOCTL request.
             * @return std::string IPv4 address.
             */
            std::string ioctlGetIPv4(uint64_t request) const;
        };
    } // namespace viface
} // namespace network

#endif // __VIFACE_H__
