// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2021 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file P25Utils.h
 * @ingroup p25
 * @file P25Utils.cpp
 * @ingroup p25
 */
#if !defined(__P25_UTILS_H__)
#define __P25_UTILS_H__

#include "common/Defines.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief This class implements various helper functions for validating and
     *  for interleaving P25 data.
     * @ingroup p25
     */
    class HOST_SW_API P25Utils {
    public:
        /**
         * @brief Helper to test and clamp a P25 NAC.
         * @param nac Network Access Code
         * @returns uint32_t Clamped network access code.
         */
        static uint32_t nac(uint32_t nac)
        {
            if (nac < 0U) { // clamp to $000
                nac = 0U;
            }
            if (nac > 0xF7FU) { // clamp to $F7F
                nac = 0xF7FU;
            }

            return nac;
        }

        /**
         * @brief Helper to test and clamp a P25 site ID.
         * @param id Site ID
         * @returns uint8_t Clamped site ID.
         */
        static uint8_t siteId(uint8_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFEU) { // clamp to $FE
                id = 0xFEU;
            }

            return id;
        }

        /**
         * @brief Helper to test and clamp a P25 network ID.
         * @param id Network ID
         * @returns uint32_t Clamped network ID.
         */
        static uint32_t netId(uint32_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFFFFEU) { // clamp to $FFFFE
                id = 0xFFFFEU;
            }

            return id;
        }

        /**
         * @brief Helper to test and clamp a P25 system ID.
         * @param id System ID
         * @returns uint32_t Clamped system ID.
         */
        static uint32_t sysId(uint32_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFFEU) { // clamp to $FFE
                id = 0xFFEU;
            }

            return id;
        }

        /**
         * @brief Helper to test and clamp a P25 RFSS ID.
         * @param id RFSS ID
         * @returns uint8_t Clamped RFSS ID.
         */
        static uint8_t rfssId(uint8_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFEU) { // clamp to $FE
                id = 0xFEU;
            }

            return id;
        }

        /**
         * @brief Helper to set the status bits on P25 frame data.
         * @param data P25 frame data buffer.
         * @param ssOffset Status symbol offset (bit offset).
         * @param b1 Status Bit 1
         * @param b2 Status Bit 2
         */
        static void setStatusBits(uint8_t* data, uint32_t ssOffset, bool b1, bool b2);
        /**
         * @brief Helper to set the starting status bits on P25 frame data to 1,1 for idle.
         * @param data P25 frame data buffer.
         */
        static void setStatusBitsStartIdle(uint8_t* data);
        /**
         * @brief Helper to set all status bits on a P25 frame data to 1,1 for idle.
         * @param data P25 frame data buffer.
         * @param length Lenght of P25 frame in bits.
         */
        static void setStatusBitsAllIdle(uint8_t* data, uint32_t length);
        /**
         * @brief Helper to add the status bits on P25 frame data.
         *  This appropriately sets the status bits for the P25 frame, starting with 1,0 and then
         *  properly setting 0,1 for inbound traffic, or 1,1 for idle (or 1,0 for unknown).
         * @param data P25 frame data buffer.
         * @param length Lenght of P25 frame in bits.
         * @param busy Flag indicating inbound channel is busy.
         * @param unknown Flag indicating unknown slot state.
         */
        static void addStatusBits(uint8_t *data, uint32_t length, bool busy, bool unknown);
        /**
         * @brief Helper to add the unknown (1,0) status bits on P25 frame data.
         *  This sets the status bits to 1,0 interleaved every variable status bit pair.
         * @param data P25 frame data buffer.
         * @param length Lenght of P25 frame in bits.
         * @param interval Status bit pair interval.
         */
        static void addUnknownStatusBits(uint8_t* data, uint32_t length, uint8_t interval = 5U);
        /**
         * @brief Helper to add the idle (1,1) status bits on P25 frame data.
         *  This sets the status bits to 1,1 interleaved every variable status bit pair.
         * @param data P25 frame data buffer.
         * @param length Lenght of P25 frame in bits.
         * @param interval Status bit pair interval.
         */
        static void addIdleStatusBits(uint8_t* data, uint32_t length, uint8_t interval = 5U);

        /**
         * @brief Decode bit interleaving.
         * @param in Input buffer to deinterleave/decode.
         * @param out Output buffer to place deinterleaved/decoded data.
         * @param start Start bit offset.
         * @param stop Stop bit offset.
         * @returns uint32_t 
         */
        static uint32_t decode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
        /**
         * @brief Encode bit interleaving.
         * @param in Input buffer to interleave/encode.
         * @param out Output buffer to place interleaved/encode data.
         * @param start Start bit offset.
         * @param stop Stop bit offset.
         * @returns uint32_t 
         */
        static uint32_t encode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
        /**
         * @brief Encode bit interleaving for a given length.
         * @param in Input buffer to interleave/encode.
         * @param out Output buffer to place interleaved/encode data.
         * @param length 
         * @returns uint32_t 
         */
        static uint32_t encodeByLength(const uint8_t* in, uint8_t* out, uint32_t length);

        /**
         * @brief Compare two datasets for the given length.
         * @param data1 
         * @param data2 
         * @param length 
         * @returns uint32_t 
         */
        static uint32_t compare(const uint8_t* data1, const uint8_t* data2, uint32_t length);
    };
} // namespace p25

#endif // __P25_UTILS_H__
