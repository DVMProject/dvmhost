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
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
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
#include "dmr/lc/ShortLC.h"
#include "edac/Hamming.h"
#include "Utils.h"

using namespace dmr::lc;
using namespace dmr;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the ShortLC class.
/// </summary>
ShortLC::ShortLC() :
    m_rawData(NULL),
    m_deInterData(NULL)
{
    m_rawData = new bool[72U];
    m_deInterData = new bool[68U];
}

/// <summary>
/// Finalizes a instance of the ShortLC class.
/// </summary>
ShortLC::~ShortLC()
{
    delete[] m_rawData;
    delete[] m_deInterData;
}

/// <summary>
/// Decode DMR short-link control data.
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <returns></returns>
bool ShortLC::decode(const uint8_t* in, uint8_t* out)
{
    assert(in != NULL);
    assert(out != NULL);

    //  Get the raw binary
    decodeExtractBinary(in);

    // Deinterleave
    decodeDeInterleave();

    // Error check
    bool ret = decodeErrorCheck();
    if (!ret)
        return false;

    // Extract Data
    decodeExtractData(out);

    return true;
}

/// <summary>
/// Encode DMR short-link control data.
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
void ShortLC::encode(const uint8_t* in, uint8_t* out)
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
//   Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
///
/// </summary>
/// <param name="in"></param>
void ShortLC::decodeExtractBinary(const uint8_t* in)
{
    assert(in != NULL);

    Utils::byteToBitsBE(in[0U], m_rawData + 0U);
    Utils::byteToBitsBE(in[1U], m_rawData + 8U);
    Utils::byteToBitsBE(in[2U], m_rawData + 16U);
    Utils::byteToBitsBE(in[3U], m_rawData + 24U);
    Utils::byteToBitsBE(in[4U], m_rawData + 32U);
    Utils::byteToBitsBE(in[5U], m_rawData + 40U);
    Utils::byteToBitsBE(in[6U], m_rawData + 48U);
    Utils::byteToBitsBE(in[7U], m_rawData + 56U);
    Utils::byteToBitsBE(in[8U], m_rawData + 64U);
}

/// <summary>
///
/// </summary>
void ShortLC::decodeDeInterleave()
{
    for (uint32_t i = 0U; i < 68U; i++)
        m_deInterData[i] = false;

    for (uint32_t a = 0U; a < 67U; a++) {
        // Calculate the interleave sequence
        uint32_t interleaveSequence = (a * 4U) % 67U;
        // Shuffle the data
        m_deInterData[a] = m_rawData[interleaveSequence];
    }

    m_deInterData[67U] = m_rawData[67U];
}

/// <summary>
///
/// </summary>
/// <returns></returns>
bool ShortLC::decodeErrorCheck()
{
    // Run through each of the 3 rows containing data
    edac::Hamming::decode17123(m_deInterData + 0U);
    edac::Hamming::decode17123(m_deInterData + 17U);
    edac::Hamming::decode17123(m_deInterData + 34U);

    // Run through each of the 17 columns
    for (uint32_t c = 0U; c < 17U; c++) {
        bool bit = m_deInterData[c + 0U] ^ m_deInterData[c + 17U] ^ m_deInterData[c + 34U];
        if (bit != m_deInterData[c + 51U])
            return false;
    }

    return true;
}

/// <summary>
///
/// </summary>
/// <param name="data"></param>
void ShortLC::decodeExtractData(uint8_t* data) const
{
    assert(data != NULL);

    bool bData[40U];

    for (uint32_t i = 0U; i < 40U; i++)
        bData[i] = false;

    uint32_t pos = 4U;
    for (uint32_t a = 0U; a < 12U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 17U; a < 29U; a++, pos++)
        bData[pos] = m_deInterData[a];

    for (uint32_t a = 34U; a < 46U; a++, pos++)
        bData[pos] = m_deInterData[a];

    Utils::bitsToByteBE(bData + 0U, data[0U]);
    Utils::bitsToByteBE(bData + 8U, data[1U]);
    Utils::bitsToByteBE(bData + 16U, data[2U]);
    Utils::bitsToByteBE(bData + 24U, data[3U]);
    Utils::bitsToByteBE(bData + 32U, data[4U]);
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
void ShortLC::encodeExtractData(const uint8_t* in) const
{
    assert(in != NULL);

    bool bData[40U];
    Utils::byteToBitsBE(in[0U], bData + 0U);
    Utils::byteToBitsBE(in[1U], bData + 8U);
    Utils::byteToBitsBE(in[2U], bData + 16U);
    Utils::byteToBitsBE(in[3U], bData + 24U);
    Utils::byteToBitsBE(in[4U], bData + 32U);

    for (uint32_t i = 0U; i < 68U; i++)
        m_deInterData[i] = false;

    uint32_t pos = 4U;
    for (uint32_t a = 0U; a < 12U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 17U; a < 29U; a++, pos++)
        m_deInterData[a] = bData[pos];

    for (uint32_t a = 34U; a < 46U; a++, pos++)
        m_deInterData[a] = bData[pos];
}

/// <summary>
///
/// </summary>
void ShortLC::encodeErrorCheck()
{
    // Run through each of the 3 rows containing data
    edac::Hamming::encode17123(m_deInterData + 0U);
    edac::Hamming::encode17123(m_deInterData + 17U);
    edac::Hamming::encode17123(m_deInterData + 34U);

    // Run through each of the 17 columns
    for (uint32_t c = 0U; c < 17U; c++)
        m_deInterData[c + 51U] = m_deInterData[c + 0U] ^ m_deInterData[c + 17U] ^ m_deInterData[c + 34U];
}

/// <summary>
///
/// </summary>
void ShortLC::encodeInterleave()
{
    for (uint32_t i = 0U; i < 72U; i++)
        m_rawData[i] = false;

    for (uint32_t a = 0U; a < 67U; a++) {
        // Calculate the interleave sequence
        uint32_t interleaveSequence = (a * 4U) % 67U;

        // Unshuffle the data
        m_rawData[interleaveSequence] = m_deInterData[a];
    }

    m_rawData[67U] = m_deInterData[67U];
}

/// <summary>
///
/// </summary>
/// <param name="data"></param>
void ShortLC::encodeExtractBinary(uint8_t* data)
{
    assert(data != NULL);

    Utils::bitsToByteBE(m_rawData + 0U, data[0U]);
    Utils::bitsToByteBE(m_rawData + 8U, data[1U]);
    Utils::bitsToByteBE(m_rawData + 16U, data[2U]);
    Utils::bitsToByteBE(m_rawData + 24U, data[3U]);
    Utils::bitsToByteBE(m_rawData + 32U, data[4U]);
    Utils::bitsToByteBE(m_rawData + 40U, data[5U]);
    Utils::bitsToByteBE(m_rawData + 48U, data[6U]);
    Utils::bitsToByteBE(m_rawData + 56U, data[7U]);
    Utils::bitsToByteBE(m_rawData + 64U, data[8U]);
}
