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
*   Copyright (C) 2010,2014,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2016 Mathias Weyland, HB9FRV
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
#include "Defines.h"
#include "edac/AMBEFEC.h"
#include "edac/Golay24128.h"
#include "edac/Hamming.h"

using namespace edac;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the AMBEFEC class.
/// </summary>
AMBEFEC::AMBEFEC()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the AMBEFEC class.
/// </summary>
AMBEFEC::~AMBEFEC()
{
    /* stub */
}

/// <summary>
/// Regnerates the DMR AMBE FEC for the input bytes.
/// </summary>
/// <param name="bytes"></param>
/// <returns>Count of errors.</returns>
uint32_t AMBEFEC::regenerateDMR(uint8_t* bytes) const
{
    assert(bytes != NULL);

    uint32_t a1 = 0U, a2 = 0U, a3 = 0U;
    uint32_t b1 = 0U, b2 = 0U, b3 = 0U;
    uint32_t c1 = 0U, c2 = 0U, c3 = 0U;

    uint32_t MASK = 0x800000U;
    for (uint32_t i = 0U; i < 24U; i++) {
        uint32_t a1Pos = DMR_A_TABLE[i];
        uint32_t b1Pos = DMR_B_TABLE[i];
        uint32_t c1Pos = DMR_C_TABLE[i];

        uint32_t a2Pos = a1Pos + 72U;
        if (a2Pos >= 108U)
            a2Pos += 48U;
        uint32_t b2Pos = b1Pos + 72U;
        if (b2Pos >= 108U)
            b2Pos += 48U;
        uint32_t c2Pos = c1Pos + 72U;
        if (c2Pos >= 108U)
            c2Pos += 48U;

        uint32_t a3Pos = a1Pos + 192U;
        uint32_t b3Pos = b1Pos + 192U;
        uint32_t c3Pos = c1Pos + 192U;

        if (READ_BIT(bytes, a1Pos))
            a1 |= MASK;
        if (READ_BIT(bytes, a2Pos))
            a2 |= MASK;
        if (READ_BIT(bytes, a3Pos))
            a3 |= MASK;
        if (READ_BIT(bytes, b1Pos))
            b1 |= MASK;
        if (READ_BIT(bytes, b2Pos))
            b2 |= MASK;
        if (READ_BIT(bytes, b3Pos))
            b3 |= MASK;
        if (READ_BIT(bytes, c1Pos))
            c1 |= MASK;
        if (READ_BIT(bytes, c2Pos))
            c2 |= MASK;
        if (READ_BIT(bytes, c3Pos))
            c3 |= MASK;

        MASK >>= 1;
    }

    uint32_t errors = regenerate(a1, b1, c1, true);
    errors += regenerate(a2, b2, c2, true);
    errors += regenerate(a3, b3, c3, true);

    MASK = 0x800000U;
    for (uint32_t i = 0U; i < 24U; i++) {
        uint32_t a1Pos = DMR_A_TABLE[i];
        uint32_t b1Pos = DMR_B_TABLE[i];
        uint32_t c1Pos = DMR_C_TABLE[i];

        uint32_t a2Pos = a1Pos + 72U;
        if (a2Pos >= 108U)
            a2Pos += 48U;
        uint32_t b2Pos = b1Pos + 72U;
        if (b2Pos >= 108U)
            b2Pos += 48U;
        uint32_t c2Pos = c1Pos + 72U;
        if (c2Pos >= 108U)
            c2Pos += 48U;

        uint32_t a3Pos = a1Pos + 192U;
        uint32_t b3Pos = b1Pos + 192U;
        uint32_t c3Pos = c1Pos + 192U;

        WRITE_BIT(bytes, a1Pos, a1 & MASK);
        WRITE_BIT(bytes, a2Pos, a2 & MASK);
        WRITE_BIT(bytes, a3Pos, a3 & MASK);
        WRITE_BIT(bytes, b1Pos, b1 & MASK);
        WRITE_BIT(bytes, b2Pos, b2 & MASK);
        WRITE_BIT(bytes, b3Pos, b3 & MASK);
        WRITE_BIT(bytes, c1Pos, c1 & MASK);
        WRITE_BIT(bytes, c2Pos, c2 & MASK);
        WRITE_BIT(bytes, c3Pos, c3 & MASK);

        MASK >>= 1;
    }

    return errors;
}

/// <summary>
/// Returns the number of errors on the DMR BER input bytes.
/// </summary>
/// <param name="bytes"></param>
/// <returns>Count of errors.</returns>
uint32_t AMBEFEC::measureDMRBER(const uint8_t* bytes) const
{
    assert(bytes != NULL);

    uint32_t a1 = 0U, a2 = 0U, a3 = 0U;
    uint32_t b1 = 0U, b2 = 0U, b3 = 0U;
    uint32_t c1 = 0U, c2 = 0U, c3 = 0U;

    uint32_t MASK = 0x800000U;
    for (uint32_t i = 0U; i < 24U; i++) {
        uint32_t a1Pos = DMR_A_TABLE[i];
        uint32_t b1Pos = DMR_B_TABLE[i];
        uint32_t c1Pos = DMR_C_TABLE[i];

        uint32_t a2Pos = a1Pos + 72U;
        if (a2Pos >= 108U)
            a2Pos += 48U;
        uint32_t b2Pos = b1Pos + 72U;
        if (b2Pos >= 108U)
            b2Pos += 48U;
        uint32_t c2Pos = c1Pos + 72U;
        if (c2Pos >= 108U)
            c2Pos += 48U;

        uint32_t a3Pos = a1Pos + 192U;
        uint32_t b3Pos = b1Pos + 192U;
        uint32_t c3Pos = c1Pos + 192U;

        if (READ_BIT(bytes, a1Pos))
            a1 |= MASK;
        if (READ_BIT(bytes, a2Pos))
            a2 |= MASK;
        if (READ_BIT(bytes, a3Pos))
            a3 |= MASK;
        if (READ_BIT(bytes, b1Pos))
            b1 |= MASK;
        if (READ_BIT(bytes, b2Pos))
            b2 |= MASK;
        if (READ_BIT(bytes, b3Pos))
            b3 |= MASK;
        if (READ_BIT(bytes, c1Pos))
            c1 |= MASK;
        if (READ_BIT(bytes, c2Pos))
            c2 |= MASK;
        if (READ_BIT(bytes, c3Pos))
            c3 |= MASK;

        MASK >>= 1;
    }

    uint32_t errors = regenerate(a1, b1, c1, true);
    errors += regenerate(a2, b2, c2, true);
    errors += regenerate(a3, b3, c3, true);

    return errors;
}

/// <summary>
/// Regenerates the P25 IMBE FEC for the input bytes.
/// </summary>
/// <param name="bytes"></param>
/// <returns>Count of errors.</returns>
uint32_t AMBEFEC::regenerateIMBE(uint8_t* bytes) const
{
    assert(bytes != NULL);

    bool orig[144U];
    bool temp[144U];

    // De-interleave
    for (uint32_t i = 0U; i < 144U; i++) {
        uint32_t n = IMBE_INTERLEAVE[i];
        orig[i] = temp[i] = READ_BIT(bytes, n);
    }

    // now ..

    // 12 voice bits     0
    // 11 golay bits     12
    //
    // 12 voice bits     23
    // 11 golay bits     35
    //
    // 12 voice bits     46
    // 11 golay bits     58
    //
    // 12 voice bits     69
    // 11 golay bits     81
    //
    // 11 voice bits     92
    //  4 hamming bits   103
    //
    // 11 voice bits     107
    //  4 hamming bits   118
    //
    // 11 voice bits     122
    //  4 hamming bits   133
    //
    //  7 voice bits     137

    // Process the c0 section first to allow the de-whitening to be accurate

    // Check/Fix FEC
    bool* bit = temp;

    // c0
    uint32_t g1 = 0U;
    for (uint32_t i = 0U; i < 23U; i++)
        g1 = (g1 << 1) | (bit[i] ? 0x01U : 0x00U);
    uint32_t c0data = Golay24128::decode23127(g1);
    uint32_t g2 = Golay24128::encode23127(c0data);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    bool prn[114U];

    // Create the whitening vector and save it for future use
    uint32_t p = 16U * c0data;
    for (uint32_t i = 0U; i < 114U; i++) {
        p = (173U * p + 13849U) % 65536U;
        prn[i] = p >= 32768U;
    }

    // De-whiten some bits
    for (uint32_t i = 0U; i < 114U; i++)
        temp[i + 23U] ^= prn[i];

    // c1
    g1 = 0U;
    for (uint32_t i = 0U; i < 23U; i++)
        g1 = (g1 << 1) | (bit[i] ? 0x01U : 0x00U);
    uint32_t c1data = Golay24128::decode23127(g1);
    g2 = Golay24128::encode23127(c1data);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c2
    g1 = 0;
    for (uint32_t i = 0U; i < 23U; i++)
        g1 = (g1 << 1) | (bit[i] ? 0x01U : 0x00U);
    uint32_t c2data = Golay24128::decode23127(g1);
    g2 = Golay24128::encode23127(c2data);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c3
    g1 = 0U;
    for (uint32_t i = 0U; i < 23U; i++)
        g1 = (g1 << 1) | (bit[i] ? 0x01U : 0x00U);
    uint32_t c3data = Golay24128::decode23127(g1);
    g2 = Golay24128::encode23127(c3data);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c4
    Hamming::decode15113_1(bit);
    bit += 15U;

    // c5
    Hamming::decode15113_1(bit);
    bit += 15U;

    // c6
    Hamming::decode15113_1(bit);

    // Whiten some bits
    for (uint32_t i = 0U; i < 114U; i++)
        temp[i + 23U] ^= prn[i];

    uint32_t errors = 0U;
    for (uint32_t i = 0U; i < 144U; i++) {
        if (orig[i] != temp[i])
            errors++;
    }

    // Interleave
    for (uint32_t i = 0U; i < 144U; i++) {
        uint32_t n = IMBE_INTERLEAVE[i];
        WRITE_BIT(bytes, n, temp[i]);
    }

    return errors;
}

/// <summary>
/// Returns the number of errors on the P25 BER input bytes.
/// </summary>
/// <param name="bytes"></param>
/// <returns>Count of errors.</returns>
uint32_t AMBEFEC::measureP25BER(const uint8_t* bytes) const
{
    assert(bytes != NULL);

    bool orig[144U];
    bool temp[144U];

    // De-interleave
    for (uint32_t i = 0U; i < 144U; i++) {
        uint32_t n = IMBE_INTERLEAVE[i];
        orig[i] = temp[i] = READ_BIT(bytes, n);
    }

    // now ..

    // 12 voice bits     0
    // 11 golay bits     12
    //
    // 12 voice bits     23
    // 11 golay bits     35
    //
    // 12 voice bits     46
    // 11 golay bits     58
    //
    // 12 voice bits     69
    // 11 golay bits     81
    //
    // 11 voice bits     92
    //  4 hamming bits   103
    //
    // 11 voice bits     107
    //  4 hamming bits   118
    //
    // 11 voice bits     122
    //  4 hamming bits   133
    //
    //  7 voice bits     137

    // Process the c0 section first to allow the de-whitening to be accurate

    // Check/Fix FEC
    bool* bit = temp;

    // c0
    uint32_t g1 = 0U;
    for (uint32_t i = 0U; i < 23U; i++)
        g1 = (g1 << 1) | (bit[i] ? 0x01U : 0x00U);
    uint32_t c0data = Golay24128::decode23127(g1);
    uint32_t g2 = Golay24128::encode23127(c0data);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    bool prn[114U];

    // Create the whitening vector and save it for future use
    uint32_t p = 16U * c0data;
    for (uint32_t i = 0U; i < 114U; i++) {
        p = (173U * p + 13849U) % 65536U;
        prn[i] = p >= 32768U;
    }

    // De-whiten some bits
    for (uint32_t i = 0U; i < 114U; i++)
        temp[i + 23U] ^= prn[i];

    // c1
    g1 = 0U;
    for (uint32_t i = 0U; i < 23U; i++)
        g1 = (g1 << 1) | (bit[i] ? 0x01U : 0x00U);
    uint32_t c1data = Golay24128::decode23127(g1);
    g2 = Golay24128::encode23127(c1data);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c2
    g1 = 0;
    for (uint32_t i = 0U; i < 23U; i++)
        g1 = (g1 << 1) | (bit[i] ? 0x01U : 0x00U);
    uint32_t c2data = Golay24128::decode23127(g1);
    g2 = Golay24128::encode23127(c2data);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c3
    g1 = 0U;
    for (uint32_t i = 0U; i < 23U; i++)
        g1 = (g1 << 1) | (bit[i] ? 0x01U : 0x00U);
    uint32_t c3data = Golay24128::decode23127(g1);
    g2 = Golay24128::encode23127(c3data);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c4
    Hamming::decode15113_1(bit);
    bit += 15U;

    // c5
    Hamming::decode15113_1(bit);
    bit += 15U;

    // c6
    Hamming::decode15113_1(bit);

    // Whiten some bits
    for (uint32_t i = 0U; i < 114U; i++)
        temp[i + 23U] ^= prn[i];

    uint32_t errors = 0U;
    for (uint32_t i = 0U; i < 144U; i++) {
        if (orig[i] != temp[i])
            errors++;
    }

    return errors;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
///
/// </summary>
/// <param name="a"></param>
/// <param name="b"></param>
/// <param name="c"></param>
/// <param name="b23"></param>
/// <returns></returns>
uint32_t AMBEFEC::regenerate(uint32_t& a, uint32_t& b, uint32_t& c, bool b23) const
{
    uint32_t old_a = a;
    uint32_t old_b = b;

    // For the b23 bypass
    bool b24 = (b & 0x01U) == 0x01U;

    uint32_t data = Golay24128::decode24128(a);

    uint32_t new_a = Golay24128::encode24128(data);

    // The PRNG
    uint32_t p = PRNG_TABLE[data];

    b ^= p;

    uint32_t datb = Golay24128::decode24128(b);

    uint32_t new_b = Golay24128::encode24128(datb);

    new_b ^= p;

    if (b23) {
        new_b &= 0xFFFFFEU;
        new_b |= b24 ? 0x01U : 0x00U;
    }

    uint32_t errsA = 0U, errsB = 0U;

    uint32_t v = new_a ^ old_a;
    while (v != 0U) {
        v &= v - 1U;
        errsA++;
    }

    v = new_b ^ old_b;
    while (v != 0U) {
        v &= v - 1U;
        errsB++;
    }

    if (b23) {
        if (errsA >= 4U || ((errsA + errsB) >= 6U && errsA >= 2U)) {
            a = 0xF00292U;
            b = 0x0E0B20U;
            c = 0x000000U;
        }
    }

    a = new_a;
    b = new_b;

    return errsA + errsB;
}
