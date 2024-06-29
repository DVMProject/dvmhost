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
#include "p25/lc/tdulc/LC_NET_STS_BCAST.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the LC_NET_STS_BCAST class. */

LC_NET_STS_BCAST::LC_NET_STS_BCAST() : TDULC()
{
    m_lco = LCO::NET_STS_BCAST;
}

/* Decode a terminator data unit w/ link control. */

bool LC_NET_STS_BCAST::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a terminator data unit w/ link control. */

void LC_NET_STS_BCAST::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = (rsValue << 20) + m_siteData.netId();                                 // Network ID
    rsValue = (rsValue << 12) + m_siteData.sysId();                                 // System ID
    rsValue = (rsValue << 4) + m_siteData.channelId();                              // Channel ID
    rsValue = (rsValue << 12) + m_siteData.channelNo();                             // Channel Number
    rsValue = (rsValue << 8) + m_siteData.serviceClass();                           // System Service Class

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
