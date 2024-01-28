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
#include "p25/lc/tdulc/LC_SYS_SRV_BCAST.h"

using namespace p25::lc::tdulc;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the LC_SYS_SRV_BCAST class.
/// </summary>
LC_SYS_SRV_BCAST::LC_SYS_SRV_BCAST() : TDULC()
{
    m_lco = p25::LC_SYS_SRV_BCAST;
}

/// <summary>
/// Decode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
bool LC_SYS_SRV_BCAST::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
void LC_SYS_SRV_BCAST::encode(uint8_t* data)
{
    assert(data != nullptr);

    const uint32_t services = (m_siteData.netActive()) ? P25_SYS_SRV_NET_ACTIVE : 0U | P25_SYS_SRV_DEFAULT;

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = 0U;
    rsValue = (rsValue << 16) + services;                                           // System Services Available
    rsValue = (rsValue << 24) + services;                                           // System Services Supported

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
