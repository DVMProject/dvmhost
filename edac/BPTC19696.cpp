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
*   Copyright (C) 2012 by Ian Wraith
*   Copyright (C) 2015 by Jonathan Naylor G4KLX
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
#include "edac/BPTC19696.h"
#include "edac/Hamming.h"
#include "Utils.h"

using namespace edac;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the BPTC19696 class.
/// </summary>
BPTC19696::BPTC19696() :
    m_rawData(NULL),
    m_deInterData(NULL)
{
    m_rawData = new bool[196];
    m_deInterData = new bool[196];
}

/// <summary>
/// Finalizes a instance of the BPTC19696 class.
/// </summary>
BPTC19696::~BPTC19696()
{
    delete[] m_rawData;
    delete[] m_deInterData;
}

/// <summary>
/// Decode BPTC (196,96) FEC.
/// </summary>
/// <param name="in">Input data to decode.</param>
/// <param name="out">Decoded data.</param>
void BPTC19696::decode(const uint8_t* in, uint8_t* out)
{
    assert(in != NULL);
    assert(out != NULL);

    //  Get the raw binary
    decodeExtractBinary(in);

    // Deinterleave
    decodeDeInterleave();

    // Error check
    decodeErrorCheck();

    // Extract Data
    decodeExtractData(out);
}

/// <summary>
/// Encode BPTC (196,96) FEC.
/// </summary>
/// <param name="in">Input data to encode.</param>
/// <param name="out">Encoded data.</param>
void BPTC19696::encode(const uint8_t* in, uint8_t* out)
{
    assert(in != NULL);
    assert(out != NULL);

    // Extract Data
    encodeExtractData(in);

    // Error check
    encodeErrorCheck();

    // Deinterleave
    encodeInterleave();

    //  Get the raw binary
    encodeExtractBinary(out);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
///
/// </summary>
/// <param name="in"></param>
void BPTC19696::decodeExtractBinary(const uint8_t* in)
{
    // First block
    Utils::byteToBitsBE(in[0U], m_rawData + 0U);
    Utils::byteToBitsBE(in[1U], m_rawData + 8U);
    Utils::byteToBitsBE(in[2U], m_rawData + 16U);
    Utils::byteToBitsBE(in[3U], m_rawData + 24U);
    Utils::byteToBitsBE(in[4U], m_rawData + 32U);
    Utils::byteToBitsBE(in[5U], m_rawData + 40U);
    Utils::byteToBitsBE(in[6U], m_rawData + 48U);
    Utils::byteToBitsBE(in[7U], m_rawData + 56U);
    Utils::byteToBitsBE(in[8U], m_rawData + 64U);
    Utils::byteToBitsBE(in[9U], m_rawData + 72U);
    Utils::byteToBitsBE(in[10U], m_rawData + 80U);
    Utils::byteToBitsBE(in[11U], m_rawData + 88U);
    Utils::byteToBitsBE(in[12U], m_rawData + 96U);

    // Handle the two bits
    bool bits[8U];
    Utils::byteToBitsBE(in[20U], bits);
    m_rawData[98U] = bits[6U];
    m_rawData[99U] = bits[7U];

    // Second block
    Utils::byteToBitsBE(in[21U], m_rawData + 100U);
    Utils::byteToBitsBE(in[22U], m_rawData + 108U);
    Utils::byteToBitsBE(in[23U], m_rawData + 116U);
    Utils::byteToBitsBE(in[24U], m_rawData + 124U);
    Utils::byteToBitsBE(in[25U], m_rawData + 132U);
    Utils::byteToBitsBE(in[26U], m_rawData + 140U);
    Utils::byteToBitsBE(in[27U], m_rawData + 148U);
    Utils::byteToBitsBE(in[28U], m_rawData + 156U);
    Utils::byteToBitsBE(in[29U], m_rawData + 164U);
    Utils::byteToBitsBE(in[30U], m_rawData + 172U);
    Utils::byteToBitsBE(in[31U], m_rawData + 180U);
    Utils::byteToBitsBE(in[32U], m_rawData + 188U);
}

/// <summary>
///
/// </summary>
void BPTC19696::decodeDeInterleave()
{
    for (uint32_t i = 0U; i < 196U; i++)
        m_deInterData[i] = false;

    // The first bit is R(3) which is not used so can be ignored
    for (uint32_t a = 0U; a < 196U; a++) {
        // Calculate the interleave sequence
        uint32_t interleaveSequence = (a * 181U) % 196U;
        // Shuffle the data
        m_deInterData[a] = m_rawData[interleaveSequence];
    }
}

/// <summary>
///
/// </summary>
void BPTC19696::decodeErrorCheck()
{
    bool fixing;
    uint32_t count = 0U;
    do {
        fixing = false;

        // Run through each of the 15 columns
        bool col[13U];
        for (uint32_t c = 0U; c < 15U; c++) {
            uint32_t pos = c + 1U;
            for (uint32_t a = 0U; a < 13U; a++) {
                col[a] = m_deInterData[pos];
                pos = pos + 15U;
            }

            if (Hamming::decode1393(col)) {
                uint32_t pos = c + 1U;
                for (uint32_t a = 0U; a < 13U; a++) {
                    m_deInterData[pos] = col[a];
                    pos = pos + 15U;
                }

                fixing = true;
            }
        }

        // Run through each of the 9 rows containing data
        for (uint32_t r = 0U; r < 9U; r++) {
            uint32_t pos = (r * 15U) + 1U;
            if (Hamming::decode15113_2(m_deInterData + pos))
                fixing = true;
        }

        count++;
    } while (fixing && count < 5U);
}

/// <summary>
///
/// </summary>
/// <param name="data"></param>
void BPTC19696::decodeExtractData(uint8_t* data) const
{
    bool bData[96U];
    uint32_t pos = 0U;
    for (uint32_t a = 4U; a <= 11U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 16U; a <= 26U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 31U; a <= 41U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 46U; a <= 56U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 61U; a <= 71U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 76U; a <= 86U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 91U; a <= 101U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 106U; a <= 116U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 121U; a <= 131U; a++, pos++)
        bData[pos] = m_deInterData[a];

    Utils::bitsToByteBE(bData + 0U, data[0U]);
    Utils::bitsToByteBE(bData + 8U, data[1U]);
    Utils::bitsToByteBE(bData + 16U, data[2U]);
    Utils::bitsToByteBE(bData + 24U, data[3U]);
    Utils::bitsToByteBE(bData + 32U, data[4U]);
    Utils::bitsToByteBE(bData + 40U, data[5U]);
    Utils::bitsToByteBE(bData + 48U, data[6U]);
    Utils::bitsToByteBE(bData + 56U, data[7U]);
    Utils::bitsToByteBE(bData + 64U, data[8U]);
    Utils::bitsToByteBE(bData + 72U, data[9U]);
    Utils::bitsToByteBE(bData + 80U, data[10U]);
    Utils::bitsToByteBE(bData + 88U, data[11U]);
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
void BPTC19696::encodeExtractData(const uint8_t* in) const
{
    bool bData[96U];
    Utils::byteToBitsBE(in[0U], bData + 0U);
    Utils::byteToBitsBE(in[1U], bData + 8U);
    Utils::byteToBitsBE(in[2U], bData + 16U);
    Utils::byteToBitsBE(in[3U], bData + 24U);
    Utils::byteToBitsBE(in[4U], bData + 32U);
    Utils::byteToBitsBE(in[5U], bData + 40U);
    Utils::byteToBitsBE(in[6U], bData + 48U);
    Utils::byteToBitsBE(in[7U], bData + 56U);
    Utils::byteToBitsBE(in[8U], bData + 64U);
    Utils::byteToBitsBE(in[9U], bData + 72U);
    Utils::byteToBitsBE(in[10U], bData + 80U);
    Utils::byteToBitsBE(in[11U], bData + 88U);

    for (uint32_t i = 0U; i < 196U; i++)
        m_deInterData[i] = false;

    uint32_t pos = 0U;
    for (uint32_t a = 4U; a <= 11U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 16U; a <= 26U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 31U; a <= 41U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 46U; a <= 56U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 61U; a <= 71U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 76U; a <= 86U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 91U; a <= 101U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 106U; a <= 116U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 121U; a <= 131U; a++, pos++)
        m_deInterData[a] = bData[pos];
}

/// <summary>
///
/// </summary>
void BPTC19696::encodeErrorCheck()
{
    // Run through each of the 9 rows containing data
    for (uint32_t r = 0U; r < 9U; r++) {
        uint32_t pos = (r * 15U) + 1U;
        Hamming::encode15113_2(m_deInterData + pos);
    }

    // Run through each of the 15 columns
    bool col[13U];
    for (uint32_t c = 0U; c < 15U; c++) {
        uint32_t pos = c + 1U;
        for (uint32_t a = 0U; a < 13U; a++) {
            col[a] = m_deInterData[pos];
            pos = pos + 15U;
        }

        Hamming::encode1393(col);

        pos = c + 1U;
        for (uint32_t a = 0U; a < 13U; a++) {
            m_deInterData[pos] = col[a];
            pos = pos + 15U;
        }
    }
}

/// <summary>
///
/// </summary>
void BPTC19696::encodeInterleave()
{
    for (uint32_t i = 0U; i < 196U; i++)
        m_rawData[i] = false;

    // The first bit is R(3) which is not used so can be ignored
    for (uint32_t a = 0U; a < 196U; a++) {
        // Calculate the interleave sequence
        uint32_t interleaveSequence = (a * 181U) % 196U;
        // Unshuffle the data
        m_rawData[interleaveSequence] = m_deInterData[a];
    }
}

/// <summary>
///
/// </summary>
/// <param name="data"></param>
void BPTC19696::encodeExtractBinary(uint8_t* data)
{
    // First block
    Utils::bitsToByteBE(m_rawData + 0U, data[0U]);
    Utils::bitsToByteBE(m_rawData + 8U, data[1U]);
    Utils::bitsToByteBE(m_rawData + 16U, data[2U]);
    Utils::bitsToByteBE(m_rawData + 24U, data[3U]);
    Utils::bitsToByteBE(m_rawData + 32U, data[4U]);
    Utils::bitsToByteBE(m_rawData + 40U, data[5U]);
    Utils::bitsToByteBE(m_rawData + 48U, data[6U]);
    Utils::bitsToByteBE(m_rawData + 56U, data[7U]);
    Utils::bitsToByteBE(m_rawData + 64U, data[8U]);
    Utils::bitsToByteBE(m_rawData + 72U, data[9U]);
    Utils::bitsToByteBE(m_rawData + 80U, data[10U]);
    Utils::bitsToByteBE(m_rawData + 88U, data[11U]);

    // Handle the two bits
    uint8_t byte;
    Utils::bitsToByteBE(m_rawData + 96U, byte);
    data[12U] = (data[12U] & 0x3FU) | ((byte >> 0) & 0xC0U);
    data[20U] = (data[20U] & 0xFCU) | ((byte >> 4) & 0x03U);

    // Second block
    Utils::bitsToByteBE(m_rawData + 100U, data[21U]);
    Utils::bitsToByteBE(m_rawData + 108U, data[22U]);
    Utils::bitsToByteBE(m_rawData + 116U, data[23U]);
    Utils::bitsToByteBE(m_rawData + 124U, data[24U]);
    Utils::bitsToByteBE(m_rawData + 132U, data[25U]);
    Utils::bitsToByteBE(m_rawData + 140U, data[26U]);
    Utils::bitsToByteBE(m_rawData + 148U, data[27U]);
    Utils::bitsToByteBE(m_rawData + 156U, data[28U]);
    Utils::bitsToByteBE(m_rawData + 164U, data[29U]);
    Utils::bitsToByteBE(m_rawData + 172U, data[30U]);
    Utils::bitsToByteBE(m_rawData + 180U, data[31U]);
    Utils::bitsToByteBE(m_rawData + 188U, data[32U]);
}
