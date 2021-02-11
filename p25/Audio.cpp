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
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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
#include "p25/Audio.h"
#include "p25/P25Utils.h"
#include "edac/Golay24128.h"
#include "edac/Hamming.h"

using namespace p25;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the Audio class.
/// </summary>
Audio::Audio() :
    m_fec()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Audio class.
/// </summary>
Audio::~Audio()
{
    /* stub */
}

/// <summary>
/// Process P25 IMBE audio data.
/// </summary>
/// <param name="data"></param>
/// <returns>Number of errors corrected.</returns>
uint32_t Audio::process(uint8_t* data)
{
    assert(data != NULL);

    uint32_t errs = 0U;

    uint8_t imbe[18U];

    P25Utils::decode(data, imbe, 114U, 262U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 114U, 262U);

    P25Utils::decode(data, imbe, 262U, 410U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 262U, 410U);

    P25Utils::decode(data, imbe, 452U, 600U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 452U, 600U);

    P25Utils::decode(data, imbe, 640U, 788U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 640U, 788U);

    P25Utils::decode(data, imbe, 830U, 978U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 830U, 978U);

    P25Utils::decode(data, imbe, 1020U, 1168U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 1020U, 1168U);

    P25Utils::decode(data, imbe, 1208U, 1356U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 1208U, 1356U);

    P25Utils::decode(data, imbe, 1398U, 1546U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 1398U, 1546U);

    P25Utils::decode(data, imbe, 1578U, 1726U);
    errs += m_fec.regenerateIMBE(imbe);
    P25Utils::encode(imbe, data, 1578U, 1726U);

    return errs;
}

/// <summary>
/// Decode a P25 IMBE audio frame.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
/// <param name="n"></param>
void Audio::decode(const uint8_t* data, uint8_t* imbe, uint32_t n)
{
    assert(data != NULL);
    assert(imbe != NULL);

    uint8_t temp[18U];

    switch (n) {
        case 0U:
            P25Utils::decode(data, temp, 114U, 262U);
            break;
        case 1U:
            P25Utils::decode(data, temp, 262U, 410U);
            break;
        case 2U:
            P25Utils::decode(data, temp, 452U, 600U);
            break;
        case 3U:
            P25Utils::decode(data, temp, 640U, 788U);
            break;
        case 4U:
            P25Utils::decode(data, temp, 830U, 978U);
            break;
        case 5U:
            P25Utils::decode(data, temp, 1020U, 1168U);
            break;
        case 6U:
            P25Utils::decode(data, temp, 1208U, 1356U);
            break;
        case 7U:
            P25Utils::decode(data, temp, 1398U, 1546U);
            break;
        case 8U:
            P25Utils::decode(data, temp, 1578U, 1726U);
            break;
        default:
            return;
    }

    // Utils::dump(2U, "Audio::decode()", temp, 18U);

    bool bit[144U];

    // De-interleave
    for (uint32_t i = 0U; i < 144U; i++) {
        uint32_t n = edac::IMBE_INTERLEAVE[i];
        bit[i] = READ_BIT(temp, n);
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

    // c0
    uint32_t c0data = 0U;
    for (uint32_t i = 0U; i < 12U; i++)
        c0data = (c0data << 1) | (bit[i] ? 0x01U : 0x00U);

    bool prn[114U];

    // Create the whitening vector and save it for future use
    uint32_t p = 16U * c0data;
    for (uint32_t i = 0U; i < 114U; i++) {
        p = (173U * p + 13849U) % 65536U;
        prn[i] = p >= 32768U;
    }

    // De-whiten some bits
    for (uint32_t i = 0U; i < 114U; i++)
        bit[i + 23U] ^= prn[i];

    uint32_t offset = 0U;
    for (uint32_t i = 0U; i < 12U; i++, offset++)
        WRITE_BIT(imbe, offset, bit[i + 0U]);
    for (uint32_t i = 0U; i < 12U; i++, offset++)
        WRITE_BIT(imbe, offset, bit[i + 23U]);
    for (uint32_t i = 0U; i < 12U; i++, offset++)
        WRITE_BIT(imbe, offset, bit[i + 46U]);
    for (uint32_t i = 0U; i < 12U; i++, offset++)
        WRITE_BIT(imbe, offset, bit[i + 69U]);
    for (uint32_t i = 0U; i < 11U; i++, offset++)
        WRITE_BIT(imbe, offset, bit[i + 92U]);
    for (uint32_t i = 0U; i < 11U; i++, offset++)
        WRITE_BIT(imbe, offset, bit[i + 107U]);
    for (uint32_t i = 0U; i < 11U; i++, offset++)
        WRITE_BIT(imbe, offset, bit[i + 122U]);
    for (uint32_t i = 0U; i < 7U; i++, offset++)
        WRITE_BIT(imbe, offset, bit[i + 137U]);
}

/// <summary>
/// Encode a P25 IMBE audio frame.
/// </summary>
/// <param name="data"></param>
/// <param name="imbe"></param>
/// <param name="n"></param>
void Audio::encode(uint8_t* data, const uint8_t* imbe, uint32_t n)
{
    assert(data != NULL);
    assert(imbe != NULL);

    bool bTemp[144U];
    bool* bit = bTemp;

    // c0
    uint32_t c0 = 0U;
    for (uint32_t i = 0U; i < 12U; i++) {
        bool b = READ_BIT(imbe, i);
        c0 = (c0 << 1) | (b ? 0x01U : 0x00U);
    }
    uint32_t g2 = edac::Golay24128::encode23127(c0);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c1
    uint32_t c1 = 0U;
    for (uint32_t i = 12U; i < 24U; i++) {
        bool b = READ_BIT(imbe, i);
        c1 = (c1 << 1) | (b ? 0x01U : 0x00U);
    }
    g2 = edac::Golay24128::encode23127(c1);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c2
    uint32_t c2 = 0;
    for (uint32_t i = 24U; i < 36U; i++) {
        bool b = READ_BIT(imbe, i);
        c2 = (c2 << 1) | (b ? 0x01U : 0x00U);
    }
    g2 = edac::Golay24128::encode23127(c2);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c3
    uint32_t c3 = 0U;
    for (uint32_t i = 36U; i < 48U; i++) {
        bool b = READ_BIT(imbe, i);
        c3 = (c3 << 1) | (b ? 0x01U : 0x00U);
    }
    g2 = edac::Golay24128::encode23127(c3);
    for (int i = 23; i >= 0; i--) {
        bit[i] = (g2 & 0x01U) == 0x01U;
        g2 >>= 1;
    }
    bit += 23U;

    // c4
    for (uint32_t i = 0U; i < 11U; i++)
        bit[i] = READ_BIT(imbe, i + 48U);
    edac::Hamming::encode15113_1(bit);
    bit += 15U;

    // c5
    for (uint32_t i = 0U; i < 11U; i++)
        bit[i] = READ_BIT(imbe, i + 59U);
    edac::Hamming::encode15113_1(bit);
    bit += 15U;

    // c6
    for (uint32_t i = 0U; i < 11U; i++)
        bit[i] = READ_BIT(imbe, i + 70U);
    edac::Hamming::encode15113_1(bit);
    bit += 15U;

    // c7
    for (uint32_t i = 0U; i < 7U; i++)
        bit[i] = READ_BIT(imbe, i + 81U);

    bool prn[114U];

    // Create the whitening vector and save it for future use
    uint32_t p = 16U * c0;
    for (uint32_t i = 0U; i < 114U; i++) {
        p = (173U * p + 13849U) % 65536U;
        prn[i] = p >= 32768U;
    }

    // Whiten some bits
    for (uint32_t i = 0U; i < 114U; i++)
        bTemp[i + 23U] ^= prn[i];

    uint8_t temp[18U];

    // Interleave
    for (uint32_t i = 0U; i < 144U; i++) {
        uint32_t n = edac::IMBE_INTERLEAVE[i];
        WRITE_BIT(temp, n, bTemp[i]);
    }

    // Utils::dump(2U, "Audio::encode()", temp, 18U);

    switch (n) {
        case 0U:
            P25Utils::encode(temp, data, 114U, 262U);
            break;
        case 1U:
            P25Utils::encode(temp, data, 262U, 410U);
            break;
        case 2U:
            P25Utils::encode(temp, data, 452U, 600U);
            break;
        case 3U:
            P25Utils::encode(temp, data, 640U, 788U);
            break;
        case 4U:
            P25Utils::encode(temp, data, 830U, 978U);
            break;
        case 5U:
            P25Utils::encode(temp, data, 1020U, 1168U);
            break;
        case 6U:
            P25Utils::encode(temp, data, 1208U, 1356U);
            break;
        case 7U:
            P25Utils::encode(temp, data, 1398U, 1546U);
            break;
        case 8U:
            P25Utils::encode(temp, data, 1578U, 1726U);
            break;
        default:
            return;
    }
}
