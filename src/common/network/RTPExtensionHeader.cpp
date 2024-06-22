// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
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
RTPExtensionHeader::~RTPExtensionHeader() = default;

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
