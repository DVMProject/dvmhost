// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - TG Patch
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017,2018,2020,2026 by Jonathan Naylor G4KLX
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DMRNetwork.h
 * @ingroup patch_mmdvm
 */
#if !defined(__MMDVM_DMR_NETWORK_H__)
#define __MMDVM_DMR_NETWORK_H__

#include "common/dmr/data/NetData.h"
#include "common/network/udp/Socket.h"
#include "common/RingBuffer.h"

#include <cstdint>
#include <random>
#include <string>

namespace mmdvm
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements the MMDVM Homebrew DMR transport used by DMRGateway.
     * @ingroup patch_mmdvm
     */
    class DMRNetwork {
    public:
        /**
         * @brief Initializes a new instance of the DMRNetwork class.
         * @param gatewayAddress Network hostname or address of DMRGateway.
         * @param gatewayPort DMRGateway local UDP port.
         * @param localPort Local UDP port presented as the MMDVM endpoint.
         * @param repeaterId Numeric repeater ID placed in outbound DMRD records.
         * @param debug Flag indicating whether packet debugging is enabled.
         */
        DMRNetwork(const std::string& gatewayAddress, uint16_t gatewayPort, uint16_t localPort, uint32_t repeaterId, bool debug);
        /** 
         * @brief Finalizes an instance of the DMRNetwork class. 
         */
        ~DMRNetwork();

        /**
         * @brief Reads one received Homebrew DMRD record.
         * @param[out] data Decoded DMR burst and metadata.
         * @param[out] streamId Homebrew stream identifier from the record.
         * @return bool True if a record was read, otherwise false.
         */
        bool read(dmr::data::NetData& data, uint32_t& streamId);
        /**
         * @brief Queues one DMR burst for paced Homebrew transmission.
         * @param[in] data DMR burst and metadata to transmit.
         * @return bool True if the record was queued, otherwise false.
         */
        bool write(const dmr::data::NetData& data);

        /**
         * @brief Clocks receive, ping, and paced transmit processing.
         * @param ms Elapsed milliseconds.
         */
        void clock(uint32_t ms);
        /** 
         * @brief Opens the UDP transport.
         * @return bool True on success.
         */
        bool open();
        /** 
         * @brief Closes the UDP transport.
         */
        void close();

    private:
        network::udp::Socket m_socket;

        sockaddr_storage m_addr;
        uint32_t m_addrLen;

        uint32_t m_repeaterId;

        bool m_debug;

        RingBuffer<uint8_t> m_rxBuffer;
        RingBuffer<uint8_t> m_txBuffer;

        uint32_t m_txElapsed;
        uint32_t m_pingElapsed;

        uint32_t m_streamId[2U];

        std::mt19937 m_random;

        /** 
         * @brief Creates a non-zero Homebrew stream identifier.
         * @return uint32_t Stream identifier.
         */
        uint32_t createStreamId();
    };
} // namespace mmdvm

#endif // __MMDVM_DMR_NETWORK_H__
