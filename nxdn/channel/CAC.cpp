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
#include "nxdn/channel/CAC.h"
#include "nxdn/Convolution.h"
#include "nxdn/NXDNDefines.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace edac;
using namespace nxdn;
using namespace nxdn::channel;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t INTERLEAVE_TABLE_OUT[] = {
    0U,  25U, 50U, 75U, 100U, 125U, 150U, 175U, 200U, 225U, 250U, 275U,
    1U,  26U, 51U, 76U, 101U, 126U, 151U, 176U, 201U, 226U, 251U, 276U,
    2U,  27U, 52U, 77U, 102U, 127U, 152U, 177U, 202U, 227U, 252U, 277U,
    3U,  28U, 53U, 78U, 103U, 128U, 153U, 178U, 203U, 228U, 253U, 278U,
    4U,  29U, 54U, 79U, 104U, 129U, 154U, 179U, 204U, 229U, 254U, 279U,
    5U,  30U, 55U, 80U, 105U, 130U, 155U, 180U, 205U, 230U, 255U, 280U,
    6U,  31U, 56U, 81U, 106U, 131U, 156U, 181U, 206U, 231U, 256U, 281U,
    7U,  32U, 57U, 82U, 107U, 132U, 157U, 182U, 207U, 232U, 257U, 282U,
    8U,  33U, 58U, 83U, 108U, 133U, 158U, 183U, 208U, 233U, 258U, 283U,
    9U,  34U, 59U, 84U, 109U, 134U, 159U, 184U, 209U, 234U, 259U, 284U,
    10U, 35U, 60U, 85U, 110U, 135U, 160U, 185U, 210U, 235U, 260U, 285U,
    11U, 36U, 61U, 86U, 111U, 136U, 161U, 186U, 211U, 236U, 261U, 286U,
    12U, 37U, 62U, 87U, 112U, 137U, 162U, 187U, 212U, 237U, 262U, 287U,
    13U, 38U, 63U, 88U, 113U, 138U, 163U, 188U, 213U, 238U, 263U, 288U,
    14U, 39U, 64U, 89U, 114U, 139U, 164U, 189U, 214U, 239U, 264U, 289U,
    15U, 40U, 65U, 90U, 115U, 140U, 165U, 190U, 215U, 240U, 265U, 290U,
    16U, 41U, 66U, 91U, 116U, 141U, 166U, 191U, 216U, 241U, 266U, 291U,
    17U, 42U, 67U, 92U, 117U, 142U, 167U, 192U, 217U, 242U, 267U, 292U,
    18U, 43U, 68U, 93U, 118U, 143U, 168U, 193U, 218U, 243U, 268U, 293U,
    19U, 44U, 69U, 94U, 119U, 144U, 169U, 194U, 219U, 244U, 269U, 294U,
    20U, 45U, 70U, 95U, 120U, 145U, 170U, 195U, 220U, 245U, 270U, 295U,
    21U, 46U, 71U, 96U, 121U, 146U, 171U, 196U, 221U, 246U, 271U, 296U,
    22U, 47U, 72U, 97U, 122U, 147U, 172U, 197U, 222U, 247U, 272U, 297U,
    23U, 48U, 73U, 98U, 123U, 148U, 173U, 198U, 223U, 248U, 273U, 298U,
    24U, 49U, 74U, 99U, 124U, 149U, 174U, 199U, 224U, 249U, 274U, 299U };

const uint32_t INTERLEAVE_TABLE_IN[] = {
    0U,  21U, 42U, 63U, 84U,  105U, 126U, 147U, 168U, 189U, 210U, 231U,
    1U,  22U, 43U, 64U, 85U,  106U, 127U, 148U, 169U, 190U, 211U, 232U,
    2U,  23U, 44U, 65U, 86U,  107U, 128U, 149U, 170U, 191U, 212U, 233U,
    3U,  24U, 45U, 66U, 87U,  108U, 129U, 150U, 171U, 192U, 213U, 234U,
    4U,  25U, 46U, 67U, 88U,  109U, 130U, 151U, 172U, 193U, 214U, 235U,
    5U,  26U, 47U, 68U, 89U,  110U, 131U, 152U, 173U, 194U, 215U, 236U,
    6U,  27U, 48U, 69U, 90U,  111U, 132U, 153U, 174U, 195U, 216U, 237U,
    7U,  28U, 49U, 70U, 91U,  112U, 133U, 154U, 175U, 196U, 217U, 238U,
    8U,  29U, 50U, 71U, 92U,  113U, 134U, 155U, 176U, 197U, 218U, 239U,
    9U,  30U, 51U, 72U, 93U,  114U, 135U, 156U, 177U, 198U, 219U, 240U,
    10U, 31U, 52U, 73U, 94U,  115U, 136U, 157U, 178U, 199U, 220U, 241U,
    11U, 32U, 53U, 74U, 95U,  116U, 137U, 158U, 179U, 200U, 221U, 242U,
    12U, 33U, 54U, 75U, 96U,  117U, 138U, 159U, 180U, 201U, 222U, 243U,
    13U, 34U, 55U, 76U, 97U,  118U, 139U, 160U, 181U, 202U, 223U, 244U,
    14U, 35U, 56U, 77U, 98U,  119U, 140U, 161U, 182U, 203U, 224U, 245U,
    15U, 36U, 57U, 78U, 99U,  120U, 141U, 162U, 183U, 204U, 225U, 246U,
    16U, 37U, 58U, 79U, 100U, 121U, 142U, 163U, 184U, 205U, 226U, 247U,
    17U, 38U, 59U, 80U, 101U, 122U, 143U, 164U, 185U, 206U, 227U, 248U,
    18U, 39U, 60U, 81U, 102U, 123U, 144U, 165U, 186U, 207U, 228U, 249U,
    19U, 40U, 61U, 82U, 103U, 124U, 145U, 166U, 187U, 208U, 229U, 250U,
    20U, 41U, 62U, 83U, 104U, 125U, 146U, 167U, 188U, 209U, 230U, 251U };

const uint32_t PUNCTURE_LIST_LONG_IN[] = {
    1U, 7U, 9U, 11U, 19U, 27U, 33U, 35U, 37U, 45U,
    53U, 59U, 61U, 63U, 71U, 79U, 85U, 87U, 89U, 97U,
    105U, 111U, 113U, 115U, 123U, 131U, 137U, 139U, 141U, 149U,
    157U, 163U, 165U, 167U, 175U, 183U, 189U, 191U, 193U, 201U,
    209U, 215U, 217U, 219U, 227U, 235U, 241U, 243U, 245U, 253U,
    261U, 267U, 269U, 271U, 279U, 287U, 293U, 295U, 297U, 305U };

const uint32_t PUNCTURE_LIST_OUT[] = {
    3U, 11U, 17U, 25U, 31U, 39U, 45U, 53U, 59U, 67U,
    73U, 81U, 87U, 95U, 101U, 109U, 115U, 123U, 129U, 137U,
    143U, 151U, 157U, 165U, 171U, 179U, 185U, 193U, 199U, 207U,
    213U, 221U, 227U, 235U, 241U, 249U, 255U, 263U, 269U, 277U,
    283U, 291U, 297U, 305U, 311U, 319U, 325U, 333U, 339U, 347U };

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CAC class.
/// </summary>
CAC::CAC() :
    m_verbose(false),
    m_ran(1U),
    m_structure(NXDN_SR_RCCH_SINGLE),
    m_idleBusy(true),
    m_txContinuous(false),
    m_receive(true),
    m_data(NULL),
    m_rxCRC(0U)
{
    m_data = new uint8_t[NXDN_CAC_CRC_LENGTH_BYTES];
    ::memset(m_data, 0x00U, NXDN_CAC_CRC_LENGTH_BYTES);
}

/// <summary>
/// Initializes a copy instance of the CAC class.
/// </summary>
/// <param name="data"></param>
CAC::CAC(const CAC& data) :
    m_verbose(false),
    m_ran(1U),
    m_structure(NXDN_SR_RCCH_SINGLE),
    m_idleBusy(true),
    m_txContinuous(false),
    m_receive(true),
    m_data(NULL),
    m_rxCRC(0U)
{
    copy(data);
}

/// <summary>
/// Finalizes a instance of CAC class.
/// </summary>
CAC::~CAC()
{
    delete[] m_data;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
CAC& CAC::operator=(const CAC& data)
{
    if (&data != this) {
        ::memcpy(m_data, data.m_data, NXDN_CAC_CRC_LENGTH_BYTES);

        m_verbose = data.m_verbose;

        m_ran = m_data[0U] & 0x3FU;
        m_structure = (m_data[0U] >> 6) & 0x03U;

        m_idleBusy = data.m_idleBusy;
        m_txContinuous = data.m_txContinuous;
        m_receive = data.m_receive;

        m_rxCRC = data.m_rxCRC;
    }

    return *this;
}

/// <summary>
/// Decode a common access channel.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CAC was decoded, otherwise false.</returns>
bool CAC::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint8_t buffer[NXDN_CAC_IN_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_CAC_IN_FEC_LENGTH_BYTES);

    // deinterleave
    for (uint32_t i = 0U; i < NXDN_CAC_IN_FEC_LENGTH_BITS; i++) {
        uint32_t n = INTERLEAVE_TABLE_IN[i] + NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS;
        bool b = READ_BIT(data, n);
        WRITE_BIT(buffer, i, b);
    }

#if DEBUG_NXDN_CAC
    Utils::dump(2U, "CAC::decode(), CAC Raw", buffer, NXDN_CAC_IN_FEC_LENGTH_BYTES);
#endif

    // TODO TODO -- Long CAC Puncturing

    // decode convolution
    Convolution conv;
    conv.start();

    uint32_t n = 0U;
    for (uint32_t i = 0U; i < (NXDN_CAC_SHORT_CRC_LENGTH_BITS + 4U); i++) {
        uint8_t s0 = buffer[n++];
        uint8_t s1 = buffer[n++];

        if (!conv.decode(s0, s1)) {
            LogError(LOG_NXDN, "CAC::decode(), failed to decode convolution");
            return false;
        }
    }

    conv.chainback(m_data, NXDN_CAC_SHORT_CRC_LENGTH_BITS);

    if (m_verbose) {
        Utils::dump(2U, "Decoded CAC", m_data, (NXDN_CAC_SHORT_CRC_LENGTH_BITS / 8U) + 1U);
    }

    // check CRC-16
    bool ret = CRC::checkCRC16(m_data, NXDN_CAC_SHORT_LENGTH_BITS);
    if (!ret) {
        LogError(LOG_NXDN, "CAC::decode(), failed CRC-6 check");
        return false;
    }

    // store recieved CRC-16
    uint8_t crc[2U];
    ::memset(crc, 0x00U, 2U);

    m_ran = m_data[0U] & 0x3FU;
    m_structure = (m_data[0U] >> 6) & 0x03U;

    uint32_t offset = NXDN_CAC_SHORT_CRC_LENGTH_BITS - 20U;
    for (uint32_t i = 0U; i < 16U; i++, offset++) {
        bool b = READ_BIT(m_data, offset);
        WRITE_BIT(crc, i, b);
    }

    m_rxCRC = (crc[0U] << 8) | (crc[1U] << 0);

#if DEBUG_NXDN_CAC
    if (m_verbose) {
        Utils::dump(2U, "Raw CAC Buffer", m_data, NXDN_CAC_FEC_LENGTH_BYTES);
    }
#endif
    return true;
}

/// <summary>
/// Encode a common access channel.
/// </summary>
/// <param name="data"></param>
void CAC::encode(uint8_t* data) const
{
    assert(data != NULL);

    m_data[0U] &= 0xC0U;
    m_data[0U] |= m_ran;

    m_data[0U] &= 0x3FU;
    m_data[0U] |= (m_structure << 6) & 0xC0U;

    uint8_t buffer[NXDN_CAC_FEC_LENGTH_BYTES];
    ::memset(buffer, 0x00U, NXDN_CAC_FEC_LENGTH_BYTES);

    for (uint32_t i = 0U; i < NXDN_CAC_LENGTH_BITS; i++) {
        bool b = READ_BIT(m_data, i);
        WRITE_BIT(buffer, i, b);
    }

    CRC::addCRC16(buffer, NXDN_CAC_LENGTH_BITS);

    if (m_verbose) {
        Utils::dump(2U, "Encoded CAC", buffer, NXDN_CAC_FEC_LENGTH_BYTES);
    }

    // encode convolution
    uint8_t convolution[NXDN_CAC_FEC_CONV_LENGTH_BYTES];
    ::memset(convolution, 0x00U, NXDN_CAC_FEC_CONV_LENGTH_BYTES);

    Convolution conv;
    conv.encode(buffer, convolution, NXDN_CAC_CRC_LENGTH_BITS);

#if DEBUG_NXDN_CAC
    Utils::dump(2U, "CAC::encode(), CAC Convolution", convolution, NXDN_CAC_FEC_CONV_LENGTH_BYTES);
#endif

    // puncture
    uint8_t puncture[NXDN_CAC_FEC_LENGTH_BYTES];
    ::memset(puncture, 0x00U, NXDN_CAC_FEC_LENGTH_BYTES);

    uint32_t n = 0U, index = 0U;
    for (uint32_t i = 0U; i < NXDN_CAC_FEC_CONV_LENGTH_BITS; i++) {
        if (i != PUNCTURE_LIST_OUT[index]) {
            bool b = READ_BIT(convolution, i);
            WRITE_BIT(puncture, n, b);
            n++;
        } else {
            index++;
        }
    }

    // interleave
    for (uint32_t i = 0U; i < NXDN_CAC_FEC_LENGTH_BITS; i++) {
        uint32_t n = INTERLEAVE_TABLE_OUT[i] + NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS;
        bool b = READ_BIT(puncture, i);
        WRITE_BIT(data, n, b);
    }

#if DEBUG_NXDN_CAC
    Utils::dump(2U, "CAC::encode(), CAC Puncture and Interleave", data, NXDN_FRAME_LENGTH_BYTES);
#endif

    // apply control field
    uint8_t control[3U];
    ::memset(control, 0x00U, 3U);

    uint8_t parity = 0x03U;
    if (m_idleBusy && m_txContinuous)
        parity = 0x01U;

    control[0U] = ((m_idleBusy ? 0x03U : 0x01U) << 6) + ((m_txContinuous ? 0x03U : 0x01U) << 4) +
        (parity << 2) + ((m_receive ? 0x03U : 0x01U));
    control[1U] = (m_rxCRC >> 8U) & 0xFFU;
    control[2U] = (m_rxCRC >> 0U) & 0xFFU;

    for (uint32_t i = 0U; i < NXDN_CAC_E_POST_FIELD_BITS; i++) {
        uint32_t n = i + NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_CAC_FEC_LENGTH_BITS;
        bool b = READ_BIT(control, i);
        WRITE_BIT(data, n, b);
    }

#if DEBUG_NXDN_CAC
    Utils::dump(2U, "CAC::encode(), CAC + Control", data, NXDN_FRAME_LENGTH_BYTES);
#endif
}

/// <summary>
/// Gets the raw CAC data.
/// </summary>
/// <param name="data"></param>
void CAC::getData(uint8_t* data) const
{
    assert(data != NULL);

    uint32_t offset = 8U;
    for (uint32_t i = 0U; i < (NXDN_CAC_SHORT_LENGTH_BITS - 10); i++, offset++) {
        bool b = READ_BIT(m_data, offset);
        WRITE_BIT(data, i, b);
    }
}

/// <summary>
/// Sets the raw CAC data.
/// </summary>
/// <param name="data"></param>
void CAC::setData(const uint8_t* data)
{
    assert(data != NULL);

    ::memset(m_data, 0x00U, NXDN_CAC_CRC_LENGTH_BYTES);

    uint32_t offset = 8U;
    for (uint32_t i = 0U; i < (NXDN_CAC_CRC_LENGTH_BITS - 31); i++, offset++) {
        bool b = READ_BIT(data, i);
        WRITE_BIT(m_data, offset, b);
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void CAC::copy(const CAC& data)
{
    m_data = new uint8_t[NXDN_CAC_CRC_LENGTH_BYTES];
    ::memcpy(m_data, data.m_data, NXDN_CAC_CRC_LENGTH_BYTES);

    m_ran = m_data[0U] & 0x3FU;
    m_structure = (m_data[0U] >> 6) & 0x03U;

    m_idleBusy = data.m_idleBusy;
    m_txContinuous = data.m_txContinuous;
    m_receive = data.m_receive;

    m_rxCRC = data.m_rxCRC;
}
