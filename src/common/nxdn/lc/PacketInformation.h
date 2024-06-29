// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file PacketInformation.h
 * @ingroup nxdn_lc
 * @file PacketInformation.cpp
 * @ingroup nxdn_lc
 */
#if !defined(__NXDN_LC__PACKET_INFORMATION_H__)
#define  __NXDN_LC__PACKET_INFORMATION_H__

#include "common/Defines.h"

namespace nxdn
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents the packet information data for link control data.
         * @ingroup nxdn_lc
         */
        class HOST_SW_API PacketInformation {
        public:
            /**
             * @brief Initializes a new instance of the PacketInformation class.
             */
            PacketInformation();
            /**
             * @brief Finalizes a instance of the PacketInformation class.
             */
            ~PacketInformation();

            /**
             * @brief Decodes packet information.
             * @param messageType NXDN Message Type
             * @param[in] data Buffer containing packet information.
             * @returns True, if packet information is decoded, otherwise false.
             */
            bool decode(const uint8_t messageType, const uint8_t* data);
            /**
             * @brief Encodes packet information.
             * @param messageType NXDN Message Type
             * @param[out] data Buffer to encode packet information.
             */
            void encode(const uint8_t messageType, uint8_t* data);

            /**
             * @brief Helper to reset data values to defaults.
             */
            void reset();

        public:
            // Common Data
            /**
             * @brief Flag indicating if confirmed delivery is needed.
             */
            __PROPERTY(bool, delivery, Delivery);
            /**
             * @brief Flag indicating if the packet is a selective retry packet.
             */
            __PROPERTY(bool, selectiveRetry, SelectiveRetry);
            /**
             * @brief Count of data blocks in t he transmission packet.
             */
            __PROPERTY(uint8_t, blockCount, BlockCount);
            /**
             * @brief Number of padding octets of the last block.
             */
            __PROPERTY(uint8_t, padCount, PadCount);
            /**
             * @brief Flag indicating the first fragment.
             */
            __PROPERTY(bool, start, Start);
            /**
             * @brief Flag indicating if the Tx fragment count circulates.
             */
            __PROPERTY(bool, circular, Circular);
            /**
             * @brief The number and sequence of fragments.
             */
            __PROPERTY(uint16_t, fragmentCount, FragmentCount);

            // Response Data
            /**
             * @brief Response class.
             */
            __PROPERTY(uint8_t, rspClass, ResponseClass);
            /**
             * @brief Response type.
             */
            __PROPERTY(uint8_t, rspType, ResponseType);
            /**
             * @brief Error Block Flag.
             */
            __PROPERTY(uint16_t, rspErrorBlock, ResponseErrorBlock);
        };
    } // namespace lc
} // namespace nxdn

#endif // __NXDN_LC__PACKET_INFORMATION_H__
