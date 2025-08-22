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
#include "p25/lc/tdulc/LC_EXPLICIT_SOURCE_ID.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the LC_EXPLICIT_SOURCE_ID class. */

LC_EXPLICIT_SOURCE_ID::LC_EXPLICIT_SOURCE_ID() : TDULC()
{
    m_lco = LCO::EXPLICIT_SOURCE_ID;
}

/* Decode a terminator data unit w/ link control. */

bool LC_EXPLICIT_SOURCE_ID::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t rs[P25_TDULC_LENGTH_BYTES + 1U];
    ::memset(rs, 0x00U, P25_TDULC_LENGTH_BYTES);

    bool ret = TDULC::decode(data, rs);
    if (!ret)
        return false;

    ulong64_t rsValue = TDULC::toValue(rs);

    m_netId = (uint32_t)((rsValue >> 36) & 0xFFFFFU);                           // Network ID
    m_sysId = (uint32_t)((rsValue >> 24) & 0xFFFU);                             // System ID
    m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source Radio Address

    return true;
}

/* Encode a terminator data unit w/ link control. */

void LC_EXPLICIT_SOURCE_ID::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = (rsValue << 8) + m_netId;                                     // Network ID
    rsValue = (rsValue << 12) + (m_sysId & 0xFFFU);                         // System ID
    rsValue = (rsValue << 24) + m_srcId;                                    // Source Radio Address

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
