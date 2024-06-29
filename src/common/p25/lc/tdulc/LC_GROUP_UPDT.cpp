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
#include "p25/lc/tdulc/LC_GROUP_UPDT.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the LC_GROUP_UPDT class. */
LC_GROUP_UPDT::LC_GROUP_UPDT() : TDULC()
{
    m_lco = LCO::GROUP_UPDT;
}

/* Decode a terminator data unit w/ link control. */
bool LC_GROUP_UPDT::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a terminator data unit w/ link control. */
void LC_GROUP_UPDT::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = m_siteData.channelId();                                               // Group A - Channel ID
    rsValue = (rsValue << 12) + m_grpVchNo;                                         // Group A - Channel Number
    rsValue = (rsValue << 16) + m_dstId;                                            // Group A - Talkgroup Address
    rsValue = (rsValue << 4) + m_siteData.channelId();                              // Group B - Channel ID
    rsValue = (rsValue << 12) + m_grpVchNo;                                         // Group B - Channel Number
    rsValue = (rsValue << 16) + m_dstId;                                            // Group B - Talkgroup Address

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
