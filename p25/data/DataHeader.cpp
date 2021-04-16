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
#include "p25/data/DataHeader.h"
#include "edac/CRC.h"
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
/// Initializes a new instance of the DataHeader class.
/// </summary>
DataHeader::DataHeader() :
    m_ackNeeded(false),
    m_outbound(false),
    m_fmt(PDU_FMT_CONFIRMED),
    m_sap(0U),
    m_mfId(P25_MFG_STANDARD),
    m_llId(0U),
    m_fullMessage(false),
    m_sync(false),
    m_n(0U),
    m_seqNo(0U),
    m_headerOffset(0U),
    m_trellis(),
    m_blocksToFollow(0U),
    m_padCount(0U),
    m_dataOctets(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the DataHeader class.
/// </summary>
DataHeader::~DataHeader()
{
    /* stub */
}

/// <summary>
/// Decodes P25 PDU data header.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if PDU data header was decoded, otherwise false.</returns>
bool DataHeader::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint8_t header[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(header, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    // decode 1/2 rate Trellis & check CRC-CCITT 16
    bool valid = m_trellis.decode12(data, header);
    if (valid)
        valid = edac::CRC::checkCCITT162(header, P25_PDU_HEADER_LENGTH_BYTES);
    if (!valid) {
        return false;
    }

    // Utils::dump(1U, "PDU Header Data", data, P25_PDU_HEADER_LENGTH_BYTES);

    m_ackNeeded = (header[0U] & 0x40U) == 0x40U;                                // Acknowledge Needed
    m_outbound = (header[0U] & 0x20U) == 0x20U;                                 // Inbound/Outbound
        
    m_fmt = header[0U] & 0x1F;                                                  // Packet Format

    m_sap = header[1U] & 0x3FU;                                                 // Service Access Point
    m_mfId = header[2U];                                                        // Mfg Id.

    m_llId = (header[3U] << 16) + (header[4U] << 8) + header[5U];               // Logical Link ID

    m_fullMessage = (header[6U] & 0x80U) == 0x80U;                              // Full Message Flag
    m_blocksToFollow = header[6U] & 0x7FU;                                      // Block Frames to Follow
    m_padCount = header[7U] & 0x1FU;                                            // Pad Count

    if (m_fmt == PDU_FMT_CONFIRMED) {
        m_dataOctets = 16 * m_blocksToFollow - 4 - m_padCount;
    }
    else {
        m_dataOctets = 12 * m_blocksToFollow - 4 - m_padCount;
    }

    m_sync = (header[8U] & 0x80U) == 0x80U;                                     // Re-synchronize Flag

    m_n = (header[8U] >> 4) & 0x07U;                                            // Packet Sequence No.
    m_seqNo = header[8U] & 0x0FU;                                               // Fragment Sequence No.

    m_headerOffset = header[9U] & 0x3FU;                                        // Data Header Offset

    return true;
}

/// <summary>
/// Encodes P25 PDU data header.
/// </summary>
/// <param name="data"></param>
void DataHeader::encode(uint8_t* data)
{
    assert(data != NULL);

    uint8_t header[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(header, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    header[0U] = (m_ackNeeded ? 0x40U : 0x00U) +                                // Acknowledge Needed
        (m_outbound ? 0x20U : 0x00U) +                                          // Inbound/Outbound
        (m_fmt & 0x1FU);                                                        // Packet Format

    header[1U] = m_sap & 0x3FU;                                                 // Service Access Point
    header[1U] |= 0xC0;

    header[2U] = m_mfId;                                                        // Mfg Id.

    header[3U] = (m_llId >> 16) & 0xFFU;                                        // Logical Link ID
    header[4U] = (m_llId >> 8) & 0xFFU;
    header[5U] = (m_llId >> 0) & 0xFFU;

    header[6U] = (m_fullMessage ? 0x80U : 0x00U) +                              // Full Message Flag
        (m_blocksToFollow & 0x7FU);                                             // Blocks Frames to Follow

    header[7U] = (m_padCount & 0x1FU);                                          // Pad Count

    header[8U] = (m_sync ? 0x80U : 0x00U) +                                     // Re-synchronize Flag
        ((m_n << 4) && 0x07U) +                                                 // Packet Sequence No.
        (m_seqNo & 0x0F);                                                       // Fragment Sequence No.

    header[9U] = m_headerOffset & 0x3FU;                                        // Data Header Offset

    // compute CRC-CCITT 16
    edac::CRC::addCCITT162(header, P25_PDU_HEADER_LENGTH_BYTES);

    // encode 1/2 rate Trellis
    m_trellis.encode12(header, data);
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

    m_fullMessage = false;
    m_blocksToFollow = 0U;
    m_padCount = 0U;

    m_dataOctets = 0U;

    m_sync = false;

    m_n = 0U;
    m_seqNo = 0U;

    m_headerOffset = 0U;
}

/// <summary>Gets the total number of data octets.</summary>
/// <returns></returns>
uint32_t DataHeader::getDataOctets() const
{
    return m_dataOctets;
}

/** Common Data */
/// <summary>Sets the total number of blocks to follow this header.</summary>
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

/// <summary>Gets the total number of blocks to follow this header.</summary>
/// <returns></returns>
uint8_t DataHeader::getBlocksToFollow() const
{
    return m_blocksToFollow;
}

/// <summary>Sets the count of block padding.</summary>
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

/// <summary>Gets the count of block padding.</summary>
/// <returns></returns>
uint8_t DataHeader::getPadCount() const
{
    return m_padCount;
}
