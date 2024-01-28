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
#include "p25/lc/tdulc/LC_GROUP_UPDT.h"

using namespace p25::lc::tdulc;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the LC_GROUP_UPDT class.
/// </summary>
LC_GROUP_UPDT::LC_GROUP_UPDT() : TDULC()
{
    m_lco = p25::LC_GROUP_UPDT;
}

/// <summary>
/// Decode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
bool LC_GROUP_UPDT::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
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
