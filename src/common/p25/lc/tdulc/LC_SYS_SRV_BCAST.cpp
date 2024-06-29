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
#include "p25/lc/tdulc/LC_SYS_SRV_BCAST.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the LC_SYS_SRV_BCAST class. */
LC_SYS_SRV_BCAST::LC_SYS_SRV_BCAST() : TDULC()
{
    m_lco = LCO::SYS_SRV_BCAST;
}

/* Decode a terminator data unit w/ link control. */
bool LC_SYS_SRV_BCAST::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a terminator data unit w/ link control. */
void LC_SYS_SRV_BCAST::encode(uint8_t* data)
{
    assert(data != nullptr);

    const uint32_t services = (m_siteData.netActive()) ? SystemService::NET_ACTIVE : 0U | SYS_SRV_DEFAULT;

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = 0U;
    rsValue = (rsValue << 16) + services;                                           // System Services Available
    rsValue = (rsValue << 24) + services;                                           // System Services Supported

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
