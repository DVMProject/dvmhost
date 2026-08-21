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
 * @file AMBE2FEC.h
 * @ingroup edac
 * @file AMBE2FEC.cpp
 * @ingroup edac
 */
#if !defined(__AMBE2_FEC_H__)
#define __AMBE2_FEC_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Structure Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Reference to a bit in a P25 Phase 2 half-rate codeword.
     * @ingroup edac
     */
    struct Ambe2BitRef {
        uint8_t word;
        uint8_t bit;
    };

    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    // TIA-102.BABA-A Annex S, symbols 0 through 35.
    const Ambe2BitRef ANNEX_S[36U][2U] = {
        {{0U,23U},{0U,5U}},  {{1U,10U},{2U,3U}}, {{0U,22U},{0U,4U}},  {{1U,9U},{2U,2U}},
        {{0U,21U},{0U,3U}},  {{1U,8U},{2U,1U}},  {{0U,20U},{0U,2U}},  {{1U,7U},{2U,0U}},
        {{0U,19U},{0U,1U}},  {{1U,6U},{3U,13U}}, {{0U,18U},{0U,0U}},  {{1U,5U},{3U,12U}},
        {{0U,17U},{1U,22U}}, {{1U,4U},{3U,11U}}, {{0U,16U},{1U,21U}}, {{1U,3U},{3U,10U}},
        {{0U,15U},{1U,20U}}, {{1U,2U},{3U,9U}},  {{0U,14U},{1U,19U}}, {{1U,1U},{3U,8U}},
        {{0U,13U},{1U,18U}}, {{1U,0U},{3U,7U}},  {{0U,12U},{1U,17U}}, {{2U,10U},{3U,6U}},
        {{0U,11U},{1U,16U}}, {{2U,9U},{3U,5U}},  {{0U,10U},{1U,15U}}, {{2U,8U},{3U,4U}},
        {{0U,9U},{1U,14U}},  {{2U,7U},{3U,3U}},  {{0U,8U},{1U,13U}},  {{2U,6U},{3U,2U}},
        {{0U,7U},{1U,12U}},  {{2U,5U},{3U,1U}},
        // Annex S source erratum: PDF prints c0(5); c0(6) is required.
        {{0U,6U},{1U,11U}},  {{2U,4U},{3U,0U}}
    };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements P25 Phase 2 half-rate AMBE+2 voice FEC.
     * @ingroup edac
     *
     * The input/output buffer is the 72-bit deinterleaved half-rate codeword:
     * c0[24], c1[23], c2[11], c3[14], MSB first. Annex S conversion between
     * transmitted dibits and this codeword is intentionally separate.
     */
    class DVM_COMMON_API AMBE2FEC {
    public:
        /**
         * @brief Regenerates the P25 Phase 2 half-rate FEC.
         * @param bytes 9-byte, 72-bit deinterleaved codeword.
         * @returns uint32_t Number of corrected codeword errors.
         */
        uint32_t regenerate(uint8_t* bytes) const;

        /**
         * @brief Regenerates both no-sync voice codewords in a Phase 2 4V/2V burst.
         * @param burst 40-byte host-side burst representation.
         * @param inbound True for IEMI/inbound layout, false for OEMI/outbound layout.
         * @returns uint32_t Number of corrected codeword errors.
         */
        uint32_t regenerateBurst(uint8_t* burst, bool inbound, bool fourVoice) const;

        /**
         * @brief Encodes the prioritized half-rate parameter words.
         * @param[out] bytes 9-byte, 72-bit deinterleaved codeword.
         * @param u0 First 12-bit prioritized word.
         * @param u1 Second 12-bit prioritized word.
         * @param u2 Third 11-bit prioritized word.
         * @param u3 Fourth 14-bit prioritized word.
         */
        void encode(uint8_t* bytes, uint16_t u0, uint16_t u1, uint16_t u2, uint16_t u3) const;

        /**
         * @brief Converts a deinterleaved codeword to the 36 transmitted dibits.
         * @param[out] frame 9-byte, 72-bit transmitted frame.
         * @param codeword 9-byte, 72-bit deinterleaved codeword.
         */
        void interleave(uint8_t* frame, const uint8_t* codeword) const;

        /**
         * @brief Converts 36 transmitted dibits to a deinterleaved codeword.
         * @param[out] codeword 9-byte, 72-bit deinterleaved codeword.
         * @param frame 9-byte, 72-bit transmitted frame.
         */
        void deinterleave(uint8_t* codeword, const uint8_t* frame) const;

    private:
        /**
         * @brief Returns the bit offset of the specified half-rate word.
         * @param word Half-rate word index (0-3).
         * @returns uint32_t Bit offset of the specified half-rate word.
         */
        static uint32_t wordOffset(uint8_t word);
        /**
         * @brief Returns the bit width of the specified half-rate word.
         * @param word Half-rate word index (0-3).
         * @returns uint32_t Bit width of the specified half-rate word.
         */
        static uint32_t wordWidth(uint8_t word);
        /**
         * @brief Returns the bit reference of the specified half-rate word and bit.
         * @param word Half-rate word index (0-3).
         * @param bit Bit index within the specified half-rate word.
         * @returns Ambe2BitRef Bit reference of the specified half-rate word and bit.
         */
        static uint8_t readWordBit(const uint8_t *codeword, const Ambe2BitRef &ref);
        /**
         * @brief Writes a bit to the specified half-rate word and bit.
         * @param codeword 9-byte, 72-bit deinterleaved codeword.
         * @param ref Bit reference of the specified half-rate word and bit.
         * @param bit Bit value to write (0 or 1).
         */
        static void writeWordBit(uint8_t *codeword, const Ambe2BitRef &ref, uint8_t bit);

        /**
         * @brief Reads a bit field from a byte array.
         * @param bytes Byte array to read from.
         * @param offset Bit offset to start reading from.
         * @param length Number of bits to read.
         * @returns uint32_t Value of the read bit field.
         */
        static uint32_t readBits(const uint8_t *bytes, uint32_t offset, uint32_t length);
        /**
         * @brief Writes a bit field to a byte array.
         * @param bytes Byte array to write to.
         * @param offset Bit offset to start writing to.
         * @param length Number of bits to write.
         * @param value Value of the bit field to write.
         */
        static void writeBits(uint8_t* bytes, uint32_t offset, uint32_t length, uint32_t value);
        /**
         * @brief Generates a pseudo-random number based on the input value.
         * @param u0 Input value to generate the pseudo-random number from.
         * @returns uint32_t Generated pseudo-random number.
         */
        static uint32_t makePN(uint32_t u0);
    };
}

#endif // __AMBE2_FEC_H__
