// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tdulc/LC_GROUP.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the LC_GROUP class. */
LC_GROUP::LC_GROUP() : TDULC()
{
    m_lco = LCO::GROUP;
}

/* Decode a terminator data unit w/ link control. */
bool LC_GROUP::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t rs[P25_TDULC_LENGTH_BYTES + 1U];
    ::memset(rs, 0x00U, P25_TDULC_LENGTH_BYTES);

    bool ret = TDULC::decode(data, rs);
    if (!ret)
        return false;

    ulong64_t rsValue = TDULC::toValue(rs);

    m_mfId = rs[1U];                                                                // Mfg Id.
    m_group = true;
    m_emergency = (rs[2U] & 0x80U) == 0x80U;                                        // Emergency Flag
    m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                        // Encryption Flag
    m_priority = (rs[2U] & 0x07U);                                                  // Priority
    m_dstId = (uint32_t)((rsValue >> 24) & 0xFFFFU);                                // Talkgroup Address
    m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                      // Source Radio Address

    return true;
}

/* Encode a terminator data unit w/ link control. */
void LC_GROUP::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t rsValue = 0U;

    rsValue = m_mfId;
    rsValue = (rsValue << 8) +
        (m_emergency ? 0x80U : 0x00U) +                                             // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                             // Encrypted Flag
        (m_priority & 0x07U);                                                       // Priority
    rsValue = (rsValue << 24) + m_dstId;                                            // Talkgroup Address
    rsValue = (rsValue << 24) + m_srcId;                                            // Source Radio Address

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
