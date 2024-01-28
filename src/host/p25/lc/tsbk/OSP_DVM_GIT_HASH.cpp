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
#include "p25/lc/tsbk/OSP_DVM_GIT_HASH.h"
#include "HostMain.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the OSP_DVM_GIT_HASH class.
/// </summary>
OSP_DVM_GIT_HASH::OSP_DVM_GIT_HASH() : TSBK()
{
    m_lco = TSBK_OSP_DVM_GIT_HASH;
    m_mfId = P25_MFG_DVM_OCS;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_DVM_GIT_HASH::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void OSP_DVM_GIT_HASH::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = g_gitHashBytes[0];                                                  // ...
    tsbkValue = (tsbkValue << 8) + (g_gitHashBytes[1U]);                            // ...
    tsbkValue = (tsbkValue << 8) + (g_gitHashBytes[2U]);                            // ...
    tsbkValue = (tsbkValue << 8) + (g_gitHashBytes[3U]);                            // ...
    tsbkValue = (tsbkValue << 16) + 0U;
    tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                          // Channel ID
    tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                         // Channel Number

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string OSP_DVM_GIT_HASH::toString(bool isp)
{
    return std::string("TSBK_OSP_DVM_GIT_HASH (DVM Git Hash Identifier)");
}
