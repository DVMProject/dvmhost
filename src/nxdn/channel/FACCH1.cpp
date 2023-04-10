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
#include "nxdn/channel/FACCH1.h"
#include "nxdn/edac/Convolution.h"
#include "nxdn/NXDNDefines.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::channel;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t INTERLEAVE_TABLE[] = {
    0U,  9U, 18U, 27U, 36U, 45U, 54U, 63U, 72U, 81U, 90U,  99U, 108U, 117U, 126U, 135U,
    1U, 10U, 19U, 28U, 37U, 46U, 55U, 64U, 73U, 82U, 91U, 100U, 109U, 118U, 127U, 136U,
    2U, 11U, 20U, 29U, 38U, 47U, 56U, 65U, 74U, 83U, 92U, 101U, 110U, 119U, 128U, 137U,
    3U, 12U, 21U, 30U, 39U, 48U, 57U, 66U, 75U, 84U, 93U, 102U, 111U, 120U, 129U, 138U,
    4U, 13U, 22U, 31U, 40U, 49U, 58U, 67U, 76U, 85U, 94U, 103U, 112U, 121U, 130U, 139U,
    5U, 14U, 23U, 32U, 41U, 50U, 59U, 68U, 77U, 86U, 95U, 104U, 113U, 122U, 131U, 140U,
    6U, 15U, 24U, 33U, 42U, 51U, 60U, 69U, 78U, 87U, 96U, 105U, 114U, 123U, 132U, 141U,
    7U, 16U, 25U, 34U, 43U, 52U, 61U, 70U, 79U, 88U, 97U, 106U, 115U, 124U, 133U, 142U,
    8U, 17U, 26U, 35U, 44U, 53U, 62U, 71U, 80U, 89U, 98U, 107U, 116U, 125U, 134U, 143U };

const uint32_t PUNCTURE_LIST[] = {
    1U,   5U,   9U,  13U,  17U,  21U,  25U,  29U,  33U,  37U,
    41U,  45U,  49U,  53U,  57U,  61U,  65U,  69U,  73U,  77U,
    81U,  85U,  89U,  93U,  97U, 101U, 105U, 109U, 113U, 117U,
    121U, 125U, 129U, 133U, 137U, 141U, 145U, 149U, 153U, 157U,
    161U, 165U, 169U, 173U, 177U, 181U, 185U, 189U };

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the FACCH1 class.
/// </summary>
FACCH1::FACCH1() :
    m_data(nullptr)
{
    m_data = new uint8_t[NXDN_FACCH1_CRC_LENGTH_BYTES];
    ::memset(m_data, 0x00U, NXDN_FACCH1_CRC_LENGTH_BYTES);
}

/// <summary>
/// Initializes a copy instance of the FACCH1 class.
/// </summary>
/// <param name="data"></param>
FACCH1::FACCH1(const FACCH1& data) :
    m_data(nullptr)
{
    copy(data);
}

/// <summary>
/// Finalizes a instance of FACCH1 class.
/// </summary>
FACCH1::~FACCH1()
{
    delete[] m_data;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
FACCH1& FACCH1::operator=(const FACCH1& data)
{
    if (&data != this) {
        ::memcpy(m_data, data.m_data, NXDN_FACCH1_CRC_LENGTH_BYTES);
    }

    return *this;
}

/// <summary>
/// Decode a fast associated control channel 1.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if FACCH1 was decoded, otherwise false.</returns>
bool FACCH1::decode(const uint8_t* data, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t buffer[NXDN_FACCH1_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_FACCH1_FEC_LENGTH_BYTES);

    // deinterleave
    for (uint32_t i = 0U; i < NXDN_FACCH1_FEC_LENGTH_BITS; i++) {
        uint32_t n = INTERLEAVE_TABLE[i] + offset;
        bool b = READ_BIT(data, n);
        WRITE_BIT(buffer, i, b);
    }

#if DEBUG_NXDN_FACCH1
    Utils::dump(2U, "FACCH1::decode(), FACCH1 Raw", buffer, NXDN_FACCH1_FEC_LENGTH_BYTES);
#endif

    // depuncture
    uint8_t puncture[210U];
    uint32_t n = 0U, index = 0U;
    for (uint32_t i = 0U; i < NXDN_FACCH1_FEC_LENGTH_BITS; i++) {
        if (n == PUNCTURE_LIST[index]) {
            puncture[n++] = 1U;
            index++;
        }

        bool b = READ_BIT(buffer, i);
        puncture[n++] = b ? 2U : 0U;
    }

    for (uint32_t i = 0U; i < 8U; i++) {
        puncture[n++] = 0U;
    }

    // decode convolution
    edac::Convolution conv;
    conv.start();

    n = 0U;
    for (uint32_t i = 0U; i < (NXDN_FACCH1_CRC_LENGTH_BITS + 4U); i++) {
        uint8_t s0 = puncture[n++];
        uint8_t s1 = puncture[n++];

        if (!conv.decode(s0, s1)) {
            LogError(LOG_NXDN, "FACCH1::decode(), failed to decode convolution");
            return false;
        }
    }

    conv.chainback(m_data, NXDN_FACCH1_CRC_LENGTH_BITS);

#if DEBUG_NXDN_FACCH1
    Utils::dump(2U, "Decoded FACCH1", m_data, NXDN_FACCH1_CRC_LENGTH_BYTES);
#endif

    // check CRC-12
    bool ret = edac::CRC::checkCRC12(m_data, NXDN_FACCH1_LENGTH_BITS);
    if (!ret) {
        LogError(LOG_NXDN, "FACCH1::decode(), failed CRC-12 check");
        return false;
    }

    return true;
}

/// <summary>
/// Encode a fast associated control channel 1.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if LICH was decoded, otherwise false.</returns>
void FACCH1::encode(uint8_t* data, uint32_t offset) const
{
    assert(data != nullptr);

    uint8_t buffer[NXDN_FACCH1_CRC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_FACCH1_CRC_LENGTH_BYTES);
    ::memcpy(buffer, m_data, NXDN_FACCH1_CRC_LENGTH_BYTES - 2U);

    edac::CRC::addCRC12(buffer, NXDN_FACCH1_LENGTH_BITS);

#if DEBUG_NXDN_FACCH1
    Utils::dump(2U, "Encoded FACCH1", buffer, NXDN_FACCH1_CRC_LENGTH_BYTES);
#endif

    // encode convolution
    uint8_t convolution[NXDN_FACCH1_FEC_CONV_LENGTH_BYTES];
    ::memset(convolution, 0x00U, NXDN_FACCH1_FEC_CONV_LENGTH_BYTES);

    edac::Convolution conv;
    conv.encode(buffer, convolution, NXDN_FACCH1_CRC_LENGTH_BITS);

    // puncture
    uint8_t puncture[NXDN_FACCH1_FEC_LENGTH_BYTES];
    ::memset(puncture, 0x00U, NXDN_FACCH1_FEC_LENGTH_BYTES);

    uint32_t n = 0U, index = 0U;
    for (uint32_t i = 0U; i < NXDN_FACCH1_FEC_CONV_LENGTH_BITS; i++) {
        if (i != PUNCTURE_LIST[index]) {
            bool b = READ_BIT(convolution, i);
            WRITE_BIT(puncture, n, b);
            n++;
        } else {
            index++;
        }
    }

    // interleave
    for (uint32_t i = 0U; i < NXDN_FACCH1_FEC_LENGTH_BITS; i++) {
        uint32_t n = INTERLEAVE_TABLE[i] + offset;
        bool b = READ_BIT(puncture, i);
        WRITE_BIT(data, n, b);
    }

#if DEBUG_NXDN_SACCH
    Utils::dump(2U, "FACCH1::encode(), FACCH1 Puncture and Interleave", data, NXDN_FACCH1_FEC_LENGTH_BYTES);
#endif
}

/// <summary>
/// Gets the raw FACCH1 data.
/// </summary>
/// <param name="data"></param>
void FACCH1::getData(uint8_t* data) const
{
    assert(data != nullptr);

    ::memcpy(data, m_data, NXDN_FACCH1_CRC_LENGTH_BYTES - 2U);
}

/// <summary>
/// Sets the raw FACCH1 data.
/// </summary>
/// <param name="data"></param>
void FACCH1::setData(const uint8_t* data)
{
    assert(data != nullptr);

    ::memcpy(m_data, data, NXDN_FACCH1_CRC_LENGTH_BYTES - 2U);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void FACCH1::copy(const FACCH1& data)
{
    m_data = new uint8_t[NXDN_FACCH1_CRC_LENGTH_BYTES];
    ::memcpy(m_data, data.m_data, NXDN_FACCH1_CRC_LENGTH_BYTES);
}
