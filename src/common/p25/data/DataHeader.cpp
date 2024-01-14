/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
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
#include "p25/data/DataHeader.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::data;
using namespace p25;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

#if FORCE_TSBK_CRC_WARN
bool DataHeader::m_warnCRC = true;
#else
bool DataHeader::m_warnCRC = false;
#endif

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the DataHeader class.
/// </summary>
DataHeader::DataHeader() :
    m_ackNeeded(false),
    m_outbound(false),
    m_fmt(PDU_FMT_CONFIRMED),
    m_sap(0U),
    m_mfId(P25_MFG_STANDARD),
    m_llId(0U),
    m_F(true),
    m_S(false),
    m_fsn(0U),
    m_Ns(0U),
    m_lastFragment(true),
    m_headerOffset(0U),
    m_srcLlId(0U),
    m_rspClass(PDU_ACK_CLASS_NACK),
    m_rspType(PDU_ACK_TYPE_NACK_ILLEGAL),
    m_rspStatus(0U),
    m_ambtOpcode(0U),
    m_ambtField8(0U),
    m_ambtField9(0U),
    m_trellis(),
    m_blocksToFollow(0U),
    m_padCount(0U),
    m_dataOctets(0U),
    m_data(nullptr)
{
    m_data = new uint8_t[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(m_data, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);
}

/// <summary>
/// Finalizes a instance of the DataHeader class.
/// </summary>
DataHeader::~DataHeader()
{
    delete[] m_data;
}

/// <summary>
/// Decodes P25 PDU data header.
/// </summary>
/// <param name="data"></param>
/// <param name="noTrellis"></param>
/// <returns>True, if PDU data header was decoded, otherwise false.</returns>
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
            if (m_warnCRC) {
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

    m_padCount = m_data[7U] & 0x1FU;                                            // Pad Count
    if (m_fmt == PDU_FMT_RSP || m_fmt == PDU_FMT_AMBT) {
        m_padCount = 0;
    }

    if (m_fmt == PDU_FMT_CONFIRMED) {
        m_dataOctets = 16 * m_blocksToFollow - 4 - m_padCount;
    }
    else {
        m_dataOctets = 12 * m_blocksToFollow - 4 - m_padCount;
    }

    switch (m_fmt) {
    case PDU_FMT_CONFIRMED:
        m_S = (m_data[8U] & 0x80U) == 0x80U;                                    // Re-synchronize Flag

        m_Ns = (m_data[8U] >> 4) & 0x07U;                                       // Packet Sequence No.
        m_fsn = m_data[8U] & 0x07U;                                             // Fragment Sequence No.
        m_lastFragment = (m_data[8U] & 0x08U) == 0x08U;                         // Last Fragment Flag

        m_headerOffset = m_data[9U] & 0x3FU;                                    // Data Header Offset
        break;
    case PDU_FMT_RSP:
        m_ackNeeded = false;
        m_sap = PDU_SAP_USER_DATA;
        m_rspClass = (m_data[1U] >> 6) & 0x03U;                                 // Response Class
        m_rspType = (m_data[1U] >> 3) & 0x07U;                                  // Response Type
        m_rspStatus = m_data[1U] & 0x07U;                                       // Response Status
        if (!m_F) {
            m_srcLlId = (m_data[7U] << 16) + (m_data[8U] << 8) + m_data[9U];    // Source Logical Link ID
        }
        break;
    case PDU_FMT_AMBT:
        m_ambtOpcode = m_data[7U] & 0x3FU;                                      // AMBT Opcode
        m_ambtField8 = m_data[8U];                                              // AMBT Field 8
        m_ambtField9 = m_data[9U];                                              // AMBT Field 9
        // fall-thru
    case PDU_FMT_UNCONFIRMED:
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

/// <summary>
/// Encodes P25 PDU data header.
/// </summary>
/// <param name="data"></param>
void DataHeader::encode(uint8_t* data, bool noTrellis)
{
    assert(data != nullptr);

    uint8_t header[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(header, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    if (m_fmt == PDU_FMT_UNCONFIRMED || m_fmt == PDU_FMT_RSP) {
        m_ackNeeded = false;
    }

    if (m_fmt == PDU_FMT_CONFIRMED && !m_ackNeeded) {
        LogWarning(LOG_P25, "DataHeader::encode(), invalid values for PDU_FMT_CONFIRMED, ackNeeded = %u", m_ackNeeded);
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
    case PDU_FMT_CONFIRMED:
        header[7U] = (m_padCount & 0x1FU);                                      // Pad Count
        header[8U] = (m_S ? 0x80U : 0x00U) +                                    // Re-synchronize Flag
            ((m_Ns & 0x07U) << 4) +                                             // Packet Sequence No.
            (m_lastFragment ? 0x08U : 0x00U) +                                  // Last Fragment Flag
            (m_fsn & 0x07);                                                     // Fragment Sequence No.

        header[9U] = m_headerOffset & 0x3FU;                                    // Data Header Offset
        break;
    case PDU_FMT_RSP:
        header[1U] = ((m_rspClass & 0x03U) << 6) +                              // Response Class
            ((m_rspType & 0x07U) << 3) +                                        // Response Type
            ((m_rspStatus & 0x07U));                                            // Response Status
        if (!m_F) {
            header[7U] = (m_srcLlId >> 16) & 0xFFU;                             // Source Logical Link ID
            header[8U] = (m_srcLlId >> 8) & 0xFFU;
            header[9U] = (m_srcLlId >> 0) & 0xFFU;
        }
        break;
    case PDU_FMT_AMBT:
        header[7U] = (m_ambtOpcode & 0x3FU);                                    // AMBT Opcode
        header[8U] = m_ambtField8;                                              // AMBT Field 8
        header[9U] = m_ambtField9;                                              // AMBT Field 9
        break;
    case PDU_FMT_UNCONFIRMED:
    default:
        header[7U] = (m_padCount & 0x1FU);                                      // Pad Count
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

/// <summary>
/// Helper to reset data values to defaults.
/// </summary>
void DataHeader::reset()
{
    m_ackNeeded = false;
    m_outbound = false;

    m_fmt = PDU_FMT_CONFIRMED;

    m_sap = PDU_SAP_USER_DATA;
    m_mfId = P25_MFG_STANDARD;
    m_llId = 0U;

    m_F = true;
    m_blocksToFollow = 0U;
    m_padCount = 0U;

    m_dataOctets = 0U;

    m_S = false;

    m_Ns = 0U;
    m_fsn = 0U;
    m_lastFragment = true;

    m_headerOffset = 0U;

    m_srcLlId = 0U;
    m_rspClass = PDU_ACK_CLASS_NACK;
    m_rspType = PDU_ACK_TYPE_NACK_ILLEGAL;
    m_rspStatus = 0U;

    m_ambtOpcode = 0U;
    m_ambtField8 = 0U;
    m_ambtField9 = 0U;

    ::memset(m_data, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);
}

/// <summary>
/// Gets the total number of data octets.
/// </summary>
/// <returns></returns>
uint32_t DataHeader::getDataOctets() const
{
    return m_dataOctets;
}

/// <summary>
/// Gets the raw header data.
/// </summary>
/// <returns></returns>
uint32_t DataHeader::getData(uint8_t* buffer) const
{
    assert(buffer != nullptr);
    assert(m_data != nullptr);

    ::memcpy(buffer, m_data, P25_PDU_HEADER_LENGTH_BYTES);
    return P25_PDU_HEADER_LENGTH_BYTES;
}

/** Common Data */
/// <summary>
/// Sets the total number of blocks to follow this header.
/// </summary>
/// <param name="blocksToFollow"></param>
void DataHeader::setBlocksToFollow(uint8_t blocksToFollow)
{
    m_blocksToFollow = blocksToFollow;

    // recalculate count of data octets
    if (m_fmt == PDU_FMT_CONFIRMED) {
        m_dataOctets = 16 * m_blocksToFollow - 4 - m_padCount;
    }
    else {
        m_dataOctets = 12 * m_blocksToFollow - 4 - m_padCount;
    }
}

/// <summary>
/// Gets the total number of blocks to follow this header.
/// </summary>
/// <returns></returns>
uint8_t DataHeader::getBlocksToFollow() const
{
    return m_blocksToFollow;
}

/// <summary>
/// Sets the count of block padding.
/// </summary>
/// <param name="padCount"></param>
void DataHeader::setPadCount(uint8_t padCount)
{
    m_padCount = padCount;

    // recalculate count of data octets
    if (m_fmt == PDU_FMT_CONFIRMED) {
        m_dataOctets = 16 * m_blocksToFollow - 4 - m_padCount;
    }
    else {
        m_dataOctets = 12 * m_blocksToFollow - 4 - m_padCount;
    }
}

/// <summary>
/// Gets the count of block padding.
/// </summary>
/// <returns></returns>
uint8_t DataHeader::getPadCount() const
{
    return m_padCount;
}
