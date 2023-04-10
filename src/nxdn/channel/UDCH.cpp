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
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#include "nxdn/channel/UDCH.h"
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
    0U,  29U, 58U,  87U, 116U, 145U, 174U, 203U, 232U, 261U, 290U, 319U,
    1U,  30U, 59U,  88U, 117U, 146U, 175U, 204U, 233U, 262U, 291U, 320U,
    2U,  31U, 60U,  89U, 118U, 147U, 176U, 205U, 234U, 263U, 292U, 321U,
    3U,  32U, 61U,  90U, 119U, 148U, 177U, 206U, 235U, 264U, 293U, 322U,
    4U,  33U, 62U,  91U, 120U, 149U, 178U, 207U, 236U, 265U, 294U, 323U,
    5U,  34U, 63U,  92U, 121U, 150U, 179U, 208U, 237U, 266U, 295U, 324U,
    6U,  35U, 64U,  93U, 122U, 151U, 180U, 209U, 238U, 267U, 296U, 325U,
    7U,  36U, 65U,  94U, 123U, 152U, 181U, 210U, 239U, 268U, 297U, 326U,
    8U,  37U, 66U,  95U, 124U, 153U, 182U, 211U, 240U, 269U, 298U, 327U,
    9U,  38U, 67U,  96U, 125U, 154U, 183U, 212U, 241U, 270U, 299U, 328U,
    10U, 39U, 68U,  97U, 126U, 155U, 184U, 213U, 242U, 271U, 300U, 329U,
    11U, 40U, 69U,  98U, 127U, 156U, 185U, 214U, 243U, 272U, 301U, 330U,
    12U, 41U, 70U,  99U, 128U, 157U, 186U, 215U, 244U, 273U, 302U, 331U,
    13U, 42U, 71U, 100U, 129U, 158U, 187U, 216U, 245U, 274U, 303U, 332U,
    14U, 43U, 72U, 101U, 130U, 159U, 188U, 217U, 246U, 275U, 304U, 333U,
    15U, 44U, 73U, 102U, 131U, 160U, 189U, 218U, 247U, 276U, 305U, 334U,
    16U, 45U, 74U, 103U, 132U, 161U, 190U, 219U, 248U, 277U, 306U, 335U,
    17U, 46U, 75U, 104U, 133U, 162U, 191U, 220U, 249U, 278U, 307U, 336U,
    18U, 47U, 76U, 105U, 134U, 163U, 192U, 221U, 250U, 279U, 308U, 337U,
    19U, 48U, 77U, 106U, 135U, 164U, 193U, 222U, 251U, 280U, 309U, 338U,
    20U, 49U, 78U, 107U, 136U, 165U, 194U, 223U, 252U, 281U, 310U, 339U,
    21U, 50U, 79U, 108U, 137U, 166U, 195U, 224U, 253U, 282U, 311U, 340U,
    22U, 51U, 80U, 109U, 138U, 167U, 196U, 225U, 254U, 283U, 312U, 341U,
    23U, 52U, 81U, 110U, 139U, 168U, 197U, 226U, 255U, 284U, 313U, 342U,
    24U, 53U, 82U, 111U, 140U, 169U, 198U, 227U, 256U, 285U, 314U, 343U,
    25U, 54U, 83U, 112U, 141U, 170U, 199U, 228U, 257U, 286U, 315U, 344U,
    26U, 55U, 84U, 113U, 142U, 171U, 200U, 229U, 258U, 287U, 316U, 345U,
    27U, 56U, 85U, 114U, 143U, 172U, 201U, 230U, 259U, 288U, 317U, 346U,
    28U, 57U, 86U, 115U, 144U, 173U, 202U, 231U, 260U, 289U, 318U, 347U };

const uint32_t PUNCTURE_LIST[] = {
    3U,  11U,  17U,  25U,  31U,  39U,  45U,  53U,  59U,  67U,
    73U,  81U,  87U,  95U, 101U, 109U, 115U, 123U, 129U, 137U,
    143U, 151U, 157U, 165U, 171U, 179U, 185U, 193U, 199U, 207U,
    213U, 221U, 227U, 235U, 241U, 249U, 255U, 263U, 269U, 277U,
    283U, 291U, 297U, 305U, 311U, 319U, 325U, 333U, 339U, 347U,
    353U, 361U, 367U, 375U, 381U, 389U, 395U, 403U };

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the UDCH class.
/// </summary>
UDCH::UDCH() :
    m_ran(0U),
    m_data(nullptr)
{
    m_data = new uint8_t[NXDN_UDCH_CRC_LENGTH_BYTES];
    ::memset(m_data, 0x00U, NXDN_UDCH_CRC_LENGTH_BYTES);
}

/// <summary>
/// Initializes a copy instance of the UDCH class.
/// </summary>
/// <param name="data"></param>
UDCH::UDCH(const UDCH& data) :
    m_ran(0U),
    m_data(nullptr)
{
    copy(data);
}

/// <summary>
/// Finalizes a instance of UDCH class.
/// </summary>
UDCH::~UDCH()
{
    delete[] m_data;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
UDCH& UDCH::operator=(const UDCH& data)
{
    if (&data != this) {
        ::memcpy(m_data, data.m_data, NXDN_UDCH_CRC_LENGTH_BYTES);

        m_ran = m_data[0U] & 0x3FU;
    }

    return *this;
}

/// <summary>
/// Decode a user data channel.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if UDCH was decoded, otherwise false.</returns>
bool UDCH::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t buffer[NXDN_UDCH_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_UDCH_FEC_LENGTH_BYTES);

    // deinterleave
    for (uint32_t i = 0U; i < NXDN_UDCH_FEC_LENGTH_BITS; i++) {
        uint32_t n = INTERLEAVE_TABLE[i] + NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS;
        bool b = READ_BIT(data, n);
        WRITE_BIT(buffer, i, b);
    }

#if DEBUG_NXDN_UDCH
    Utils::dump(2U, "UDCH::decode(), UDCH Raw", buffer, NXDN_UDCH_FEC_LENGTH_BYTES);
#endif

    // depuncture
    uint8_t puncture[420U];
    uint32_t n = 0U, index = 0U;
    for (uint32_t i = 0U; i < NXDN_UDCH_FEC_LENGTH_BITS; i++) {
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
    for (uint32_t i = 0U; i < (NXDN_UDCH_CRC_LENGTH_BITS + 4U); i++) {
        uint8_t s0 = puncture[n++];
        uint8_t s1 = puncture[n++];

        if (!conv.decode(s0, s1)) {
            LogError(LOG_NXDN, "UDCH::decode(), failed to decode convolution");
            return false;
        }
    }

    conv.chainback(m_data, NXDN_UDCH_CRC_LENGTH_BITS);

#if DEBUG_NXDN_UDCH
    Utils::dump(2U, "Decoded UDCH", m_data, NXDN_UDCH_CRC_LENGTH_BYTES);
#endif

    // check CRC-15
    bool ret = edac::CRC::checkCRC15(m_data, NXDN_UDCH_LENGTH_BITS);
    if (!ret) {
        LogError(LOG_NXDN, "UDCH::decode(), failed CRC-15 check");
        return false;
    }

    m_ran = m_data[0U] & 0x3FU;

    return true;
}

/// <summary>
/// Encode a user data channel.
/// </summary>
/// <param name="data"></param>
void UDCH::encode(uint8_t* data) const
{
    assert(data != nullptr);

    m_data[0U] = m_ran;

    uint8_t buffer[NXDN_UDCH_CRC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_UDCH_CRC_LENGTH_BYTES);
    ::memcpy(buffer, m_data, 23U);

    edac::CRC::addCRC15(buffer, NXDN_UDCH_LENGTH_BITS);

#if DEBUG_NXDN_UDCH
    Utils::dump(2U, "Encoded UDCH", m_data, NXDN_UDCH_CRC_LENGTH_BYTES);
#endif

    // encode convolution
    uint8_t convolution[NXDN_UDCH_FEC_CONV_LENGTH_BYTES];
    ::memset(convolution, 0x00U, NXDN_UDCH_FEC_CONV_LENGTH_BYTES);

    edac::Convolution conv;
    conv.encode(buffer, convolution, NXDN_UDCH_CRC_LENGTH_BITS);

    // puncture
    uint8_t puncture[NXDN_UDCH_FEC_LENGTH_BYTES];
    ::memset(puncture, 0x00U, NXDN_UDCH_FEC_LENGTH_BYTES);

    uint32_t n = 0U, index = 0U;
    for (uint32_t i = 0U; i < NXDN_UDCH_FEC_CONV_LENGTH_BITS; i++) {
        if (i != PUNCTURE_LIST[index]) {
            bool b = READ_BIT(convolution, i);
            WRITE_BIT(puncture, n, b);
            n++;
        } else {
            index++;
        }
    }

    // interleave
    for (uint32_t i = 0U; i < NXDN_UDCH_FEC_LENGTH_BITS; i++) {
        uint32_t n = INTERLEAVE_TABLE[i] + NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS;
        bool b = READ_BIT(puncture, i);
        WRITE_BIT(data, n, b);
    }

#if DEBUG_NXDN_UDCH
    Utils::dump(2U, "UDCH::encode(), UDCH Puncture and Interleave", data, NXDN_UDCH_FEC_LENGTH_BYTES);
#endif
}

/// <summary>
/// Gets the raw UDCH data.
/// </summary>
/// <param name="data"></param>
void UDCH::getData(uint8_t* data) const
{
    assert(data != nullptr);

    ::memcpy(data, m_data + 1U, NXDN_RTCH_LC_LENGTH_BYTES);
}

/// <summary>
/// Sets the raw UDCH data.
/// </summary>
/// <param name="data"></param>
void UDCH::setData(const uint8_t* data)
{
    assert(data != nullptr);

    ::memcpy(m_data + 1U, data, NXDN_RTCH_LC_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void UDCH::copy(const UDCH& data)
{
    m_data = new uint8_t[NXDN_UDCH_CRC_LENGTH_BYTES];
    ::memcpy(m_data, data.m_data, NXDN_UDCH_CRC_LENGTH_BYTES);

    m_ran = m_data[0U] & 0x3FU;
}
