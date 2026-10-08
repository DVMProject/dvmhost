// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/data/IPv4Packet.h"

using namespace p25::data;

// ---------------------------------------------------------------------------
//  Structure Members
// ---------------------------------------------------------------------------

/* Parses and validates an IPv4 packet. */

bool IPv4Packet::parse(const uint8_t* data, uint32_t length, IPv4Packet& packet)
{
    if (data == nullptr || length < 20U || (data[0U] >> 4U) != 4U)
        return false;

    uint8_t headerLength = uint8_t((data[0U] & 0x0FU) * 4U);
    if (headerLength < 20U || headerLength > length)
        return false;

    uint16_t totalLength = GET_UINT16(data, 2U);//uint16_t((uint16_t(data[2U]) << 8U) | uint16_t(data[3U]));
    if (totalLength < headerLength || totalLength > length)
        return false;

    packet.headerLength = headerLength;
    packet.protocol = data[9U];
    packet.totalLength = totalLength;
    packet.sourceAddress = GET_UINT32(data, 12U);
    packet.destinationAddress = GET_UINT32(data, 16U);

    return true;
}
