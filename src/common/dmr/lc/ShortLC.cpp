// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
#include "Defines.h"
#include "dmr/lc/ShortLC.h"
#include "edac/Hamming.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::lc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ShortLC class. */
ShortLC::ShortLC() :
    m_rawData(nullptr),
    m_deInterData(nullptr)
{
    m_rawData = new bool[72U];
    m_deInterData = new bool[68U];
}

/* Finalizes a instance of the ShortLC class. */
ShortLC::~ShortLC()
{
    delete[] m_rawData;
    delete[] m_deInterData;
}

/* Decode DMR short-link control data. */
bool ShortLC::decode(const uint8_t* in, uint8_t* out)
{
    assert(in != nullptr);
    assert(out != nullptr);

    // get the raw binary
    decodeExtractBinary(in);

    // deinterleave
    decodeDeInterleave();

    // error check
    bool ret = decodeErrorCheck();
    if (!ret)
        return false;

    // extract Data
    decodeExtractData(out);

    return true;
}

/* Encode DMR short-link control data. */
void ShortLC::encode(const uint8_t* in, uint8_t* out)
{
    assert(in != nullptr);
    assert(out != nullptr);

    // extract Data
    encodeExtractData(in);

    // error check
    encodeErrorCheck();

    // interleave
    encodeInterleave();

    // get the raw binary
    encodeExtractBinary(out);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* */
void ShortLC::decodeExtractBinary(const uint8_t* in)
{
    assert(in != nullptr);

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

/* */
void ShortLC::decodeDeInterleave()
{
    for (uint32_t i = 0U; i < 68U; i++)
        m_deInterData[i] = false;

    for (uint32_t a = 0U; a < 67U; a++) {
        // calculate the interleave sequence
        uint32_t interleaveSequence = (a * 4U) % 67U;
        // shuffle the data
        m_deInterData[a] = m_rawData[interleaveSequence];
    }

    m_deInterData[67U] = m_rawData[67U];
}

/* */
bool ShortLC::decodeErrorCheck()
{
    // run through each of the 3 rows containing data
    edac::Hamming::decode17123(m_deInterData + 0U);
    edac::Hamming::decode17123(m_deInterData + 17U);
    edac::Hamming::decode17123(m_deInterData + 34U);

    // run through each of the 17 columns
    for (uint32_t c = 0U; c < 17U; c++) {
        bool bit = m_deInterData[c + 0U] ^ m_deInterData[c + 17U] ^ m_deInterData[c + 34U];
        if (bit != m_deInterData[c + 51U])
            return false;
    }

    return true;
}

/* */
void ShortLC::decodeExtractData(uint8_t* data) const
{
    assert(data != nullptr);

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

/* */
void ShortLC::encodeExtractData(const uint8_t* in) const
{
    assert(in != nullptr);

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

/* */
void ShortLC::encodeErrorCheck()
{
    // run through each of the 3 rows containing data
    edac::Hamming::encode17123(m_deInterData + 0U);
    edac::Hamming::encode17123(m_deInterData + 17U);
    edac::Hamming::encode17123(m_deInterData + 34U);

    // run through each of the 17 columns
    for (uint32_t c = 0U; c < 17U; c++)
        m_deInterData[c + 51U] = m_deInterData[c + 0U] ^ m_deInterData[c + 17U] ^ m_deInterData[c + 34U];
}

/* */
void ShortLC::encodeInterleave()
{
    for (uint32_t i = 0U; i < 72U; i++)
        m_rawData[i] = false;

    for (uint32_t a = 0U; a < 67U; a++) {
        // calculate the interleave sequence
        uint32_t interleaveSequence = (a * 4U) % 67U;

        // unshuffle the data
        m_rawData[interleaveSequence] = m_deInterData[a];
    }

    m_rawData[67U] = m_deInterData[67U];
}

/* */
void ShortLC::encodeExtractBinary(uint8_t* data)
{
    assert(data != nullptr);

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
