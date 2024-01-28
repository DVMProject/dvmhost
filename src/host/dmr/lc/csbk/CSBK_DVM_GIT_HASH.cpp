// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
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
