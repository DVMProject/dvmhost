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
 * @defgroup p25_sndcp Sub Network Dependent Convergence Protocol
 * @brief Implementation for the data hanlding of the TIA-102.BAEB Project 25 standard.
 * @ingroup p25
 * 
 * @file SNDCPPacket.h
 * @ingroup p25_sndcp
 * @file SNDCPPacket.cpp
 * @ingroup p25_sndcp
 */
#if !defined(__P25_SNDCP__SNDCPHEADER_H__)
#define  __P25_SNDCP__SNDCPHEADER_H__

#include "common/Defines.h"
#include "common/Utils.h"

#include <string>

namespace p25
{
    namespace sndcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a SNDCP PDU packet header.
         * @ingroup p25_sndcp
         */
        class HOST_SW_API SNDCPPacket {
        public:
            /**
             * @brief Initializes a copy instance of the SNDCPPacket class.
             * @param data Instance of SNDCPPacket to copy/
             */
            SNDCPPacket(const SNDCPPacket& data);
            /**
             * @brief Initializes a new instance of the SNDCPPacket class.
             */
            SNDCPPacket();
            /**
             * @brief Finalizes a instance of the SNDCPPacket class.
             */
            ~SNDCPPacket();

            /**
             * @brief Decode a SNDCP packet.
             * @param[in] data Buffer containing SNDCP packet data to decode.
             * @returns bool True, if decoded, otherwise false.
             */
            virtual bool decode(const uint8_t* data) = 0;
            /**
             * @brief Encode a SNDCP packet.
             * @param[out] data Buffer to encode SNDCP packet data to.
             */
            virtual void encode(uint8_t* data) = 0;

        public:
            // Common Data
            /**
             * @brief SNDCP PDU Type.
             */
            __PROTECTED_PROPERTY(uint8_t, pduType, PDUType);
            /**
             * @brief Link control opcode.
             */
            __READONLY_PROPERTY(uint8_t, sndcpVersion, SNDCPVersion);
            /**
             * @brief Network Service Access Point Identifier
             */
            __PROTECTED_PROPERTY(uint8_t, nsapi, NSAPI);

        protected:
            /**
             * @brief Internal helper to decode a SNDCP header.
             * @param data Buffer containing SNDCP packet data to decode.
             * @param outbound Flag indicating whether the packet is inbound or outbound.
             * @returns bool True, if decoded, otherwise false.
             */
            bool decodeHeader(const uint8_t* data, bool outbound = false);
            /**
             * @brief Internal helper to encode a SNDCP header.
             * @param data Buffer to encode SNDCP packet data to.
             * @param outbound Flag indicating whether the packet is inbound or outbound.
             */
            void encodeHeader(uint8_t* data, bool outbound = false);

            __PROTECTED_COPY(SNDCPPacket);
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCPHEADER_H__
