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
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
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
#include "dmr/data/EmbeddedData.h"
#include "edac/Hamming.h"
#include "edac/CRC.h"
#include "Utils.h"

using namespace dmr::data;
using namespace dmr;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the EmbeddedData class.
/// </summary>
EmbeddedData::EmbeddedData() :
    m_valid(false),
    m_FLCO(FLCO_GROUP),
    m_state(LCS_NONE),
    m_data(NULL),
    m_raw(NULL)
{
    m_raw = new bool[128U];
    m_data = new bool[72U];
}

/// <summary>
/// Finalizes a instance of the EmbeddedData class.
/// </summary>
EmbeddedData::~EmbeddedData()
{
    delete[] m_raw;
    delete[] m_data;
}

/// <summary>
/// Add LC data (which may consist of 4 blocks) to the data store.
/// </summary>
/// <param name="data"></param>
/// <param name="lcss"></param>
/// <returns></returns>
bool EmbeddedData::addData(const uint8_t* data, uint8_t lcss)
{
    assert(data != NULL);

    bool rawData[40U];
    Utils::byteToBitsBE(data[14U], rawData + 0U);
    Utils::byteToBitsBE(data[15U], rawData + 8U);
    Utils::byteToBitsBE(data[16U], rawData + 16U);
    Utils::byteToBitsBE(data[17U], rawData + 24U);
    Utils::byteToBitsBE(data[18U], rawData + 32U);

    // Is this the first block of a 4 block embedded LC ?
    if (lcss == 1U) {
        for (uint32_t a = 0U; a < 32U; a++)
            m_raw[a] = rawData[a + 4U];

        // Show we are ready for the next LC block
        m_state = LCS_FIRST;
        m_valid = false;

        return false;
    }

    // Is this the 2nd block of a 4 block embedded LC ?
    if (lcss == 3U && m_state == LCS_FIRST) {
        for (uint32_t a = 0U; a < 32U; a++)
            m_raw[a + 32U] = rawData[a + 4U];

        // Show we are ready for the next LC block
        m_state = LCS_SECOND;

        return false;
    }

    // Is this the 3rd block of a 4 block embedded LC ?
    if (lcss == 3U && m_state == LCS_SECOND) {
        for (uint32_t a = 0U; a < 32U; a++)
            m_raw[a + 64U] = rawData[a + 4U];

        // Show we are ready for the final LC block
        m_state = LCS_THIRD;

        return false;
    }

    // Is this the final block of a 4 block embedded LC ?
    if (lcss == 2U && m_state == LCS_THIRD) {
        for (uint32_t a = 0U; a < 32U; a++)
            m_raw[a + 96U] = rawData[a + 4U];

        // Show that we're not ready for any more data
        m_state = LCS_NONE;

        // Process the complete data block
        decodeEmbeddedData();
        if (m_valid)
            encodeEmbeddedData();

        return m_valid;
    }

    return false;
}

/// <summary>
///
/// </summary>
/// <param name="data"></param>
/// <param name="n"></param>
/// <returns></returns>
uint8_t EmbeddedData::getData(uint8_t* data, uint8_t n) const
{
    assert(data != NULL);

    if (n >= 1U && n < 5U) {
        n--;

        bool bits[40U];
        ::memset(bits, 0x00U, 40U * sizeof(bool));
        ::memcpy(bits + 4U, m_raw + n * 32U, 32U * sizeof(bool));

        uint8_t bytes[5U];
        Utils::bitsToByteBE(bits + 0U, bytes[0U]);
        Utils::bitsToByteBE(bits + 8U, bytes[1U]);
        Utils::bitsToByteBE(bits + 16U, bytes[2U]);
        Utils::bitsToByteBE(bits + 24U, bytes[3U]);
        Utils::bitsToByteBE(bits + 32U, bytes[4U]);

        data[14U] = (data[14U] & 0xF0U) | (bytes[0U] & 0x0FU);
        data[15U] = bytes[1U];
        data[16U] = bytes[2U];
        data[17U] = bytes[3U];
        data[18U] = (data[18U] & 0x0FU) | (bytes[4U] & 0xF0U);

        switch (n) {
        case 0U:
            return 1U;
        case 3U:
            return 2U;
        default:
            return 3U;
        }
    }
    else {
        data[14U] &= 0xF0U;
        data[15U] = 0x00U;
        data[16U] = 0x00U;
        data[17U] = 0x00U;
        data[18U] &= 0x0FU;

        return 0U;
    }
}

/// <summary>Sets link control data.</summary>
/// <param name="lc"></param>
void EmbeddedData::setLC(const lc::LC& lc)
{
    lc.getData(m_data);

    m_FLCO = lc.getFLCO();
    m_valid = true;

    encodeEmbeddedData();
}

/// <summary>Gets link control data.</summary>
/// <returns></returns>
lc::LC* EmbeddedData::getLC() const
{
    if (!m_valid)
        return NULL;

    if (m_FLCO != FLCO_GROUP && m_FLCO != FLCO_PRIVATE)
        return NULL;

    return new lc::LC(m_data);
}

/// <summary>
///
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool EmbeddedData::getRawData(uint8_t* data) const
{
    assert(data != NULL);

    if (!m_valid)
        return false;

    Utils::bitsToByteBE(m_data + 0U, data[0U]);
    Utils::bitsToByteBE(m_data + 8U, data[1U]);
    Utils::bitsToByteBE(m_data + 16U, data[2U]);
    Utils::bitsToByteBE(m_data + 24U, data[3U]);
    Utils::bitsToByteBE(m_data + 32U, data[4U]);
    Utils::bitsToByteBE(m_data + 40U, data[5U]);
    Utils::bitsToByteBE(m_data + 48U, data[6U]);
    Utils::bitsToByteBE(m_data + 56U, data[7U]);
    Utils::bitsToByteBE(m_data + 64U, data[8U]);

    return true;
}

/// <summary>
/// Helper to reset data values to defaults.
/// </summary>
void EmbeddedData::reset()
{
    m_state = LCS_NONE;
    m_valid = false;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Unpack and error check an embedded LC.
/// </summary>
void EmbeddedData::decodeEmbeddedData()
{
    // The data is unpacked downwards in columns
    bool data[128U];
    ::memset(data, 0x00U, 128U * sizeof(bool));

    uint32_t b = 0U;
    for (uint32_t a = 0U; a < 128U; a++) {
        data[b] = m_raw[a];
        b += 16U;
        if (b > 127U)
            b -= 127U;
    }

    // Hamming (16,11,4) check each row except the last one
    for (uint32_t a = 0U; a < 112U; a += 16U) {
        if (!edac::Hamming::decode16114(data + a))
            return;
    }

    // Check the parity bits
    for (uint32_t a = 0U; a < 16U; a++) {
        bool parity = data[a + 0U] ^ data[a + 16U] ^ data[a + 32U] ^ data[a + 48U] ^ data[a + 64U] ^ data[a + 80U] ^ data[a + 96U] ^ data[a + 112U];
        if (parity)
            return;
    }

    // We have passed the Hamming check so extract the actual payload
    b = 0U;
    for (uint32_t a = 0U; a < 11U; a++, b++)
        m_data[b] = data[a];
    for (uint32_t a = 16U; a < 27U; a++, b++)
        m_data[b] = data[a];
    for (uint32_t a = 32U; a < 42U; a++, b++)
        m_data[b] = data[a];
    for (uint32_t a = 48U; a < 58U; a++, b++)
        m_data[b] = data[a];
    for (uint32_t a = 64U; a < 74U; a++, b++)
        m_data[b] = data[a];
    for (uint32_t a = 80U; a < 90U; a++, b++)
        m_data[b] = data[a];
    for (uint32_t a = 96U; a < 106U; a++, b++)
        m_data[b] = data[a];

    // Extract the 5 bit CRC
    uint32_t crc = 0U;
    if (data[42])  crc += 16U;
    if (data[58])  crc += 8U;
    if (data[74])  crc += 4U;
    if (data[90])  crc += 2U;
    if (data[106]) crc += 1U;

    // Now CRC check this
    if (!edac::CRC::checkFiveBit(m_data, crc))
        return;

    m_valid = true;

    // Extract the FLCO
    uint8_t flco;
    Utils::bitsToByteBE(m_data + 0U, flco);
    m_FLCO = flco & 0x3FU;
}

/// <summary>
/// Pack and FEC for an embedded LC.
/// </summary>
void EmbeddedData::encodeEmbeddedData()
{
    uint32_t crc;
    edac::CRC::encodeFiveBit(m_data, crc);

    bool data[128U];
    ::memset(data, 0x00U, 128U * sizeof(bool));

    data[106U] = (crc & 0x01U) == 0x01U;
    data[90U] = (crc & 0x02U) == 0x02U;
    data[74U] = (crc & 0x04U) == 0x04U;
    data[58U] = (crc & 0x08U) == 0x08U;
    data[42U] = (crc & 0x10U) == 0x10U;

    uint32_t b = 0U;
    for (uint32_t a = 0U; a < 11U; a++, b++)
        data[a] = m_data[b];
    for (uint32_t a = 16U; a < 27U; a++, b++)
        data[a] = m_data[b];
    for (uint32_t a = 32U; a < 42U; a++, b++)
        data[a] = m_data[b];
    for (uint32_t a = 48U; a < 58U; a++, b++)
        data[a] = m_data[b];
    for (uint32_t a = 64U; a < 74U; a++, b++)
        data[a] = m_data[b];
    for (uint32_t a = 80U; a < 90U; a++, b++)
        data[a] = m_data[b];
    for (uint32_t a = 96U; a < 106U; a++, b++)
        data[a] = m_data[b];

    // Hamming (16,11,4) check each row except the last one
    for (uint32_t a = 0U; a < 112U; a += 16U)
        edac::Hamming::encode16114(data + a);

    // Add the parity bits for each column
    for (uint32_t a = 0U; a < 16U; a++)
        data[a + 112U] = data[a + 0U] ^ data[a + 16U] ^ data[a + 32U] ^ data[a + 48U] ^ data[a + 64U] ^ data[a + 80U] ^ data[a + 96U];

    // The data is packed downwards in columns
    b = 0U;
    for (uint32_t a = 0U; a < 128U; a++) {
        m_raw[a] = data[b];
        b += 16U;
        if (b > 127U)
            b -= 127U;
    }
}
