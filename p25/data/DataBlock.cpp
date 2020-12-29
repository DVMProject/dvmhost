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
*   Copyright (C) 2018-2019 by Bryan Biedenkapp N2PLL
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
#include "p25/P25Defines.h"
#include "p25/data/DataBlock.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::data;
using namespace p25;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the DataBlock class.
/// </summary>
DataBlock::DataBlock() :
    m_halfRateTrellis(false),
    m_serialNo(0U),
    m_trellis(),
    m_fmt(PDU_FMT_CONFIRMED),
    m_data(NULL)
{
    m_data = new uint8_t[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
}

/// <summary>
/// Finalizes a instance of the DataBlock class.
/// </summary>
DataBlock::~DataBlock()
{
    delete[] m_data;
}

/// <summary>
/// Decodes P25 PDU data block.
/// </summary>
/// <param name="data"></param>
/// <param name="header">Instance of the DataHeader class.</param>
/// <returns>True, if data block was decoded, otherwise false.</returns>
bool DataBlock::decode(const uint8_t* data, const DataHeader header)
{
    assert(data != NULL);
    assert(m_data != NULL);

    uint8_t buffer[P25_PDU_CONFIRMED_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_PDU_CONFIRMED_LENGTH_BYTES);

    // decode 3/4 rate Trellis
    m_halfRateTrellis = false;
    bool valid = m_trellis.decode34(data, buffer);
    if (!valid) {
        // decode 1/2 rate Trellis
        m_halfRateTrellis = true;
        valid = m_trellis.decode12(data, buffer);
        if (!valid) {
            return false;
        }
    }

    // Utils::dump(1U, "PDU Data Block", buffer, P25_PDU_CONFIRMED_LENGTH_BYTES);

    m_fmt = header.getFormat();

    if (m_fmt == PDU_FMT_CONFIRMED) {
        m_serialNo = buffer[0] & 0xFEU;                                                  // Confirmed Data Serial No.
        uint16_t crc = ((buffer[0] & 0x01U) << 8) + buffer[1];                           // CRC-9 Check Sum
        uint32_t count = P25_PDU_CONFIRMED_LENGTH_BYTES;
        if (m_serialNo == (header.getBlocksToFollow() - 1))
            count = P25_PDU_CONFIRMED_LENGTH_BYTES - 4U;

        for (uint32_t i = 2U; i < count; i++) {
            m_data[i - 2U] = buffer[i];                                                  // Payload Data
        }

        // compute CRC-9 for the packet
        ::memset(buffer, 0x00U, P25_PDU_CONFIRMED_LENGTH_BYTES);

        buffer[0U] = (m_serialNo & 0x7FU) << 1;
        ::memcpy(buffer + 1U, m_data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
        uint16_t computedCRC = edac::CRC::crc9(buffer, P25_PDU_CONFIRMED_LENGTH_BYTES);

        LogMessage(LOG_P25, "P25_DUID_PDU, fmt = $%02X, crc = $%04X, computedCRC = $%04X", m_fmt, crc, computedCRC);
    }
    else if ((m_fmt == PDU_FMT_UNCONFIRMED) || (m_fmt == PDU_FMT_RSP)) {
        for (uint32_t i = 0U; i < P25_PDU_UNCONFIRMED_LENGTH_BYTES; i++) {
            m_data[i] = buffer[i];                                                       // Payload Data
        }
    }
    else {
        LogError(LOG_P25, "unknown FMT value in P25_DUID_PDU, fmt = $%02X", m_fmt);
    }

    return true;
}

/// <summary>
/// Encodes a P25 PDU data block.
/// </summary>
/// <param name="data">Buffer to encode data block to.</param>
void DataBlock::encode(uint8_t* data)
{
    assert(data != NULL);
    assert(m_data != NULL);

    if (m_fmt == PDU_FMT_CONFIRMED) {
        uint8_t buffer[P25_PDU_CONFIRMED_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_CONFIRMED_LENGTH_BYTES);

        // compute CRC-9 for the packet
        buffer[0U] = (m_serialNo & 0x7FU) << 1;
        ::memcpy(buffer + 1U, m_data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
        uint16_t crc = edac::CRC::crc9(buffer, P25_PDU_CONFIRMED_LENGTH_BYTES);

        ::memcpy(buffer + 2U, m_data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
        buffer[0] = ((m_serialNo & 0x7FU) << 1) +                                        // Confirmed Data Serial No.
            (crc >> 8);                                                                  // CRC-9 Check Sum (b8)
        buffer[1] = (crc & 0xFFU);                                                       // CRC-9 Check Sum (b0 - b7)

        if (!m_halfRateTrellis) {
            m_trellis.encode34(buffer, data);
        }
        else {
            m_trellis.encode12(buffer, data);
        }
    }
    else if ((m_fmt == PDU_FMT_UNCONFIRMED) || (m_fmt == PDU_FMT_RSP)) {
        uint8_t buffer[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES);

        ::memcpy(buffer, m_data, P25_PDU_UNCONFIRMED_LENGTH_BYTES);

        if (!m_halfRateTrellis) {
            m_trellis.encode34(buffer, data);
        }
        else {
            m_trellis.encode12(buffer, data);
        }
    }
    else {
        LogError(LOG_P25, "unknown FMT value in P25_DUID_PDU, fmt = $%02X", m_fmt);
        return;
    }
}

/// <summary>Sets the data format.</summary>
/// <param name="fmt"></param>
void DataBlock::setFormat(const uint8_t fmt)
{
    m_fmt = fmt;
}

/// <summary>Sets the data format from the data header.</summary>
/// <param name="header"></param>
void DataBlock::setFormat(const DataHeader header)
{
    m_fmt = header.getFormat();
}

/// <summary>Gets the data format.</summary>
/// <returns></returns>
uint8_t DataBlock::getFormat() const
{
    return m_fmt;
}

/// <summary>Sets the raw data stored in the data block.</summary>
/// <param name="buffer"></param>
void DataBlock::setData(const uint8_t* buffer)
{
    assert(buffer != NULL);
    assert(m_data != NULL);

    if (m_fmt == PDU_FMT_CONFIRMED) {
        ::memcpy(m_data, buffer, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
    }
    else if ((m_fmt == PDU_FMT_UNCONFIRMED) || (m_fmt == PDU_FMT_RSP)) {
        ::memcpy(m_data, buffer, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
    }
    else {
        LogError(LOG_P25, "unknown FMT value in P25_DUID_PDU, fmt = $%02X", m_fmt);
    }
}

/// <summary>Gets the raw data stored in the data block.</summary>
/// <returns></returns>
uint32_t DataBlock::getData(uint8_t* buffer) const
{
    assert(buffer != NULL);
    assert(m_data != NULL);

    if (m_fmt == PDU_FMT_CONFIRMED) {
        ::memcpy(buffer, m_data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
        return P25_PDU_CONFIRMED_DATA_LENGTH_BYTES;
    } 
    else if ((m_fmt == PDU_FMT_UNCONFIRMED) || (m_fmt == PDU_FMT_RSP)) {
        ::memcpy(buffer, m_data, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
        return P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }
    else {
        LogError(LOG_P25, "unknown FMT value in P25_DUID_PDU, fmt = $%02X", m_fmt);
        return 0U;
    }
}
