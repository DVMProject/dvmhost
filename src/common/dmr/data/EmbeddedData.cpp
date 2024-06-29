// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/data/EmbeddedData.h"
#include "edac/Hamming.h"
#include "edac/CRC.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::data;

#include <cassert>
#include <cstring>
#include <memory>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the EmbeddedData class. */

EmbeddedData::EmbeddedData() :
    m_valid(false),
    m_FLCO(FLCO::GROUP),
    m_state(LCS_NONE),
    m_data(nullptr),
    m_raw(nullptr)
{
    m_raw = new bool[128U];
    m_data = new bool[72U];
}

/* Finalizes a instance of the EmbeddedData class. */

EmbeddedData::~EmbeddedData()
{
    delete[] m_raw;
    delete[] m_data;
}

/* Add LC data (which may consist of 4 blocks) to the data store. */

bool EmbeddedData::addData(const uint8_t* data, uint8_t lcss)
{
    assert(data != nullptr);

    bool rawData[40U];
    Utils::byteToBitsBE(data[14U], rawData + 0U);
    Utils::byteToBitsBE(data[15U], rawData + 8U);
    Utils::byteToBitsBE(data[16U], rawData + 16U);
    Utils::byteToBitsBE(data[17U], rawData + 24U);
    Utils::byteToBitsBE(data[18U], rawData + 32U);

    // is this the first block of a 4 block embedded LC ?
    if (lcss == 1U) {
        for (uint32_t a = 0U; a < 32U; a++)
            m_raw[a] = rawData[a + 4U];

        // show we are ready for the next LC block
        m_state = LCS_FIRST;
        m_valid = false;

        return false;
    }

    // is this the 2nd block of a 4 block embedded LC ?
    if (lcss == 3U && m_state == LCS_FIRST) {
        for (uint32_t a = 0U; a < 32U; a++)
            m_raw[a + 32U] = rawData[a + 4U];

        // show we are ready for the next LC block
        m_state = LCS_SECOND;

        return false;
    }

    // is this the 3rd block of a 4 block embedded LC ?
    if (lcss == 3U && m_state == LCS_SECOND) {
        for (uint32_t a = 0U; a < 32U; a++)
            m_raw[a + 64U] = rawData[a + 4U];

        // show we are ready for the final LC block
        m_state = LCS_THIRD;

        return false;
    }

    // is this the final block of a 4 block embedded LC ?
    if (lcss == 2U && m_state == LCS_THIRD) {
        for (uint32_t a = 0U; a < 32U; a++)
            m_raw[a + 96U] = rawData[a + 4U];

        // show that we're not ready for any more data
        m_state = LCS_NONE;

        // process the complete data block
        decodeEmbeddedData();
        if (m_valid)
            encodeEmbeddedData();

        return m_valid;
    }

    return false;
}

/* Get LC data from the data store. */

uint8_t EmbeddedData::getData(uint8_t* data, uint8_t n) const
{
    assert(data != nullptr);

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

/* Sets link control data. */

void EmbeddedData::setLC(const lc::LC& lc)
{
    lc.getData(m_data);

    m_FLCO = lc.getFLCO();
    m_valid = true;

    encodeEmbeddedData();
}

/* Gets link control data. */

std::unique_ptr<lc::LC> EmbeddedData::getLC() const
{
    if (!m_valid)
        return nullptr;

    if (m_FLCO != FLCO::GROUP && m_FLCO != FLCO::PRIVATE)
        return nullptr;

    return std::make_unique<lc::LC>(m_data);
}

/* Get raw embedded data buffer. */

bool EmbeddedData::getRawData(uint8_t* data) const
{
    assert(data != nullptr);

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

/* Helper to reset data values to defaults. */

void EmbeddedData::reset()
{
    m_state = LCS_NONE;
    m_valid = false;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Unpack and error check an embedded LC. */

void EmbeddedData::decodeEmbeddedData()
{
    // the data is unpacked downwards in columns
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

    // check the parity bits
    for (uint32_t a = 0U; a < 16U; a++) {
        bool parity = data[a + 0U] ^ data[a + 16U] ^ data[a + 32U] ^ data[a + 48U] ^ data[a + 64U] ^ data[a + 80U] ^ data[a + 96U] ^ data[a + 112U];
        if (parity)
            return;
    }

    // we have passed the Hamming check so extract the actual payload
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

    // extract the 5 bit CRC
    uint32_t crc = 0U;
    if (data[42])  crc += 16U;
    if (data[58])  crc += 8U;
    if (data[74])  crc += 4U;
    if (data[90])  crc += 2U;
    if (data[106]) crc += 1U;

    // now CRC check this
    if (!edac::CRC::checkFiveBit(m_data, crc))
        return;

    m_valid = true;

    // extract the FLCO
    uint8_t flco;
    Utils::bitsToByteBE(m_data + 0U, flco);
    m_FLCO = (FLCO::E)(flco & 0x3FU);
}

/* Pack and FEC for an embedded LC. */

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

    // add the parity bits for each column
    for (uint32_t a = 0U; a < 16U; a++)
        data[a + 112U] = data[a + 0U] ^ data[a + 16U] ^ data[a + 32U] ^ data[a + 48U] ^ data[a + 64U] ^ data[a + 80U] ^ data[a + 96U];

    // the data is packed downwards in columns
    b = 0U;
    for (uint32_t a = 0U; a < 128U; a++) {
        m_raw[a] = data[b];
        b += 16U;
        if (b > 127U)
            b -= 127U;
    }
}
