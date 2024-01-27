/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2018 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "nxdn/Audio.h"
#include "common/edac/Golay24128.h"
#include "common/Utils.h"

using namespace nxdn;
using namespace edac;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Audio class.
/// </summary>
Audio::Audio() = default;

/// <summary>
/// Finalizes a instance of the Audio class.
/// </summary>
Audio::~Audio() = default;

/// <summary>
/// Decode a NXDN AMBE audio frame.
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
void Audio::decode(const uint8_t* in, uint8_t* out) const
{
    assert(in != nullptr);
    assert(out != nullptr);

    decode(in + 0U, out, 0U);
    decode(in + 9U, out, 49U);
    }

/// <summary>
/// Encode a NXDN AMBE audio frame.
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
void Audio::encode(const uint8_t* in, uint8_t* out) const
{
    assert(in != nullptr);
    assert(out != nullptr);

    encode(in, out + 0U, 0U);
    encode(in, out + 9U, 49U);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="offset"></param>
void Audio::decode(const uint8_t* in, uint8_t* out, uint32_t offset) const
{
    assert(in != nullptr);
    assert(out != nullptr);

    uint32_t a = 0U;
    uint32_t MASK = 0x800000U;
    for (uint32_t i = 0U; i < 24U; i++, MASK >>= 1) {
        uint32_t aPos = A_TABLE[i];
        if (READ_BIT(in, aPos))
            a |= MASK;
    }

    uint32_t b = 0U;
    MASK = 0x400000U;
    for (uint32_t i = 0U; i < 23U; i++, MASK >>= 1) {
        uint32_t bPos = B_TABLE[i];
        if (READ_BIT(in, bPos))
            b |= MASK;
    }

    uint32_t c = 0U;
    MASK = 0x1000000U;
    for (uint32_t i = 0U; i < 25U; i++, MASK >>= 1) {
        uint32_t cPos = C_TABLE[i];
        if (READ_BIT(in, cPos))
            c |= MASK;
    }

    a >>= 12;

    // The PRNG
    b ^= (PRNG_TABLE[a] >> 1);
    b >>= 11;

    MASK = 0x000800U;
    for (uint32_t i = 0U; i < 12U; i++, MASK >>= 1) {
        uint32_t aPos = i + offset + 0U;
        uint32_t bPos = i + offset + 12U;
        WRITE_BIT(out, aPos, a & MASK);
        WRITE_BIT(out, bPos, b & MASK);
    }

    MASK = 0x1000000U;
    for (uint32_t i = 0U; i < 25U; i++, MASK >>= 1) {
        uint32_t cPos = i + offset + 24U;
        WRITE_BIT(out, cPos, c & MASK);
    }
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="offset"></param>
void Audio::encode(const uint8_t* in, uint8_t* out, uint32_t offset) const
{
    assert(in != nullptr);
    assert(out != nullptr);

    uint32_t aOrig = 0U;
    uint32_t bOrig = 0U;
    uint32_t cOrig = 0U;

    uint32_t MASK = 0x000800U;
    for (uint32_t i = 0U; i < 12U; i++, MASK >>= 1) {
        uint32_t n1 = i + offset + 0U;
        uint32_t n2 = i + offset + 12U;
        if (READ_BIT(in, n1))
            aOrig |= MASK;
        if (READ_BIT(in, n2))
            bOrig |= MASK;
    }

    MASK = 0x1000000U;
    for (uint32_t i = 0U; i < 25U; i++, MASK >>= 1) {
        uint32_t n = i + offset + 24U;
        if (READ_BIT(in, n))
            cOrig |= MASK;
    }

    uint32_t a = Golay24128::encode24128(aOrig);

    // The PRNG
    uint32_t p = PRNG_TABLE[aOrig] >> 1;

    uint32_t b = Golay24128::encode23127(bOrig) >> 1;
    b ^= p;

    MASK = 0x800000U;
    for (uint32_t i = 0U; i < 24U; i++, MASK >>= 1) {
        uint32_t aPos = A_TABLE[i];
        WRITE_BIT(out, aPos, a & MASK);
    }

    MASK = 0x400000U;
    for (uint32_t i = 0U; i < 23U; i++, MASK >>= 1) {
        uint32_t bPos = B_TABLE[i];
        WRITE_BIT(out, bPos, b & MASK);
    }

    MASK = 0x1000000U;
    for (uint32_t i = 0U; i < 25U; i++, MASK >>= 1) {
        uint32_t cPos = C_TABLE[i];
        WRITE_BIT(out, cPos, cOrig & MASK);
    }
}
