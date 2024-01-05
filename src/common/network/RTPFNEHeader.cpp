/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#include "network/RTPFNEHeader.h"

using namespace network::frame;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the RTPFNEHeader class.
/// </summary>
RTPFNEHeader::RTPFNEHeader() :
    RTPExtensionHeader(),
    m_crc16(0U),
    m_func(0U),
    m_subFunc(0U),
    m_streamId(0U),
    m_peerId(0U),
    m_messageLength(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the RTPExtensionHeader class.
/// </summary>
RTPFNEHeader::~RTPFNEHeader()
{
    /* stub */
}

/// <summary>
/// Decode a RTP header.
/// </summary>
/// <param name="data"></param>
bool RTPFNEHeader::decode(const uint8_t* data)
{
    assert(data != nullptr);

    RTPExtensionHeader::decode(data);
    if (m_payloadLength != RTP_FNE_HEADER_LENGTH_EXT_LEN) {
        return false;
    }

    if (m_payloadType != DVM_FRAME_START) {
        return false;
    }

    m_crc16 = (data[4U] << 8) | (data[5U] << 0);                                // CRC-16
    m_func = data[6U];                                                          // Function
    m_subFunc = data[7U];                                                       // Sub-Function
    m_streamId = __GET_UINT32(data, 8U);                                        // Stream ID
    m_peerId = __GET_UINT32(data, 12U);                                         // Peer ID
    m_messageLength = __GET_UINT32(data, 16U);                                  // Message Length

    return true;
}

/// <summary>
/// Encode a RTP header.
/// </summary>
/// <param name="data"></param>
void RTPFNEHeader::encode(uint8_t* data)
{
    assert(data != nullptr);

    m_payloadType = DVM_FRAME_START;
    m_payloadLength = RTP_FNE_HEADER_LENGTH_EXT_LEN;
    RTPExtensionHeader::encode(data);

    data[4U] = (m_crc16 >> 8) & 0xFFU;                                          // CRC-16 MSB
    data[5U] = (m_crc16 >> 0) & 0xFFU;                                          // CRC-16 LSB
    data[6U] = m_func;                                                          // Function
    data[7U] = m_subFunc;                                                       // Sub-Function

    __SET_UINT32(m_streamId, data, 8U);                                         // Stream ID
    __SET_UINT32(m_peerId, data, 12U);                                          // Peer ID
    __SET_UINT32(m_messageLength, data, 16U);                                   // Message Length
}
