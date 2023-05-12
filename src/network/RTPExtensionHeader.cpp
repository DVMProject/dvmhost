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
#include "network/RTPExtensionHeader.h"

using namespace network::frame;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the RTPExtensionHeader class.
/// </summary>
RTPExtensionHeader::RTPExtensionHeader() :
    m_payloadType(0U),
    m_payloadLength(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the RTPExtensionHeader class.
/// </summary>
RTPExtensionHeader::~RTPExtensionHeader()
{
    /* stub */
}

/// <summary>
/// Decode a RTP header.
/// </summary>
/// <param name="data"></param>
bool RTPExtensionHeader::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_payloadType = (data[0U] << 8) | (data[1U] << 0);                          // Payload Type
    m_payloadLength = (data[2U] << 8) | (data[3U] << 0);                        // Payload Length

    return true;
}

/// <summary>
/// Encode a RTP header.
/// </summary>
/// <param name="data"></param>
void RTPExtensionHeader::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = (m_payloadType >> 8) & 0xFFU;                                    // Payload Type MSB
    data[1U] = (m_payloadType >> 0) & 0xFFU;                                    // Payload Type LSB
    data[2U] = (m_payloadLength >> 8) & 0xFFU;                                  // Payload Length MSB
    data[3U] = (m_payloadLength >> 0) & 0xFFU;                                  // Payload Length LSB
}
