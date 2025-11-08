// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018,2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/data/DataHeader.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::data;

#include <cassert>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

#if FORCE_TSBK_CRC_WARN
bool DataHeader::s_warnCRC = true;
#else
bool DataHeader::s_warnCRC = false;
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a copy instance of the DataHeader class. */

DataHeader::DataHeader(const DataHeader& data) : DataHeader()
{
    copy(data);
}

/* Initializes a new instance of the DataHeader class. */

DataHeader::DataHeader() :
    m_ackNeeded(false),
    m_outbound(false),
    m_fmt(PDUFormatType::CONFIRMED),
    m_sap(PDUSAP::USER_DATA),
    m_mfId(MFG_STANDARD),
    m_llId(0U),
    m_blocksToFollow(0U),
    m_padLength(0U),
    m_F(true),
    m_S(false),
    m_fsn(0U),
    m_Ns(0U),
    m_lastFragment(true),
    m_headerOffset(0U),
    m_exSap(PDUSAP::USER_DATA),
    m_srcLlId(0U),
    m_rspClass(PDUAckClass::NACK),
    m_rspType(PDUAckType::NACK_ILLEGAL),
    m_rspStatus(0U),
    m_ambtOpcode(0U),
    m_ambtField8(0U),
    m_ambtField9(0U),
    m_algId(ALGO_UNENCRYPT),
    m_kId(0U),
    m_trellis(),
    m_data(nullptr),
    m_extAddrData(nullptr),
    m_auxESData(nullptr),
    m_mi(nullptr)
{
    m_data = new uint8_t[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(m_data, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);
    m_extAddrData = new uint8_t[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(m_extAddrData, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);
    m_auxESData = new uint8_t[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
    ::memset(m_auxESData, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

    m_mi = new uint8_t[MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
}

/* Finalizes a instance of the DataHeader class. */

DataHeader::~DataHeader()
{
    if (m_data != nullptr) {
        delete[] m_data;
        m_data = nullptr;
    }

    if (m_extAddrData != nullptr) {
        delete[] m_extAddrData;
        m_extAddrData = nullptr;
    }

    if (m_auxESData != nullptr) {
        delete[] m_auxESData;
        m_auxESData = nullptr;
    }

    if (m_mi != nullptr) {
        delete[] m_mi;
        m_mi = nullptr;
    }
}

/* Decodes P25 PDU data header. */

bool DataHeader::decode(const uint8_t* data, bool noTrellis)
{
    assert(data != nullptr);

    // decode 1/2 rate Trellis & check CRC-CCITT 16
    bool valid = true;
    if (noTrellis) {
        ::memcpy(m_data, data, P25_PDU_HEADER_LENGTH_BYTES);
    }
    else {
        valid = m_trellis.decode12(data, m_data);
    }

    if (valid) {
        valid = edac::CRC::checkCCITT162(m_data, P25_PDU_HEADER_LENGTH_BYTES);
        if (!valid) {
            if (s_warnCRC) {
                // if we're already warning instead of erroring CRC, don't announce invalid CRC in the 
                // case where no CRC is defined
                if ((m_data[P25_PDU_HEADER_LENGTH_BYTES - 2U] != 0x00U) && (m_data[P25_PDU_HEADER_LENGTH_BYTES - 1U] != 0x00U)) {
                    LogWarning(LOG_P25, "DataHeader::decode(), failed CRC CCITT-162 check");
                }

                valid = true; // ignore CRC error
            }
            else {
                LogError(LOG_P25, "DataHeader::decode(), failed CRC CCITT-162 check");
            }
        }
    }

    if (!valid) {
        return false;
    }

#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, DataHeader::decode(), PDU Header Data", m_data, P25_PDU_HEADER_LENGTH_BYTES);
#endif

    m_ackNeeded = (m_data[0U] & 0x40U) == 0x40U;                                // Acknowledge Needed
    m_outbound = (m_data[0U] & 0x20U) == 0x20U;                                 // Inbound/Outbound
    m_fmt = m_data[0U] & 0x1FU;                                                 // Packet Format

    m_sap = m_data[1U] & 0x3FU;                                                 // Service Access Point

    m_mfId = m_data[2U];                                                        // Mfg Id.

    m_llId = (m_data[3U] << 16) + (m_data[4U] << 8) + m_data[5U];               // Logical Link ID

    m_F = (m_data[6U] & 0x80U) == 0x80U;                                        // Full Message Flag
    m_blocksToFollow = m_data[6U] & 0x7FU;                                      // Block Frames to Follow

    m_padLength = m_data[7U] & 0x1FU;                                           // Pad Byte Count
    if (m_fmt == PDUFormatType::RSP || m_fmt == PDUFormatType::AMBT) {
        m_padLength = 0U;
    }

    switch (m_fmt) {
    case PDUFormatType::CONFIRMED:
        m_S = (m_data[8U] & 0x80U) == 0x80U;                                    // Re-synchronize Flag

        m_Ns = (m_data[8U] >> 4) & 0x07U;                                       // Packet Sequence No.
        m_fsn = m_data[8U] & 0x07U;                                             // Fragment Sequence No.
        m_lastFragment = (m_data[8U] & 0x08U) == 0x08U;                         // Last Fragment Flag

        m_headerOffset = m_data[9U] & 0x3FU;                                    // Data Header Offset
        break;
    case PDUFormatType::RSP:
        m_ackNeeded = false;
        m_sap = PDUSAP::USER_DATA;
        m_rspClass = (m_data[1U] >> 6) & 0x03U;                                 // Response Class
        m_rspType = (m_data[1U] >> 3) & 0x07U;                                  // Response Type
        m_rspStatus = m_data[1U] & 0x07U;                                       // Response Status
        if (!m_F) {
            m_srcLlId = (m_data[7U] << 16) + (m_data[8U] << 8) + m_data[9U];    // Source Logical Link ID
        }
        break;
    case PDUFormatType::AMBT:
        m_ambtOpcode = m_data[7U] & 0x3FU;                                      // AMBT Opcode
        m_ambtField8 = m_data[8U];                                              // AMBT Field 8
        m_ambtField9 = m_data[9U];                                              // AMBT Field 9
        // fall-thru
    case PDUFormatType::UNCONFIRMED:
    default:
        m_ackNeeded = false;
        m_S = false;

        m_Ns = 0U;
        m_fsn = 0U;
        m_headerOffset = 0U;
        break;
    }

    return true;
}

/* Encodes P25 PDU data header. */

void DataHeader::encode(uint8_t* data, bool noTrellis)
{
    assert(data != nullptr);

    uint8_t header[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(header, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    if (m_fmt == PDUFormatType::UNCONFIRMED || m_fmt == PDUFormatType::RSP) {
        m_ackNeeded = false;
    }

    if (m_fmt == PDUFormatType::CONFIRMED && !m_ackNeeded) {
        LogWarning(LOG_P25, "DataHeader::encode(), invalid values for confirmed PDU, ackNeeded = %u", m_ackNeeded);
        m_ackNeeded = true; // force set this to true
    }

    header[0U] = (m_ackNeeded ? 0x40U : 0x00U) +                                // Acknowledge Needed
        (m_outbound ? 0x20U : 0x00U) +                                          // Inbound/Outbound
        (m_fmt & 0x1FU);                                                        // Packet Format

    header[1U] = m_sap & 0x3FU;                                                 // Service Access Point
    header[1U] |= 0xC0U;

    header[2U] = m_mfId;                                                        // Mfg Id.

    header[3U] = (m_llId >> 16) & 0xFFU;                                        // Logical Link ID
    header[4U] = (m_llId >> 8) & 0xFFU;
    header[5U] = (m_llId >> 0) & 0xFFU;

    header[6U] = (m_F ? 0x80U : 0x00U) +                                        // Full Message Flag
        (m_blocksToFollow & 0x7FU);                                             // Blocks Frames to Follow

    switch (m_fmt) {
    case PDUFormatType::CONFIRMED:
        header[7U] = (m_padLength & 0x1FU);                                     // Pad Byte Count
        header[8U] = (m_S ? 0x80U : 0x00U) +                                    // Re-synchronize Flag
            ((m_Ns & 0x07U) << 4) +                                             // Packet Sequence No.
            (m_lastFragment ? 0x08U : 0x00U) +                                  // Last Fragment Flag
            (m_fsn & 0x07);                                                     // Fragment Sequence No.

        header[9U] = m_headerOffset & 0x3FU;                                    // Data Header Offset
        break;
    case PDUFormatType::RSP:
        header[1U] = ((m_rspClass & 0x03U) << 6) +                              // Response Class
            ((m_rspType & 0x07U) << 3) +                                        // Response Type
            ((m_rspStatus & 0x07U));                                            // Response Status

        // the "full message" flag in a response PDU indicates whether or not the response is to an
        // extended addressing PDU
        if (!m_F) {
            header[7U] = (m_srcLlId >> 16) & 0xFFU;                             // Source Logical Link ID
            header[8U] = (m_srcLlId >> 8) & 0xFFU;
            header[9U] = (m_srcLlId >> 0) & 0xFFU;
        }
        break;
    case PDUFormatType::AMBT:
        header[7U] = (m_ambtOpcode & 0x3FU);                                    // AMBT Opcode
        header[8U] = m_ambtField8;                                              // AMBT Field 8
        header[9U] = m_ambtField9;                                              // AMBT Field 9
        break;
    case PDUFormatType::UNCONFIRMED:
    default:
        header[7U] = (m_padLength & 0x1FU);                                     // Pad Byte Count
        header[8U] = 0x00U;
        header[9U] = m_headerOffset & 0x3FU;                                    // Data Header Offset
        break;
    }

    // compute CRC-CCITT 16
    edac::CRC::addCCITT162(header, P25_PDU_HEADER_LENGTH_BYTES);

#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, DataHeader::encode(), PDU Header Data", header, P25_PDU_HEADER_LENGTH_BYTES);
#endif

    if (!noTrellis) {
        // encode 1/2 rate Trellis
        m_trellis.encode12(header, data);
    } else {
        ::memcpy(data, header, P25_PDU_HEADER_LENGTH_BYTES);
    }
}

/* Decodes P25 PDU extended addressing header. */

bool DataHeader::decodeExtAddr(const uint8_t* data)
{
    assert(data != nullptr);

    ::memset(m_extAddrData, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    if (m_sap != PDUSAP::EXT_ADDR)
        return false;

    if (m_fmt == PDUFormatType::CONFIRMED) {
        ::memcpy(m_extAddrData, data, 4U);
#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataHeader::decodeExtAddr(), PDU Extended Address Data", m_extAddrData, 4U);
#endif
        m_exSap = m_extAddrData[3U] & 0x3FU;                                    // Service Access Point
        m_srcLlId = (m_extAddrData[0U] << 16) + (m_extAddrData[1U] << 8) +      // Source Logical Link ID
            m_extAddrData[2U];        
    } else if (m_fmt == PDUFormatType::UNCONFIRMED) {
        ::memcpy(m_extAddrData, data, P25_PDU_HEADER_LENGTH_BYTES);

        // check CRC-CCITT 16
        bool valid = edac::CRC::checkCCITT162(m_extAddrData, P25_PDU_HEADER_LENGTH_BYTES);
        if (!valid) {
            if (s_warnCRC) {
                // if we're already warning instead of erroring CRC, don't announce invalid CRC in the 
                // case where no CRC is defined
                if ((m_extAddrData[P25_PDU_HEADER_LENGTH_BYTES - 2U] != 0x00U) && (m_extAddrData[P25_PDU_HEADER_LENGTH_BYTES - 1U] != 0x00U)) {
                    LogWarning(LOG_P25, "DataHeader::decodeExtAddr(), failed CRC CCITT-162 check");
                }

                valid = true; // ignore CRC error
            }
            else {
                LogError(LOG_P25, "DataHeader::decodeExtAddr(), failed CRC CCITT-162 check");
            }
        }

        if (!valid) {
            return false;
        }

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataHeader::decodeExtAddr(), PDU Extended Address Data", m_extAddrData, P25_PDU_HEADER_LENGTH_BYTES);
#endif

        m_exSap = m_extAddrData[1U] & 0x3FU;                                    // Service Access Point
        m_srcLlId = (m_extAddrData[3U] << 16) + (m_extAddrData[4U] << 8) +      // Source Logical Link ID
            m_extAddrData[5U];
    }

    return true;
}

/* Encodes P25 PDU extended addressing header. */

void DataHeader::encodeExtAddr(uint8_t* data)
{
    assert(data != nullptr);

    if (m_sap != PDUSAP::EXT_ADDR)
        return;

    uint8_t header[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(header, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    if (m_fmt == PDUFormatType::CONFIRMED) {
        header[3U] = m_exSap & 0x3FU;                                           // Service Access Point
        header[3U] |= 0xC0U;

        header[0U] = (m_srcLlId >> 16) & 0xFFU;                                 // Source Logical Link ID
        header[1U] = (m_srcLlId >> 8) & 0xFFU;
        header[2U] = (m_srcLlId >> 0) & 0xFFU;

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataHeader::encodeExtAddr(), PDU Extended Address Data", header, P25_PDU_HEADER_LENGTH_BYTES);
#endif

        ::memcpy(data, header, 4U); // only copy the 4 bytes of header data for confirmed
    } else if (m_fmt == PDUFormatType::UNCONFIRMED) {
        header[0U] = m_fmt & 0x1FU;                                             // Packet Format

        header[1U] = m_exSap & 0x3FU;                                           // Service Access Point
        header[1U] |= 0xC0U;

        //header[2U] = m_mfId;                                                    // Mfg Id.

        header[3U] = (m_srcLlId >> 16) & 0xFFU;                                 // Source Logical Link ID
        header[4U] = (m_srcLlId >> 8) & 0xFFU;
        header[5U] = (m_srcLlId >> 0) & 0xFFU;

        // compute CRC-CCITT 16
        edac::CRC::addCCITT162(header, P25_PDU_HEADER_LENGTH_BYTES);

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataHeader::encodeExtAddr(), PDU Extended Address Data", header, P25_PDU_HEADER_LENGTH_BYTES);
#endif
        ::memcpy(data, header, P25_PDU_HEADER_LENGTH_BYTES);
    }
}

/* Decodes P25 PDU auxiliary ES header. */

bool DataHeader::decodeAuxES(const uint8_t* data)
{
    assert(data != nullptr);

    ::memset(m_auxESData, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

    if (m_sap != PDUSAP::ENC_USER_DATA && m_sap != PDUSAP::ENC_KMM)
        return false;

    if (m_fmt == PDUFormatType::CONFIRMED) {
        ::memcpy(m_auxESData, data, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataHeader::decodeAuxES(), PDU Auxiliary ES Data", m_auxESData, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
#endif

        m_algId = m_auxESData[9U];                                              // Algorithm ID
        if (m_algId != ALGO_UNENCRYPT) {
            if (m_mi != nullptr)
                delete[] m_mi;
            m_mi = new uint8_t[MI_LENGTH_BYTES];
            ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
            ::memcpy(m_mi, m_auxESData, MI_LENGTH_BYTES);                       // Message Indicator

            m_kId = (m_auxESData[10U] << 8) + m_auxESData[11U];                 // Key ID
        }
        else {
            if (m_mi != nullptr)
                delete[] m_mi;
            m_mi = new uint8_t[MI_LENGTH_BYTES];
            ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);

            m_kId = 0x0000U;
        }

        m_exSap = m_auxESData[12U] & 0x3FU;                                     // Service Access Point
    } else if (m_fmt == PDUFormatType::UNCONFIRMED) {
        ::memcpy(m_auxESData, data, P25_PDU_HEADER_LENGTH_BYTES);

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataHeader::decodeAuxES(), PDU Auxiliary ES Data", m_auxESData, P25_PDU_HEADER_LENGTH_BYTES);
#endif

        m_algId = m_auxESData[9U];                                              // Algorithm ID
        if (m_algId != ALGO_UNENCRYPT) {
            if (m_mi != nullptr)
                delete[] m_mi;
            m_mi = new uint8_t[MI_LENGTH_BYTES];
            ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
            ::memcpy(m_mi, m_auxESData, MI_LENGTH_BYTES);                       // Message Indicator

            m_kId = (m_auxESData[10U] << 8) + m_auxESData[11U];                 // Key ID
        }
        else {
            if (m_mi != nullptr)
                delete[] m_mi;
            m_mi = new uint8_t[MI_LENGTH_BYTES];
            ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);

            m_kId = 0x0000U;
        }
    }

    return true;
}

/* Encodes P25 PDU auxiliary ES header. */

void DataHeader::encodeAuxES(uint8_t* data)
{
    assert(data != nullptr);

    if (m_sap != PDUSAP::ENC_USER_DATA && m_sap != PDUSAP::ENC_KMM)
        return;

    uint8_t header[P25_PDU_CONFIRMED_DATA_LENGTH_BYTES];
    ::memset(header, 0x00U, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);

    if (m_fmt == PDUFormatType::CONFIRMED) {
        for (uint32_t i = 0; i < MI_LENGTH_BYTES; i++)
            header[i] = m_mi[i];                                                // Message Indicator

        header[9U] = m_algId;                                                   // Algorithm ID
        header[10U] = (m_kId >> 8) & 0xFFU;                                     // Key ID
        header[11U] = (m_kId >> 0) & 0xFFU;                                     // ...

        header[12U] = m_exSap & 0x3FU;                                          // Service Access Point

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataHeader::encodeExtAddr(), PDU Auxiliary ES Data", header, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
#endif

        ::memcpy(data, header, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
    } else if (m_fmt == PDUFormatType::UNCONFIRMED) {
        for (uint32_t i = 0; i < MI_LENGTH_BYTES; i++)
            header[i] = m_mi[i];                                                        // Message Indicator

        header[9U] = m_algId;                                                           // Algorithm ID
        header[10U] = (m_kId >> 8) & 0xFFU;                                             // Key ID
        header[11U] = (m_kId >> 0) & 0xFFU;                                             // ...

        header[12U] = m_exSap & 0x3FU;                                          // Service Access Point

#if DEBUG_P25_PDU_DATA
        Utils::dump(1U, "P25, DataHeader::encodeAuxES(), PDU Auxiliary ES Data", header, P25_PDU_HEADER_LENGTH_BYTES);
#endif
        ::memcpy(data, header, P25_PDU_HEADER_LENGTH_BYTES + 1U);
    }
}

/* Helper to reset data values to defaults. */

void DataHeader::reset()
{
    if (m_data == nullptr)
        return; // bail bail bail

    m_ackNeeded = false;
    m_outbound = false;

    m_fmt = PDUFormatType::CONFIRMED;

    m_sap = PDUSAP::USER_DATA;
    m_mfId = MFG_STANDARD;
    m_llId = 0U;

    m_F = true;
    m_blocksToFollow = 0U;
    m_padLength = 0U;

    m_S = false;

    m_Ns = 0U;
    m_fsn = 0U;
    m_lastFragment = true;

    m_headerOffset = 0U;

    m_exSap = PDUSAP::USER_DATA;
    m_srcLlId = 0U;

    m_rspClass = PDUAckClass::NACK;
    m_rspType = PDUAckType::NACK_ILLEGAL;
    m_rspStatus = 0U;

    m_ambtOpcode = 0U;
    m_ambtField8 = 0U;
    m_ambtField9 = 0U;

    m_algId = ALGO_UNENCRYPT;
    m_kId = 0U;

    if (m_data != nullptr)
        ::memset(m_data, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    if (m_mi != nullptr) {
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
    } else {
        m_mi = new uint8_t[MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
    }
}

/* Gets the total length in bytes of enclosed packet data. */

uint32_t DataHeader::getPacketLength() const
{
    if (m_fmt == PDUFormatType::RSP) {
        return 0U; // responses have no packet length as they are header only
    }

    if (m_fmt == PDUFormatType::CONFIRMED) {
        return P25_PDU_CONFIRMED_DATA_LENGTH_BYTES * m_blocksToFollow - 4 - m_padLength;
    }
    else {
        return P25_PDU_UNCONFIRMED_LENGTH_BYTES * m_blocksToFollow - 4 - m_padLength;
    }
}

/* Gets the total length in bytes of entire PDU. */

uint32_t DataHeader::getPDULength() const
{
    if (m_fmt == PDUFormatType::RSP) {
        return 0U; // responses have no packet length as they are header only
    }

    if (m_fmt == PDUFormatType::CONFIRMED) {
        return P25_PDU_CONFIRMED_DATA_LENGTH_BYTES * m_blocksToFollow;
    }
    else {
        return P25_PDU_UNCONFIRMED_LENGTH_BYTES * m_blocksToFollow;
    }
}

/* Gets the raw header data. */

uint32_t DataHeader::getData(uint8_t* buffer) const
{
    assert(buffer != nullptr);
    assert(m_data != nullptr);

    ::memcpy(buffer, m_data, P25_PDU_HEADER_LENGTH_BYTES);
    return P25_PDU_HEADER_LENGTH_BYTES;
}

/* Gets the raw extended address header data. */

uint32_t DataHeader::getExtAddrData(uint8_t* buffer) const
{
    assert(buffer != nullptr);
    assert(m_extAddrData != nullptr);

    ::memcpy(buffer, m_extAddrData, P25_PDU_HEADER_LENGTH_BYTES);
    return P25_PDU_HEADER_LENGTH_BYTES;
}

/* Gets the raw auxiliary ES header data. */

uint32_t DataHeader::getAuxiliaryESData(uint8_t* buffer) const
{
    assert(buffer != nullptr);
    assert(m_auxESData != nullptr);

    if (m_fmt == PDUFormatType::CONFIRMED) {
        ::memcpy(buffer, m_auxESData, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
        return P25_PDU_CONFIRMED_DATA_LENGTH_BYTES;
    } else {
        ::memcpy(buffer, m_auxESData, P25_PDU_HEADER_LENGTH_BYTES);
        return P25_PDU_HEADER_LENGTH_BYTES;
    }
}

/* Helper to calculate the number of blocks to follow and padding length for a PDU. */

void DataHeader::calculateLength(uint32_t packetLength)
{
    uint32_t len = packetLength + 4U; // packet length + CRC32
    if (m_fmt == PDUFormatType::UNCONFIRMED && m_sap == PDUSAP::EXT_ADDR) {
        len += P25_PDU_HEADER_LENGTH_BYTES;
    }

    if (m_fmt == PDUFormatType::CONFIRMED && m_sap == PDUSAP::EXT_ADDR) {
        len += 4U;
    }

    if ((m_sap == PDUSAP::ENC_USER_DATA || m_sap == PDUSAP::ENC_KMM)) {
        len += 13U;
    }

    uint32_t blockLen = (m_fmt == PDUFormatType::CONFIRMED) ? P25_PDU_CONFIRMED_DATA_LENGTH_BYTES : P25_PDU_UNCONFIRMED_LENGTH_BYTES;

    if (len > blockLen) {
        m_padLength = blockLen - (len % blockLen);
        m_blocksToFollow = (uint8_t)ceilf((float)len / (float)blockLen);
    } else {
        m_padLength = 0U;
        m_blocksToFollow = 1U;
    }
}

/* Helper to determine the pad length for a given packet length. */

uint32_t DataHeader::calculatePadLength(uint8_t fmt, uint32_t packetLength)
{
    uint32_t len = packetLength + 4U; // packet length + CRC32
    if (fmt == PDUFormatType::CONFIRMED) {
        return P25_PDU_CONFIRMED_DATA_LENGTH_BYTES - (len % P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
    }
    else {
        return P25_PDU_UNCONFIRMED_LENGTH_BYTES - (len % P25_PDU_UNCONFIRMED_LENGTH_BYTES);
    }
}

/*
** Encryption data 
*/

/* Sets the encryption message indicator. */

void DataHeader::setMI(const uint8_t* mi)
{
    assert(mi != nullptr);

    ::memcpy(m_mi, mi, MI_LENGTH_BYTES);
}

/* Gets the encryption message indicator. */

void DataHeader::getMI(uint8_t* mi) const
{
    assert(mi != nullptr);

    ::memcpy(mi, m_mi, MI_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void DataHeader::copy(const DataHeader& data)
{
    m_ackNeeded = data.m_ackNeeded;
    m_outbound = data.m_outbound;

    m_fmt = data.m_fmt;
    m_sap = data.m_sap;

    m_mfId = data.m_mfId;
    m_llId = data.m_llId;

    m_blocksToFollow = data.m_blocksToFollow;
    m_padLength = data.m_padLength;

    m_F = data.m_F;
    m_S = data.m_S;
    m_fsn = data.m_fsn;
    m_Ns = data.m_Ns;
    m_lastFragment = data.m_lastFragment;
    m_headerOffset = data.m_headerOffset;

    m_exSap = data.m_exSap;
    m_srcLlId = data.m_srcLlId;

    m_rspClass = data.m_rspClass;
    m_rspType = data.m_rspType;
    m_rspStatus = data.m_rspStatus;

    m_ambtOpcode = data.m_ambtOpcode;
    m_ambtField8 = data.m_ambtField8;
    m_ambtField9 = data.m_ambtField9;

    m_algId = data.m_algId;
    m_kId = data.m_kId;

    if (m_data != nullptr && data.m_data != nullptr) {
        ::memcpy(m_data, data.m_data, P25_PDU_HEADER_LENGTH_BYTES);
    }

    if (m_extAddrData != nullptr && data.m_extAddrData != nullptr) {
        ::memcpy(m_extAddrData, data.m_extAddrData, P25_PDU_HEADER_LENGTH_BYTES);
    }
    
    if (m_auxESData != nullptr && data.m_auxESData != nullptr) {
        ::memcpy(m_auxESData, data.m_auxESData, P25_PDU_CONFIRMED_DATA_LENGTH_BYTES);
    }

    if (m_mi != nullptr && data.m_mi != nullptr) {
        ::memcpy(m_mi, data.m_mi, MI_LENGTH_BYTES);
    }
}
