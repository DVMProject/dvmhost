// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/lc/tdulc/LC_CONV_FALLBACK.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the LC_CONV_FALLBACK class.
/// </summary>
LC_CONV_FALLBACK::LC_CONV_FALLBACK() : TDULC()
{
    m_lco = LCO::CONV_FALLBACK;
}

/// <summary>
/// Decode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
bool LC_CONV_FALLBACK::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
void LC_CONV_FALLBACK::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t rsValue = 0U;

    rsValue = (rsValue << 48) + m_siteData.channelId();                             // Channel ID 6
    rsValue = (rsValue << 40) + m_siteData.channelId();                             // Channel ID 5
    rsValue = (rsValue << 32) + m_siteData.channelId();                             // Channel ID 4
    rsValue = (rsValue << 24) + m_siteData.channelId();                             // Channel ID 3
    rsValue = (rsValue << 16) + m_siteData.channelId();                             // Channel ID 2
    rsValue = (rsValue << 8) + m_siteData.channelId();                              // Channel ID 1

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
