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
#include "dmr/DMRDefines.h"
#include "dmr/data/DataBlock.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::data;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DataBlock class. */

DataBlock::DataBlock() :
    m_serialNo(0U),
    m_lastBlock(false),
    m_bptc(),
    m_trellis(),
    m_dataType(DataType::RATE_34_DATA),
    m_DPF(DPF::CONFIRMED_DATA),
    m_data(nullptr)
{
    m_data = new uint8_t[DMR_PDU_UNCODED_LENGTH_BYTES];
}

/* Finalizes a instance of the DataBlock class. */

DataBlock::~DataBlock()
{
    delete[] m_data;
}

/* Decodes DMR PDU data block. */

bool DataBlock::decode(const uint8_t* data, const DataHeader& header)
{
    assert(data != nullptr);
    assert(m_data != nullptr);

    uint8_t buffer[DMR_PDU_UNCODED_LENGTH_BYTES];
    ::memset(buffer, 0x00U, DMR_PDU_UNCODED_LENGTH_BYTES);

    m_DPF = header.getDPF();

    // set these to reasonable defaults
    m_serialNo = 0U;
    m_lastBlock = false;

    if (m_DPF == DPF::CONFIRMED_DATA) {
        try {
            if (m_dataType == DataType::RATE_34_DATA) {
                bool valid = m_trellis.decode34(data, buffer, true);
                if (!valid) {
                    LogError(LOG_DMR, "DataBlock::decode(), failed to decode Trellis 3/4 rate coding");
                    return false;
                }
            }
            else if (m_dataType == DataType::RATE_12_DATA) {
                m_bptc.decode(data, buffer);
            }
            else {
                LogError(LOG_DMR, "DataBlock::decode(), cowardly refusing to decode confirmed full-rate (rate 1) data");
                return false;
            }

#if DEBUG_DMR_PDU_DATA
            Utils::dump(1U, "DMR, DataBlock::decode(), Confirmed PDU Data Block", buffer, DMR_PDU_UNCODED_LENGTH_BYTES);
#endif

            m_serialNo = (buffer[0] & 0xFEU) >> 1;                                          // Confirmed Data Serial No.
            uint16_t crc = ((buffer[0] & 0x01U) << 8) + buffer[1];                          // CRC-9 Check Sum

            ::memset(m_data, 0x00U, DMR_PDU_UNCODED_LENGTH_BYTES);
            if (m_dataType == DataType::RATE_34_DATA)
                ::memcpy(m_data, buffer + 2U, DMR_PDU_CONFIRMED_DATA_LENGTH_BYTES);         // Payload Data
            else if (m_dataType == DataType::RATE_12_DATA)
                ::memcpy(m_data, buffer + 2U, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);            // Payload Data
            else {
                LogError(LOG_DMR, "DataBlock::decode(), failed to decode block, invalid dataType = $%02X", m_dataType);
                return false;
            }

            uint8_t crcBuffer[DMR_PDU_UNCODED_LENGTH_BYTES];
            ::memset(crcBuffer, 0x00U, DMR_PDU_UNCODED_LENGTH_BYTES);

            // generate CRC buffer
            uint32_t bufferLen = DMR_PDU_CONFIRMED_DATA_LENGTH_BYTES;
            if (m_dataType == DataType::RATE_12_DATA)
                bufferLen = DMR_PDU_UNCONFIRMED_LENGTH_BYTES;
            uint32_t crcBitLength = 144U;
            if (m_dataType == DataType::RATE_12_DATA)
                crcBitLength = 96U;

            for (uint32_t i = 16U; i < crcBitLength; i++) {
                bool b = READ_BIT(buffer, i);
                WRITE_BIT(crcBuffer, i - 16U, b);
            }

            for (uint32_t i = 0U; i < 6U; i++) {
                bool b = READ_BIT(buffer, i);
                WRITE_BIT(crcBuffer, i + (crcBitLength - 16U), b);
            }

            // compute CRC-9 for the packet
            uint16_t calculated = edac::CRC::createCRC9(crcBuffer, crcBitLength - 9U);
            calculated = ~calculated & 0x1FFU;

            if ((crc ^ calculated) != 0) {
                LogWarning(LOG_DMR, "DMR, dataType = $%02X, invalid crc = $%04X != $%04X (computed)", m_dataType, crc, calculated);
            }

#if DEBUG_DMR_PDU_DATA
            LogDebug(LOG_DMR, "DMR, dataType = $%02X, crc = $%04X, calculated = $%04X", m_dataType, crc, calculated);
#endif
        }
        catch (...) {
            Utils::dump(2U, "DMR, decoding excepted with input data", data, DMR_PDU_UNCODED_LENGTH_BYTES);
            return false;
        }
    }
    else if (m_DPF == DPF::UNCONFIRMED_DATA || m_DPF == DPF::RESPONSE || m_DPF == DPF::DEFINED_RAW ||
             m_DPF == DPF::DEFINED_SHORT || m_DPF == DPF::UDT) {
        try {
            if (m_dataType == DataType::RATE_34_DATA) {
                bool valid = m_trellis.decode34(data, buffer, true);
                if (!valid) {
                    LogError(LOG_DMR, "DataBlock::decode(), failed to decode Trellis 3/4 rate coding");
                    return false;
                }
            }
            else if (m_dataType == DataType::RATE_12_DATA) {
                m_bptc.decode(data, buffer);
            }
            else {
                ::memcpy(buffer, data, DMR_PDU_UNCODED_LENGTH_BYTES);
                return true; // never do any further processing for uncoded
            }

            ::memset(m_data, 0x00U, DMR_PDU_UNCODED_LENGTH_BYTES);
            if (m_dataType == DataType::RATE_34_DATA)
                ::memcpy(m_data, buffer, DMR_PDU_CONFIRMED_DATA_LENGTH_BYTES);              // Payload Data
            else if (m_dataType == DataType::RATE_12_DATA)
                ::memcpy(m_data, buffer, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);                 // Payload Data
            else {
                LogError(LOG_DMR, "DataBlock::decode(), failed to decode block, invalid dataType = $%02X", m_dataType);
                return false;
            }
        }
        catch (...) {
            Utils::dump(2U, "DMR, decoding excepted with input data", data, DMR_PDU_UNCODED_LENGTH_BYTES);
            return false;
        }
    }
    else {
        LogError(LOG_P25, "unknown DPF value in PDU, dpf = $%02X", m_DPF);
    }

    return true;
}

/* Encodes a DMR PDU data block. */

void DataBlock::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(m_data != nullptr);

    if (m_DPF == DPF::CONFIRMED_DATA) {
        if (m_dataType == DataType::RATE_34_DATA) {
            uint8_t buffer[DMR_PDU_CONFIRMED_LENGTH_BYTES];
            ::memset(buffer, 0x00U, DMR_PDU_CONFIRMED_LENGTH_BYTES);

            buffer[0U] = ((m_serialNo << 1) & 0xFEU);                                           // Confirmed Data Serial No.

            ::memcpy(buffer + 2U, m_data, DMR_PDU_CONFIRMED_DATA_LENGTH_BYTES);                 // Payload Data

            uint8_t crcBuffer[DMR_PDU_UNCODED_LENGTH_BYTES];
            ::memset(crcBuffer, 0x00U, DMR_PDU_UNCODED_LENGTH_BYTES);

            // generate CRC buffer
            uint32_t bufferLen = DMR_PDU_CONFIRMED_DATA_LENGTH_BYTES;
            uint32_t crcBitLength = 144U;

            for (uint32_t i = 2U; i < bufferLen; i++)
                crcBuffer[i - 2U] = buffer[2U];
            for (uint32_t i = 0; i < 6U; i++) {
                bool b = READ_BIT(buffer, i);
                WRITE_BIT(crcBuffer, i + (crcBitLength - 15U), b);
            }

            uint16_t crc = edac::CRC::createCRC9(crcBuffer, 135U);
            buffer[0U] = buffer[0U] + ((crc >> 8) & 0x01U);                                     // CRC-9 Check Sum (b8)
            buffer[1U] = (crc & 0xFFU);                                                         // CRC-9 Check Sum (b0 - b7)

#if DEBUG_DMR_PDU_DATA
            Utils::dump(1U, "DMR, DataBlock::encode(), Confirmed PDU Data Block", buffer, DMR_PDU_CONFIRMED_LENGTH_BYTES);
#endif

            m_trellis.encode34(buffer, data, true);
        }
        else if (m_dataType == DataType::RATE_12_DATA) {
            uint8_t buffer[DMR_PDU_UNCONFIRMED_LENGTH_BYTES];
            ::memset(buffer, 0x00U, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);

            buffer[0U] = ((m_serialNo << 1) & 0xFEU);                                           // Confirmed Data Serial No.

            ::memcpy(buffer + 2U, m_data, DMR_PDU_CONFIRMED_HALFRATE_DATA_LENGTH_BYTES);        // Payload Data

            uint8_t crcBuffer[DMR_PDU_UNCODED_LENGTH_BYTES];
            ::memset(crcBuffer, 0x00U, DMR_PDU_UNCODED_LENGTH_BYTES);

            // generate CRC buffer
            uint32_t bufferLen = DMR_PDU_CONFIRMED_HALFRATE_DATA_LENGTH_BYTES;
            uint32_t crcBitLength = 96U;

            for (uint32_t i = 2U; i < bufferLen; i++)
                crcBuffer[i - 2U] = buffer[2U];
            for (uint32_t i = 0; i < 6U; i++) {
                bool b = READ_BIT(buffer, i);
                WRITE_BIT(crcBuffer, i + (crcBitLength - 15U), b);
            }

            uint16_t crc = edac::CRC::createCRC9(crcBuffer, 87U);
            buffer[0U] = buffer[0U] + ((crc >> 8) & 0x01U);                                     // CRC-9 Check Sum (b8)
            buffer[1U] = (crc & 0xFFU);                                                         // CRC-9 Check Sum (b0 - b7)

            ::memcpy(buffer, m_data, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);

#if DEBUG_DMR_PDU_DATA
            Utils::dump(1U, "DMR, DataBlock::encode(), Unconfirmed PDU Data Block", buffer, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);
#endif

            m_bptc.encode(buffer, data);
        }
        else {
            LogError(LOG_DMR, "DataBlock::encode(), cowardly refusing to encode confirmed full-rate (rate 1) data");
            return;
        }
    }
    else if (m_DPF == DPF::UNCONFIRMED_DATA || m_DPF == DPF::RESPONSE || m_DPF == DPF::DEFINED_RAW ||
             m_DPF == DPF::DEFINED_SHORT || m_DPF == DPF::UDT) {
        if (m_dataType == DataType::RATE_34_DATA) {
            uint8_t buffer[DMR_PDU_CONFIRMED_LENGTH_BYTES];
            ::memset(buffer, 0x00U, DMR_PDU_CONFIRMED_LENGTH_BYTES);

            ::memcpy(buffer, m_data, DMR_PDU_CONFIRMED_LENGTH_BYTES);

#if DEBUG_DMR_PDU_DATA
            Utils::dump(1U, "DMR, DataBlock::encode(), Unconfirmed PDU Data Block", buffer, DMR_PDU_CONFIRMED_LENGTH_BYTES);
#endif

            m_trellis.encode34(buffer, data, true);
        }
        else if (m_dataType == DataType::RATE_12_DATA) {
            uint8_t buffer[DMR_PDU_UNCONFIRMED_LENGTH_BYTES];
            ::memset(buffer, 0x00U, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);

            ::memcpy(buffer, m_data, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);

#if DEBUG_DMR_PDU_DATA
            Utils::dump(1U, "DMR, DataBlock::encode(), Unconfirmed PDU Data Block", buffer, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);
#endif

            m_bptc.encode(buffer, data);
        }
        else {
            uint8_t buffer[DMR_PDU_UNCODED_LENGTH_BYTES];
            ::memset(buffer, 0x00U, DMR_PDU_UNCODED_LENGTH_BYTES);

            ::memcpy(buffer, m_data, DMR_PDU_UNCODED_LENGTH_BYTES);

            ::memcpy(data, buffer, DMR_PDU_UNCODED_LENGTH_BYTES);
        }
    }
    else {
        LogError(LOG_P25, "unknown DPF value in PDU, dpf = $%02X", m_DPF);
        return;
    }
}

/* Sets the data type. */

void DataBlock::setDataType(const DataType::E& dataType)
{
    m_dataType = dataType;
}

/* Gets the data type. */

DataType::E DataBlock::getDataType() const
{
    return m_dataType;
}

/* Sets the data format. */

void DataBlock::setFormat(const DPF::E fmt)
{
    m_DPF = fmt;
}

/* Sets the data format from the data header. */

void DataBlock::setFormat(const DataHeader& header)
{
    m_DPF = header.getDPF();
}

/* Gets the data format. */

DPF::E DataBlock::getFormat() const
{
    return m_DPF;
}

/* Sets the raw data stored in the data block. */

void DataBlock::setData(const uint8_t* buffer)
{
    assert(buffer != nullptr);
    assert(m_data != nullptr);

    if (m_dataType == DataType::RATE_34_DATA)
        ::memcpy(m_data, buffer, DMR_PDU_CONFIRMED_DATA_LENGTH_BYTES);
    else if (m_dataType == DataType::RATE_12_DATA)
        ::memcpy(m_data, buffer, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);
    else if (m_dataType == DataType::RATE_1_DATA)
        ::memcpy(m_data, buffer, DMR_PDU_UNCODED_LENGTH_BYTES);
    else
        LogError(LOG_DMR, "unknown dataType value in PDU, dataType = $%02X", m_dataType);
}

/* Gets the raw data stored in the data block. */

uint32_t DataBlock::getData(uint8_t* buffer) const
{
    assert(buffer != nullptr);
    assert(m_data != nullptr);

    if (m_dataType == DataType::RATE_34_DATA) {
        ::memcpy(buffer, m_data, DMR_PDU_CONFIRMED_DATA_LENGTH_BYTES);
        return DMR_PDU_CONFIRMED_DATA_LENGTH_BYTES;
    } else if (m_dataType == DataType::RATE_12_DATA) {
        ::memcpy(buffer, m_data, DMR_PDU_UNCONFIRMED_LENGTH_BYTES);
        return DMR_PDU_UNCONFIRMED_LENGTH_BYTES;
    } else if (m_dataType == DataType::RATE_1_DATA) {
        ::memcpy(buffer, m_data, DMR_PDU_UNCODED_LENGTH_BYTES);
        return DMR_PDU_UNCODED_LENGTH_BYTES;
    } else {
        LogError(LOG_DMR, "unknown dataType value in PDU, dataType = $%02X", m_dataType);
        return 0U;
    }
}
