// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/data/DataBlock.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::data;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DataBlock class. */

DataBlock::DataBlock() :
    m_serialNo(0U),
    m_lastBlock(false),
    m_trellis(),
    m_fmt(PDUFormatType::CONFIRMED),
    m_headerSap(0U),
    m_data(nullptr)
{
    m_data = new uint8_t[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
}

/* Finalizes a instance of the DataBlock class. */

DataBlock::~DataBlock()
{
    if (m_data != nullptr)
        delete[] m_data;
}

/* Decodes P25 PDU data block. */

bool DataBlock::decode(const uint8_t* data, const DataHeader& header)
{
    assert(data != nullptr);
    assert(m_data != nullptr);

    uint8_t buffer[P25_PDU_CONFIRMED_LENGTH_BYTES];
    ::memset(buffer, 0x00U, P25_PDU_CONFIRMED_LENGTH_BYTES);

    m_fmt = header.getFormat();
    m_headerSap = header.getSAP();

    // set these to reasonable defaults
    m_serialNo = 0U;
    m_lastBlock = false;

    if (m_fmt == PDUFormatType::CONFIRMED) {
        // decode 3/4 rate Trellis
        try {
            bool valid = m_trellis.decode34(data, buffer);
            if (!valid) {
                LogError(LOG_P25, "DataBlock::decode(), failed to decode Trellis 3/4 rate coding");
                return false;
            }

#if DEBUG_P25_PDU_DATA
            Utils::dump(1U, "P25, DataBlock::decode(), Confirmed PDU Data Block", buffer, P25_PDU_CONFIRMED_LENGTH_BYTES);
#endif

            m_serialNo = (buffer[0] & 0xFEU) >> 1;                                          // Confirmed Data Serial No.
            uint16_t crc = ((buffer[0] & 0x01U) << 8) + buffer[1];                          // CRC-9 Check Sum

            ::memset(m_data, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
            ::memcpy(m_data, buffer + 2U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);             // Payload Data

            uint8_t crcBuffer[P25_PDU_CONFIRMED_LENGTH_BYTES];
            ::memset(crcBuffer, 0x00U, P25_PDU_CONFIRMED_LENGTH_BYTES);

            // generate CRC buffer
            for (uint32_t i = 0U; i < 144U; i++) {
                bool b = READ_BIT(buffer, i);
                if (i < 7U) {
                    WRITE_BIT(crcBuffer, i, b);
                } else if (i > 15U) {
                    WRITE_BIT(crcBuffer, i - 9U, b);
                }
            }

            // compute CRC-9 for the packet
            uint16_t calculated = edac::CRC::createCRC9(crcBuffer, 135U);
            if ((crc ^ calculated) != 0) {
                LogWarning(LOG_P25, "PDU, fmt = $%02X, invalid crc = $%04X != $%04X (computed)", m_fmt, crc, calculated);
            }

#if DEBUG_P25_PDU_DATA
            LogDebug(LOG_P25, "PDU, fmt = $%02X, crc = $%04X, calculated = $%04X", m_fmt, crc, calculated);
#endif
        }
        catch (...) {
            Utils::dump(2U, "P25, decoding excepted with input data", data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
            return false;
        }
    }
    else if ((m_fmt == PDUFormatType::UNCONFIRMED) || (m_fmt == PDUFormatType::RSP) || (m_fmt == PDUFormatType::AMBT)) {
        // decode 1/2 rate Trellis
        try {
            bool valid = m_trellis.decode12(data, buffer);
            if (!valid) {
                LogError(LOG_P25, "DataBlock::decode(), failed to decode Trellis 1/2 rate coding");
                return false;
            }

            ::memset(m_data, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

#if DEBUG_P25_PDU_DATA
            Utils::dump(1U, "P25, DataBlock::decode(), Unconfirmed PDU Data Block", buffer, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
#endif

            ::memcpy(m_data, buffer, P25_PDU_UNCONFIRMED_LENGTH_BYTES);                     // Payload Data
        }
        catch (...) {
            Utils::dump(2U, "P25, DataBlock::decode(), decoding excepted with input data", data, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
            return false;
        }
    }
    else {
        LogError(LOG_P25, "unknown FMT value in PDU, fmt = $%02X", m_fmt);
    }

    return true;
}

/* Encodes a P25 PDU data block. */

void DataBlock::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(m_data != nullptr);

    if (m_fmt == PDUFormatType::CONFIRMED) {
        uint8_t buffer[P25_PDU_CONFIRMED_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_CONFIRMED_LENGTH_BYTES);

        buffer[0U] = ((m_serialNo << 1) & 0xFEU);                                           // Confirmed Data Serial No.

        ::memcpy(buffer + 2U, m_data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);                 // Payload Data

        uint8_t crcBuffer[P25_PDU_CONFIRMED_LENGTH_BYTES];
        ::memset(crcBuffer, 0x00U, P25_PDU_CONFIRMED_LENGTH_BYTES);

        // generate CRC buffer
        for (uint32_t i = 0U; i < 144U; i++) {
            bool b = READ_BIT(buffer, i);
            if (i < 7U) {
                WRITE_BIT(crcBuffer, i, b);
            } else if (i > 15U) {
                WRITE_BIT(crcBuffer, i - 9U, b);
            }
        }

        uint16_t crc = edac::CRC::createCRC9(crcBuffer, 135U);
        buffer[0U] = buffer[0U] + ((crc >> 8) & 0x01U);                                     // CRC-9 Check Sum (b8)
        buffer[1U] = (crc & 0xFFU);                                                         // CRC-9 Check Sum (b0 - b7)

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataBlock::encode(), Confirmed PDU Data Block", buffer, P25_PDU_CONFIRMED_LENGTH_BYTES);
#endif

        m_trellis.encode34(buffer, data);
    }
    else if (m_fmt == PDUFormatType::UNCONFIRMED || m_fmt == PDUFormatType::RSP || m_fmt == PDUFormatType::AMBT) {
        uint8_t buffer[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
        ::memset(buffer, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES);

        ::memcpy(buffer, m_data, P25_PDU_UNCONFIRMED_LENGTH_BYTES);

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataBlock::encode(), Unconfirmed PDU Data Block", buffer, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
#endif

        m_trellis.encode12(buffer, data);
    }
    else {
        LogError(LOG_P25, "unknown FMT value in PDU, fmt = $%02X", m_fmt);
        return;
    }
}

/* Sets the data format. */

void DataBlock::setFormat(const uint8_t fmt)
{
    m_fmt = fmt;
}

/* Sets the data format from the data header. */

void DataBlock::setFormat(const DataHeader& header)
{
    m_fmt = header.getFormat();
}

/* Gets the data format. */

uint8_t DataBlock::getFormat() const
{
    return m_fmt;
}

/* Sets the raw data stored in the data block. */

void DataBlock::setData(const uint8_t* buffer)
{
    assert(buffer != nullptr);
    assert(m_data != nullptr);

    if (m_fmt == PDUFormatType::CONFIRMED) {
        ::memcpy(m_data, buffer, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
    }
    else if (m_fmt == PDUFormatType::UNCONFIRMED || m_fmt == PDUFormatType::RSP || m_fmt == PDUFormatType::AMBT) {
        ::memcpy(m_data, buffer, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
    }
    else {
        LogError(LOG_P25, "unknown FMT value in PDU, fmt = $%02X", m_fmt);
    }
}

/* Gets the raw data stored in the data block. */

uint32_t DataBlock::getData(uint8_t* buffer) const
{
    assert(buffer != nullptr);
    assert(m_data != nullptr);

    if (m_fmt == PDUFormatType::CONFIRMED) {
        ::memcpy(buffer, m_data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
        return P25_PDU_CONFIRMED_DATA_LENGTH_BYTES;
    }
    else if (m_fmt == PDUFormatType::UNCONFIRMED || m_fmt == PDUFormatType::RSP || m_fmt == PDUFormatType::AMBT) {
        ::memcpy(buffer, m_data, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
        return P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }
    else {
        LogError(LOG_P25, "unknown FMT value in PDU, fmt = $%02X", m_fmt);
        return 0U;
    }
}
