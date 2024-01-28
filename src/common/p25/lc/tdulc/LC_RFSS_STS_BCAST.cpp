// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/lc/tdulc/LC_RFSS_STS_BCAST.h"

using namespace p25::lc::tdulc;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the LC_RFSS_STS_BCAST class.
/// </summary>
LC_RFSS_STS_BCAST::LC_RFSS_STS_BCAST() : TDULC()
{
    m_lco = p25::LC_RFSS_STS_BCAST;
}

/// <summary>
/// Decode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
bool LC_RFSS_STS_BCAST::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
void LC_RFSS_STS_BCAST::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = m_siteData.lra();                                                     // Location Registration Area
    rsValue = (rsValue << 12) + m_siteData.sysId();                                 // System ID
    rsValue = (rsValue << 8) + m_siteData.rfssId();                                 // RF Sub-System ID
    rsValue = (rsValue << 8) + m_siteData.siteId();                                 // Site ID
    rsValue = (rsValue << 4) + m_siteData.channelId();                              // Channel ID
    rsValue = (rsValue << 12) + m_siteData.channelNo();                             // Channel Number
    rsValue = (rsValue << 8) + m_siteData.serviceClass();                           // System Service Class

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
