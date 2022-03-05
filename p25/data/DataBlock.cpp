/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2018-2022 by Bryan Biedenkapp N2PLL
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
    m_serialNo(0U),
    m_lastBlock(false),
    m_llId(0U),
    m_sap(0U),
    m_trellis(),
    m_fmt(PDU_FMT_CONFIRMED),
    m_headerSap(0U),
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

    m_fmt = header.getFormat();
    m_headerSap = header.getSAP();

    if (m_fmt == PDU_FMT_CONFIRMED) {
        // decode 3/4 rate Trellis
        try {
            bool valid = m_trellis.decode34(data, buffer);
            if (!valid) {
                LogError(LOG_P25, "DataBlock::decode(), failed to decode Trellis 3/4 rate coding");
                return false;
            }

            // determine the number of user data bytes
            uint32_t count = P25_PDU_CONFIRMED_DATA_LENGTH_BYTES;
            if ((m_serialNo == (header.getBlocksToFollow() - 1) && header.getBlocksToFollow() > 1) ||
                (m_headerSap == PDU_SAP_EXT_ADDR && m_serialNo == 0U)) {
                m_lastBlock = true;
            } else {
                if (header.getBlocksToFollow() <= 1) {
                    m_lastBlock = true;
                }
            }

#if DEBUG_P25_PDU_DATA
            Utils::dump(1U, "P25, DataBlock::decode(), PDU Confirmed Data Block", buffer, P25_PDU_CONFIRMED_LENGTH_BYTES);
#endif

            m_serialNo = (buffer[0] & 0xFEU) >> 1;                                       // Confirmed Data Serial No.
            uint16_t crc = ((buffer[0] & 0x01U) << 8) + buffer[1];                       // CRC-9 Check Sum

            ::memset(m_data, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

            // if this is extended addressing and the first block decode the SAP and LLId
            if (m_headerSap == PDU_SAP_EXT_ADDR && m_serialNo == 0U) {
                count = P25_PDU_CONFIRMED_DATA_LENGTH_BYTES - 4U;                

                m_sap = buffer[5U] & 0x3FU;                                              // Service Access Point
                m_llId = (buffer[2U] << 16) + (buffer[3U] << 8) + buffer[4U];            // Logical Link ID

                ::memcpy(m_data, buffer + 6U, count);                                    // Payload Data
            }
            else {
                ::memcpy(m_data, buffer + 2U, count);                                    // Payload Data
             }

#if DEBUG_P25_PDU_DATA
            Utils::dump(1U, "P25, DataBlock::decode(), Confirmed PDU Block Data", m_data, count);
#endif

            // compute CRC-9 for the packet
            uint16_t calculated = edac::CRC::crc9(buffer, 144U);
            if (((crc ^ calculated) != 0) && ((crc ^ calculated) != 0x1FFU)) {
                LogWarning(LOG_P25, "P25_DUID_PDU, fmt = $%02X, invalid crc = $%04X != $%04X (computed)", m_fmt, crc, calculated);
            }

#if DEBUG_P25_PDU_DATA
            LogDebug(LOG_P25, "P25_DUID_PDU, fmt = $%02X, crc = $%04X, calculated = $%04X", m_fmt, crc, calculated);
#endif
        }
        catch (...) {
            Utils::dump(2U, "P25, decoding excepted with input data", data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
            return false;
        }
    }
    else if ((m_fmt == PDU_FMT_UNCONFIRMED) || (m_fmt == PDU_FMT_RSP) || (m_fmt == PDU_FMT_AMBT)) {
        // decode 1/2 rate Trellis
        try {
            bool valid = m_trellis.decode12(data, buffer);
            if (!valid) {
                LogError(LOG_P25, "DataBlock::decode(), failed to decode Trellis 1/2 rate coding");
                return false;
            }

            ::memset(m_data, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

#if DEBUG_P25_PDU_DATA
            Utils::dump(1U, "P25, DataBlock::decode(), PDU Unconfirmed Data Block", buffer, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
#endif

            ::memcpy(m_data, buffer, P25_PDU_UNCONFIRMED_LENGTH_BYTES);                      // Payload Data
        }
        catch (...) {
            Utils::dump(2U, "P25, decoding excepted with input data", data, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
            return false;
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

        buffer[0U] = ((m_serialNo << 1) & 0xFEU);                                       // Confirmed Data Serial No.

        // if this is extended addressing and the first block decode the SAP and LLId
        if (m_headerSap == PDU_SAP_EXT_ADDR && m_serialNo == 0U) {
            buffer[5U] = m_sap & 0x3FU;                                                 // Service Access Point

            buffer[2U] = (m_llId >> 16) & 0xFFU;                                        // Logical Link ID
            buffer[3U] = (m_llId >> 8) & 0xFFU;
            buffer[4U] = (m_llId >> 0) & 0xFFU;

            ::memcpy(buffer + 6U, m_data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES - 4U);
        }
        else {
            ::memcpy(buffer + 2U, m_data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
        }

        uint16_t crc = edac::CRC::crc9(buffer, 144U);
        buffer[0U] = buffer[0U] + ((crc >> 8) & 0x01U);                                 // CRC-9 Check Sum (b8)
        buffer[1U] = (crc & 0xFFU);                                                     // CRC-9 Check Sum (b0 - b7)

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataBlock::encode(), PDU Confirmed Data Block", buffer, P25_PDU_CONFIRMED_LENGTH_BYTES);
#endif

        m_trellis.encode34(buffer, data);
    }
    else if (m_fmt == PDU_FMT_UNCONFIRMED || m_fmt == PDU_FMT_RSP || m_fmt == PDU_FMT_AMBT) {
        uint8_t buffer[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES);

        ::memcpy(buffer, m_data, P25_PDU_UNCONFIRMED_LENGTH_BYTES);

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataBlock::encode(), PDU Unconfirmed Data Block", buffer, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
#endif

        m_trellis.encode12(buffer, data);
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
    else if (m_fmt == PDU_FMT_UNCONFIRMED || m_fmt == PDU_FMT_RSP || m_fmt == PDU_FMT_AMBT) {
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
    else if (m_fmt == PDU_FMT_UNCONFIRMED || m_fmt == PDU_FMT_RSP || m_fmt == PDU_FMT_AMBT) {
        ::memcpy(buffer, m_data, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
        return P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }
    else {
        LogError(LOG_P25, "unknown FMT value in P25_DUID_PDU, fmt = $%02X", m_fmt);
        return 0U;
    }
}
