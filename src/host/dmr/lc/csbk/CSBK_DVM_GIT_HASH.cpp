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
#include "dmr/lc/csbk/CSBK_DVM_GIT_HASH.h"
#include "HostMain.h"

using namespace dmr::lc::csbk;
using namespace dmr::lc;
using namespace dmr;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK_DVM_GIT_HASH class.
/// </summary>
CSBK_DVM_GIT_HASH::CSBK_DVM_GIT_HASH() : CSBK()
{
    m_CSBKO = CSBKO_DVM_GIT_HASH;
    m_FID = FID_OCS_DVM;
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_DVM_GIT_HASH::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_DVM_GIT_HASH::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = g_gitHashBytes[0];                                                  // ...
    csbkValue = (csbkValue << 8) + (g_gitHashBytes[1U]);                            // ...
    csbkValue = (csbkValue << 8) + (g_gitHashBytes[2U]);                            // ...
    csbkValue = (csbkValue << 8) + (g_gitHashBytes[3U]);                            // ...
    csbkValue = (csbkValue << 16) + 0U;
    csbkValue = (csbkValue << 4) + m_siteIdenEntry.channelId();                     // Channel ID
    csbkValue = (csbkValue << 12) + m_logicalCh1;                                   // Channel Number

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/// <summary>
/// Returns a string that represents the current CSBK.
/// </summary>
/// <returns></returns>
std::string CSBK_DVM_GIT_HASH::toString()
{
    return std::string("CSBKO_DVM_GIT_HASH (DVM Git Hash Identifier)");
}
