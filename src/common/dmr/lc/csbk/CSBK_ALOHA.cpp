/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
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
#include "dmr/lc/csbk/CSBK_ALOHA.h"
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
/// Initializes a new instance of the CSBK_ALOHA class.
/// </summary>
CSBK_ALOHA::CSBK_ALOHA() : CSBK(),
    m_siteTSSync(false),
    m_alohaMask(0U),
    m_backoffNo(1U),
    m_nRandWait(DEFAULT_NRAND_WAIT)
{
    m_CSBKO = CSBKO_ALOHA;
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_ALOHA::decode(const uint8_t* data)
{
    assert(data != NULL);

    /* stub */

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_ALOHA::encode(uint8_t* data)
{
    assert(data != NULL);

    ulong64_t csbkValue = 0U;

    csbkValue = (csbkValue << 2) + 0U;                                              // Reserved
    csbkValue = (csbkValue << 1) + ((m_siteTSSync) ? 1U : 0U);                      // Site Time Slot Synchronization
    csbkValue = (csbkValue << 3) + DMR_ALOHA_VER_151;                               // DMR Spec. Version (1.5.1)
    csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);                // Site Timing: Aligned or Offset
    csbkValue = (csbkValue << 1) + ((m_siteData.netActive()) ? 1U : 0U);            // Site Networked
    csbkValue = (csbkValue << 5) + (m_alohaMask & 0x1FU);                           // MS Mask
    csbkValue = (csbkValue << 2) + 0U;                                              // Service Function
    csbkValue = (csbkValue << 4) + (m_nRandWait & 0x0FU);                           // Random Access Wait
    csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U);           // Require Registration
    csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);                           // Backoff Number
    csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();                    // Site Identity
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/// <summary>
/// Returns a string that represents the current CSBK.
/// </summary>
/// <returns></returns>
std::string CSBK_ALOHA::toString()
{
    return std::string("CSBKO_ALOHA (Aloha PDU)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void CSBK_ALOHA::copy(const CSBK_ALOHA& data)
{
    CSBK::copy(data);

    m_siteTSSync = data.m_siteTSSync;
    m_alohaMask = data.m_alohaMask;

    m_backoffNo = data.m_backoffNo;
    m_nRandWait = data.m_nRandWait;
}
