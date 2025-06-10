// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2009-2014,2016,2020,2021 by Jonathan Naylor G4KLX
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup patch_mmdvm MMDVM Networking
 * @brief Implementation for the patch MMDVM networking.
 * @ingroup patch
 *
 * @file PeerNetwork.h
 * @ingroup patch_mmdvm
 * @file PeerNetwork.cpp
 * @ingroup patch_mmdvm
 */
#if !defined(__P25_NETWORK_H__)
#define __P25_NETWORK_H__

#include "Defines.h"
#include "common/p25/Audio.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/lc/LC.h"
#include "common/network/udp/Socket.h"
#include "common/RingBuffer.h"

#include <cstdint>
#include <string>

namespace mmdvm
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements the MMDVM networking logic.
    // ---------------------------------------------------------------------------

    class P25Network {
    public:
        /**
         * @brief Initializes a new instance of the P25Network class.
         * @param gatewayAddress Network Hostname/IP address to connect to.
         * @param gatewayPort Network port number.
         * @param localPort
         * @param debug Flag indicating whether network debug is enabled.
         */
        P25Network(const std::string& gatewayAddress, uint16_t gatewayPort, uint16_t localPort, bool debug);
        /**
         * @brief Finalizes a instance of the P25Network class.
         */
        ~P25Network();

        /**
         * @brief Reads P25 raw frame data from the P25 ring buffer.
         * @param[out] data Buffer to write received frame data to.
         * @param[out] length Length in bytes of received frame.
         * @returns 
         */
        uint32_t read(uint8_t* data, uint32_t length);

        /**
         * @brief Writes P25 LDU1 frame data to the network.
         * @param[in] ldu1 Buffer containing P25 LDU1 data to send.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] end 
         * @returns bool True, if message was sent, otherwise false.
         */
        bool writeLDU1(const uint8_t* ldu1, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, bool end);

        /**
         * @brief Writes P25 LDU2 frame data to the network.
         * @param[in] ldu1 Buffer containing P25 LDU2 data to send.
         * @param[in] control Instance of p25::lc::LC containing link control data.
         * @param[in] lsd Instance of p25::data::LowSpeedData containing low speed data.
         * @param[in] end 
         * @returns bool True, if message was sent, otherwise false.
         */
        bool writeLDU2(const uint8_t* ldu2, const p25::lc::LC& control, const p25::data::LowSpeedData& lsd, bool end);

        /**
         * @brief Updates the timer by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms);

        /**
         * @brief Helper to determine if we are connected to a MMDVM gateway.
         * @return bool True, if connected, otherwise false.
         */
        bool isConnected() const;

        /**
         * @brief Opens connection to the network.
         * @returns bool True, if networking has started, otherwise false.
         */
        bool open();

        /**
         * @brief Closes connection to the network.
         */
        void close();

    private:
        network::udp::Socket m_socket;

        sockaddr_storage m_addr;
        uint32_t m_addrLen;

        bool m_debug;

        RingBuffer<uint8_t> m_buffer;

        p25::Audio m_audio;
    };
} // namespace mmdvm

#endif // __P25_NETWORK_H__
