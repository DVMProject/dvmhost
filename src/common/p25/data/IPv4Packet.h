// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file IPv4Packet.h
 * @ingroup p25_pdu
 * @file IPv4Packet.cpp
 * @ingroup p25_pdu
 */
#if !defined(__P25_DATA__IPV4PACKET_H__)
#define  __P25_DATA__IPV4PACKET_H__

#include "common/Defines.h"

#include <cstdint>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Struct Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents validated fields from an IPv4 packet.
         * @ingroup p25_pdu
         */
        struct DVM_COMMON_API IPv4Packet {
            /**
             * @brief The length of the IPv4 header in bytes.
             */
            uint8_t headerLength = 0U;
            /**
             * @brief The protocol field from the IPv4 header.
             */
            uint8_t protocol = 0U;
            /**
             * @brief The total length of the IPv4 packet in bytes.
             */
            uint16_t totalLength = 0U;
            /**
             * @brief The source IPv4 address.
             */
            uint32_t sourceAddress = 0U;
            /**
             * @brief The destination IPv4 address.
             */
            uint32_t destinationAddress = 0U;

            /**
             * @brief Parses and validates an IPv4 packet.
             * @param data Pointer to the raw IPv4 packet data.
             * @param length Length of the raw data in bytes.
             * @param packet Reference to an IPv4Packet structure to populate with parsed data.
             * @return True if the packet is successfully parsed and validated, false otherwise.
             */
            static bool parse(const uint8_t* data, uint32_t length, IPv4Packet& packet);
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__IPV4PACKET_H__
