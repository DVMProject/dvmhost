/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "p25/lc/tdulc/LC_SYS_SRV_BCAST.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tdulc;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>

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
    assert(data != NULL);

    /* stub */

    return true;
}

/// <summary>
/// Encode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
void LC_SYS_SRV_BCAST::encode(uint8_t* data)
{
    assert(data != NULL);

    const uint32_t services = (m_siteData.netActive()) ? P25_SYS_SRV_NET_ACTIVE : 0U | P25_SYS_SRV_DEFAULT;

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = 0U;
    rsValue = (rsValue << 16) + services;                                           // System Services Available
    rsValue = (rsValue << 24) + services;                                           // System Services Supported

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
