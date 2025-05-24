// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "network/RPCHeader.h"
#include "Utils.h"

using namespace network::frame;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RPCHeader class. */

RPCHeader::RPCHeader() :
    m_crc16(0U),
    m_func(0U),
    m_messageLength(0U)
{
    /* stub */
}

/* Finalizes a instance of the RPCHeader class. */

RPCHeader::~RPCHeader() = default;

/* Decode a RTP header. */

bool RPCHeader::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_crc16 = (data[0U] << 8) | (data[1U] << 0);                                // CRC-16
    m_func = GET_UINT16(data, 2U);                                              // Function
    m_messageLength = GET_UINT32(data, 4U);                                     // Message Length

    return true;
}

/* Encode a RTP header. */

void RPCHeader::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = (m_crc16 >> 8) & 0xFFU;                                          // CRC-16 MSB
    data[1U] = (m_crc16 >> 0) & 0xFFU;                                          // CRC-16 LSB
    SET_UINT16(m_func, data, 2U);                                               // Function

    SET_UINT32(m_messageLength, data, 4U);                                      // Message Length
}
