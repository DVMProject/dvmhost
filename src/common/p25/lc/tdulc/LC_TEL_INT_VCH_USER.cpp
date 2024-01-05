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
#include "p25/lc/tdulc/LC_TEL_INT_VCH_USER.h"
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
/// Initializes a new instance of the LC_TEL_INT_VCH_USER class.
/// </summary>
LC_TEL_INT_VCH_USER::LC_TEL_INT_VCH_USER() : TDULC()
{
    m_lco = p25::LC_TEL_INT_VCH_USER;
}

/// <summary>
/// Decode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
bool LC_TEL_INT_VCH_USER::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint8_t rs[P25_TDULC_LENGTH_BYTES + 1U];
    ::memset(rs, 0x00U, P25_TDULC_LENGTH_BYTES);

    bool ret = TDULC::decode(data, rs);
    if (!ret)
        return false;

    ulong64_t rsValue = TDULC::toValue(rs);

    m_emergency = (rs[2U] & 0x80U) == 0x80U;                                        // Emergency Flag
    m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                        // Encryption Flag
    m_priority = (rs[2U] & 0x07U);                                                  // Priority
    m_callTimer = (uint32_t)((rsValue >> 24) & 0xFFFFU);                            // Call Timer
    if (m_srcId == 0U) {
        m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source/Target Address
    }

    return true;
}

/// <summary>
/// Encode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
void LC_TEL_INT_VCH_USER::encode(uint8_t* data)
{
    assert(data != NULL);

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = m_mfId;
    rsValue = (rsValue << 8) +
        (m_emergency ? 0x80U : 0x00U) +                                             // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                             // Encrypted Flag
        (m_priority & 0x07U);                                                       // Priority
    rsValue = (rsValue << 24) + m_dstId;                                            // Target Radio Address
    rsValue = (rsValue << 24) + m_srcId;                                            // Source Radio Address

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
