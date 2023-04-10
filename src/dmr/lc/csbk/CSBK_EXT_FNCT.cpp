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
#include "dmr/lc/csbk/CSBK_EXT_FNCT.h"
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
/// Initializes a new instance of the CSBK_EXT_FNCT class.
/// </summary>
CSBK_EXT_FNCT::CSBK_EXT_FNCT() : CSBK(),
    m_extendedFunction(DMR_EXT_FNCT_CHECK)
{
    m_CSBKO = CSBKO_EXT_FNCT;
    m_FID = FID_DMRA;
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_EXT_FNCT::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_dataContent = (((csbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 //
    m_extendedFunction = (uint8_t)((csbkValue >> 48) & 0xFFU);                      // Function
    m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                            // Target Radio Address
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_EXT_FNCT::encode(uint8_t* data)
{
    assert(data != NULL);

    ulong64_t csbkValue = 0U;

    csbkValue =
        (m_GI ? 0x40U : 0x00U) +                                                    // Group or Invididual
        (m_dataContent ? 0x80U : 0x00U);
    csbkValue = (csbkValue << 8) + m_extendedFunction;                              // Function
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
void CSBK_EXT_FNCT::copy(const CSBK_EXT_FNCT& data)
{
    CSBK::copy(data);

    m_extendedFunction = data.m_extendedFunction;
}
