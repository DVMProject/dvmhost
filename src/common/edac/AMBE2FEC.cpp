// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/edac/AMBE2FEC.h"
#include "common/edac/Golay24128.h"
#include "common/Utils.h"

#include <cassert>
#include <cstring>

using namespace edac;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Regenerates the P25 Phase 2 half-rate FEC. */

uint32_t AMBE2FEC::regenerate(uint8_t* bytes) const
{
    assert(bytes != nullptr);

    uint32_t c0 = readBits(bytes, 0U, 24U);
    uint32_t c1 = readBits(bytes, 24U, 23U);
    uint32_t u0 = 0U;
    bool c0Valid = Golay24128::decode24128(c0, u0);
    uint32_t corrected = c0Valid ? 0U : 1U;

    uint32_t u1 = Golay24128::decode23127(c1 ^ makePN(u0));
    uint32_t correctedC1 = (Golay24128::encode23127(u1) >> 1U) ^ makePN(u0);
    corrected += Utils::countBits32(c1 ^ correctedC1);
    writeBits(bytes, 0U, 24U, Golay24128::encode24128(u0));
    writeBits(bytes, 24U, 23U, correctedC1);

    return corrected;
}

/* Regenerates both no-sync voice codewords in a Phase 2 4V/2V burst. */

uint32_t AMBE2FEC::regenerateBurst(uint8_t* burst, bool inbound, bool fourVoice) const
{
    assert(burst != nullptr);

    static const uint32_t inboundOffsets[4U] = {0U, 74U, 174U, 248U};
    static const uint32_t outboundOffsets[4U] = {2U, 76U, 176U, 250U};
    const uint32_t* offsets = inbound ? inboundOffsets : outboundOffsets;
    uint32_t corrected = 0U;

    uint32_t frameCount = fourVoice ? 4U : 2U;
    for (uint32_t frame = 0U; frame < frameCount; frame++) {
        uint8_t transmitted[9U] = {0U};
        uint8_t codeword[9U] = {0U};
        for (uint32_t bit = 0U; bit < 72U; bit++)
            writeBits(transmitted, bit, 1U, readBits(burst, offsets[frame] + bit, 1U));

        deinterleave(codeword, transmitted);
        corrected += regenerate(codeword);
        interleave(transmitted, codeword);
        for (uint32_t bit = 0U; bit < 72U; bit++)
            writeBits(burst, offsets[frame] + bit, 1U, readBits(transmitted, bit, 1U));
    }

    return corrected;
}

/* Encodes the prioritized half-rate parameter words. */

void AMBE2FEC::encode(uint8_t* bytes, uint16_t u0, uint16_t u1, uint16_t u2, uint16_t u3) const
{
    assert(bytes != nullptr);
    ::memset(bytes, 0x00U, 9U);

    writeBits(bytes, 0U, 24U, Golay24128::encode24128(u0));
    writeBits(bytes, 24U, 23U, (Golay24128::encode23127(u1) >> 1U) ^ makePN(u0));
    writeBits(bytes, 47U, 11U, u2);
    writeBits(bytes, 58U, 14U, u3);
}

/* Converts a deinterleaved codeword to the 36 transmitted dibits. */

void AMBE2FEC::interleave(uint8_t* frame, const uint8_t* codeword) const
{
    assert(frame != nullptr);
    assert(codeword != nullptr);
    ::memset(frame, 0x00U, 9U);

    for (uint32_t symbol = 0U; symbol < 36U; symbol++) {
        uint8_t bit1 = readWordBit(codeword, ANNEX_S[symbol][0U]);
        uint8_t bit0 = readWordBit(codeword, ANNEX_S[symbol][1U]);
        writeBits(frame, symbol * 2U, 2U, (bit1 << 1U) | bit0);
    }
}

/* Converts 36 transmitted dibits to a deinterleaved codeword. */

void AMBE2FEC::deinterleave(uint8_t* codeword, const uint8_t* frame) const
{
    assert(codeword != nullptr);
    assert(frame != nullptr);
    ::memset(codeword, 0x00U, 9U);

    for (uint32_t symbol = 0U; symbol < 36U; symbol++) {
        uint32_t dibit = readBits(frame, symbol * 2U, 2U);
        const Ambe2BitRef& ref1 = ANNEX_S[symbol][0U];
        const Ambe2BitRef& ref0 = ANNEX_S[symbol][1U];
        writeWordBit(codeword, ref1, static_cast<uint8_t>((dibit >> 1U) & 1U));
        writeWordBit(codeword, ref0, static_cast<uint8_t>(dibit & 1U));
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Returns the bit offset of the specified half-rate word. */

uint32_t AMBE2FEC::wordOffset(uint8_t word)
{
    static const uint32_t offsets[4U] = { 0U, 24U, 47U, 58U };
    return offsets[word];
}

/* Returns the bit width of the specified half-rate word. */

uint32_t AMBE2FEC::wordWidth(uint8_t word)
{
    static const uint32_t widths[4U] = { 24U, 23U, 11U, 14U };
    return widths[word];
}

/* Returns the bit reference of the specified half-rate word and bit. */

uint8_t AMBE2FEC::readWordBit(const uint8_t* codeword, const Ambe2BitRef& ref)
{
    uint32_t offset = wordOffset(ref.word) + wordWidth(ref.word) - 1U - ref.bit;
    return static_cast<uint8_t>((codeword[offset >> 3U] >> (7U - (offset & 7U))) & 1U);
}

/* Writes a bit to the specified half-rate word and bit. */

void AMBE2FEC::writeWordBit(uint8_t* codeword, const Ambe2BitRef& ref, uint8_t bit)
{
    uint32_t offset = wordOffset(ref.word) + wordWidth(ref.word) - 1U - ref.bit;
    uint8_t mask = static_cast<uint8_t>(1U << (7U - (offset & 7U)));
    if (bit != 0U)
        codeword[offset >> 3U] |= mask;
    else
        codeword[offset >> 3U] &= static_cast<uint8_t>(~mask);
}

/* Reads a bit field from a byte array. */

uint32_t AMBE2FEC::readBits(const uint8_t* bytes, uint32_t offset, uint32_t length)
{
    uint32_t value = 0U;

    for (uint32_t i = 0U; i < length; i++)
        value = (value << 1U) | ((bytes[(offset + i) >> 3U] >> (7U - ((offset + i) & 7U))) & 1U);

    return value;
}

/* Writes a bit field to a byte array. */

void AMBE2FEC::writeBits(uint8_t* bytes, uint32_t offset, uint32_t length, uint32_t value)
{
    for (uint32_t i = 0U; i < length; i++) {
        uint32_t bit = (value >> (length - 1U - i)) & 1U;
        uint8_t mask = static_cast<uint8_t>(1U << (7U - ((offset + i) & 7U)));
        bytes[(offset + i) >> 3U] = bit != 0U ? bytes[(offset + i) >> 3U] | mask :
            bytes[(offset + i) >> 3U] & static_cast<uint8_t>(~mask);
    }
}

/* Generates a pseudo-random number based on the input value. */

uint32_t AMBE2FEC::makePN(uint32_t u0)
{
    uint32_t pn = 0U;
    uint32_t state = 16U * u0;

    for (uint32_t i = 0U; i < 23U; i++) {
        state = (173U * state + 13849U) & 0xFFFFU;
        pn = (pn << 1U) | (state >= 32768U ? 1U : 0U);
    }

    return pn;
}
