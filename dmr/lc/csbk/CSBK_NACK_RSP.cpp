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
#include "dmr/lc/csbk/CSBK_NACK_RSP.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr::lc::csbk;
using namespace dmr::lc;
using namespace dmr;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK_NACK_RSP class.
/// </summary>
CSBK_NACK_RSP::CSBK_NACK_RSP() : CSBK(),
    m_serviceKind(0U)
{
    m_CSBKO = CSBKO_NACK_RSP;
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_NACK_RSP::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_GI = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                          // Group/Individual Flag
    m_serviceKind = (((csbkValue >> 56) & 0xFFU) & 0x3FU);                          // Service Kind
    m_reason = (uint8_t)((csbkValue >> 48) & 0xFFU);                                // Reason Code
    m_srcId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                            // Source Radio Address
    m_dstId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Target Radio Address

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_NACK_RSP::encode(uint8_t* data)
{
    assert(data != NULL);

    ulong64_t csbkValue = 0U;

    csbkValue = 0x80U +                                                             // Additional Information Field (always 1)
        (m_GI ? 0x40U : 0x00U) +                                                    // Source Type
        (m_serviceKind & 0x3FU);                                                    // Service Kind
    csbkValue = (csbkValue << 8) + m_reason;                                        // Reason Code
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address
    csbkValue = (csbkValue << 24) + m_dstId;                                        // Target Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void CSBK_NACK_RSP::copy(const CSBK_NACK_RSP& data)
{
    CSBK::copy(data);

    m_serviceKind = data.m_serviceKind;
}
