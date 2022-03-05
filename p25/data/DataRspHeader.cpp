/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2020,2022 by Bryan Biedenkapp N2PLL
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
#include "p25/data/DataRspHeader.h"
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
/// Initializes a new instance of the DataRspHeader class.
/// </summary>
DataRspHeader::DataRspHeader() :
    m_outbound(true),
    m_rspClass(PDU_ACK_CLASS_NACK),
    m_rspType(PDU_ACK_TYPE_NACK_ILLEGAL),
    m_rspStatus(0U),
    m_mfId(P25_MFG_STANDARD),
    m_llId(0U),
    m_srcLlId(0U),
    m_extended(true),
    m_trellis(),
    m_blocksToFollow(0U),
    m_dataOctets(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the DataRspHeader class.
/// </summary>
DataRspHeader::~DataRspHeader()
{
    /* stub */
}

/// <summary>
/// Decodes P25 PDU data response header.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if PDU data response header was decoded, otherwise false.</returns>
bool DataRspHeader::decode(const uint8_t* data)
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

#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, DataRspHeader::decode(), PDU Response Header Data", data, P25_PDU_HEADER_LENGTH_BYTES);
#endif

    m_outbound = (header[0U] & 0x20U) == 0x20U;                                 // Inbound/Outbound

    m_rspClass = (header[1U] >> 6) & 0x03U;                                     // Response Class
    m_rspType = (header[1U] >> 3) & 0x07U;                                      // Response Type
    m_rspStatus = header[1U] & 0x07U;                                           // Response Status

    m_mfId = header[2U];                                                        // Mfg Id.

    m_llId = (header[3U] << 16) + (header[4U] << 8) + header[5U];               // Logical Link ID

    m_extended = (header[6U] & 0x80U) == 0x80U;                                 // Extended Addressing
    m_blocksToFollow = header[6U] & 0x7FU;                                      // Block Frames to Follow

    m_srcLlId = (header[7U] << 16) + (header[8U] << 8) + header[9U];            // Source Logical Link ID

    return true;
}

/// <summary>
/// Encodes P25 PDU data response header.
/// </summary>
/// <param name="data"></param>
void DataRspHeader::encode(uint8_t * data)
{
    assert(data != NULL);

    uint8_t header[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(header, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    header[0U] = PDU_FMT_RSP;
    header[0U] = (m_outbound ? 0x20U : 0x00U) +                                 // Inbound/Outbound
        (PDU_FMT_RSP & 0x1FU);                                                  // Packet Format

    header[1U] = ((m_rspClass & 0x03U) << 6) +                                  // Response Class
        ((m_rspType & 0x07U) << 3) +                                            // Response Type
        ((m_rspStatus & 0x07U));                                                // Response Status

    header[2U] = m_mfId;                                                        // Mfg Id.

    header[3U] = (m_llId >> 16) & 0xFFU;                                        // Logical Link ID
    header[4U] = (m_llId >> 8) & 0xFFU;
    header[5U] = (m_llId >> 0) & 0xFFU;

    header[6U] = (m_extended ? 0x80U : 0x00U) +                                 // Extended Addressing
        (m_blocksToFollow & 0x7FU);                                             // Blocks Frames to Follow

    header[7U] = (m_srcLlId >> 16) & 0xFFU;                                     // Source Logical Link ID
    header[8U] = (m_srcLlId >> 8) & 0xFFU;
    header[9U] = (m_srcLlId >> 0) & 0xFFU;

    // compute CRC-CCITT 16
    edac::CRC::addCCITT162(header, P25_PDU_HEADER_LENGTH_BYTES);

#if DEBUG_P25_PDU_DATA
    Utils::dump(1U, "P25, DataRspHeader::encode(), PDU Response Header Data", data, P25_PDU_HEADER_LENGTH_BYTES);
#endif

    // encode 1/2 rate Trellis
    m_trellis.encode12(header, data);
}

/// <summary>
/// Helper to reset data values to defaults.
/// </summary>
void DataRspHeader::reset()
{
    m_outbound = false;

    m_rspClass = PDU_ACK_CLASS_NACK;
    m_rspType = PDU_ACK_TYPE_NACK_ILLEGAL;
    m_rspStatus = 0U;

    m_mfId = P25_MFG_STANDARD;
    m_llId = 0U;
    m_srcLlId = 0U;

    m_extended = true;

    m_blocksToFollow = 0U;
    m_dataOctets = 0U;
}

/// <summary>Gets the total number of data octets.</summary>
/// <returns></returns>
uint32_t DataRspHeader::getDataOctets() const
{
    return m_dataOctets;
}

/** Common Data */
/// <summary>Sets the total number of blocks to follow this header.</summary>
/// <param name="blocksToFollow"></param>
void DataRspHeader::setBlocksToFollow(uint8_t blocksToFollow)
{
    m_blocksToFollow = blocksToFollow;

    // recalculate count of data octets
    m_dataOctets = 16 * m_blocksToFollow - 4;
}

/// <summary>Gets the total number of blocks to follow this header.</summary>
/// <returns></returns>
uint8_t DataRspHeader::getBlocksToFollow() const
{
    return m_blocksToFollow;
}
