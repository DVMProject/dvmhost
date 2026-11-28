// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - IPSC Gateway
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file BurstConverter.h
 * @ingroup ipsc
 * @file BurstConverter.cpp
 * @ingroup ipsc
 */
#if !defined(__IPSC_DMR_BURST_CONVERTER_H__)
#define __IPSC_DMR_BURST_CONVERTER_H__

#include "Defines.h"
#include "common/dmr/data/EmbeddedData.h"
#include "common/dmr/data/NetData.h"

namespace ipsc
{
    namespace dmr
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Converts DMR air-interface bursts to and from IPSC burst payloads.
         * @ingroup ipsc
         */
        class BurstConverter {
        public:
            /**
             * @brief Converts DVM network DMR data into an IPSC burst.
             * @param data DVM network DMR frame.
             * @param colorCode DMR color code to include in burst slot metadata.
             * @param[out] burst Buffer that receives the encoded IPSC burst payload.
             * @param capacity Capacity of the burst buffer in bytes.
             * @param[out] length Length of the encoded IPSC burst payload.
             * @returns bool True if the DMR frame was converted, otherwise false.
             */
            static bool fromDMR(const ::dmr::data::NetData& data, uint8_t colorCode, uint8_t* burst, uint32_t capacity, uint32_t& length);

            /**
             * @brief Converts an IPSC burst into DVM network DMR data.
             * @param burst IPSC burst payload.
             * @param length Length of the IPSC burst payload in bytes.
             * @param srcId DMR source radio ID.
             * @param dstId DMR destination radio or talkgroup ID.
             * @param group Flag indicating group-call operation.
             * @param slotNo DMR slot number.
             * @param seqNo DVM network sequence number.
             * @param n DMR voice burst index.
             * @param colorCode DMR color code used to reconstruct the burst.
             * @param embeddedData Embedded link-control encoder for the active call.
             * @param[out] data Converted DVM network DMR frame.
             * @returns bool True if the IPSC burst was converted, otherwise false.
             */
            static bool toDMR(const uint8_t* burst, uint32_t length, uint32_t srcId, uint32_t dstId, bool group, uint8_t slotNo, uint8_t seqNo, uint8_t n,
                              uint8_t colorCode, ::dmr::data::EmbeddedData& embeddedData, ::dmr::data::NetData& data);

        private:
            /** 
             * @brief Extracts a 49-bit AMBE+2 codec frame from a 72-bit FEC codeword. 
             * @param codeword The 72-bit FEC codeword to decode.
             * @param ambe The extracted 49-bit AMBE+2 codec frame.
             */
            static void decodeAMBE(const uint8_t* codeword, uint8_t* ambe);

            /** 
             * @brief Generates a 72-bit FEC codeword from a 49-bit AMBE+2 codec frame. 
             * @param ambe The 49-bit AMBE+2 codec frame to encode.
             * @param codeword The generated 72-bit FEC codeword.
             */
            static void encodeAMBE(const uint8_t* ambe, uint8_t* codeword);
        };
    } // namespace dmr
} // namespace ipsc

#endif // __IPSC_DMR_BURST_CONVERTER_H__
