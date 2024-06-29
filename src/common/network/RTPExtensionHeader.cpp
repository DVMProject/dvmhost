// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "network/RTPExtensionHeader.h"

using namespace network::frame;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RTPExtensionHeader class. */

RTPExtensionHeader::RTPExtensionHeader() :
    m_payloadType(0U),
    m_payloadLength(0U)
{
    /* stub */
}

/* Finalizes a instance of the RTPExtensionHeader class. */

RTPExtensionHeader::~RTPExtensionHeader() = default;

/* Decode a RTP header. */

bool RTPExtensionHeader::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_payloadType = (data[0U] << 8) | (data[1U] << 0);                          // Payload Type
    m_payloadLength = (data[2U] << 8) | (data[3U] << 0);                        // Payload Length

    return true;
}

/* Encode a RTP header. */

void RTPExtensionHeader::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = (m_payloadType >> 8) & 0xFFU;                                    // Payload Type MSB
    data[1U] = (m_payloadType >> 0) & 0xFFU;                                    // Payload Type LSB
    data[2U] = (m_payloadLength >> 8) & 0xFFU;                                  // Payload Length MSB
    data[3U] = (m_payloadLength >> 0) & 0xFFU;                                  // Payload Length LSB
}
