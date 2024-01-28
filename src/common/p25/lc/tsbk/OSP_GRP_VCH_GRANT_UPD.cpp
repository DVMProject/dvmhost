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
#include "p25/lc/tsbk/OSP_GRP_VCH_GRANT_UPD.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the OSP_GRP_VCH_GRANT_UPD class.
/// </summary>
OSP_GRP_VCH_GRANT_UPD::OSP_GRP_VCH_GRANT_UPD() : TSBK()
{
    m_lco = TSBK_OSP_GRP_VCH_GRANT_UPD;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_GRP_VCH_GRANT_UPD::decode(const uint8_t* data, bool rawTSBK)
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
void OSP_GRP_VCH_GRANT_UPD::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    if (m_grpVchId != 0U) {
        tsbkValue = m_grpVchId;                                                     // Channel ID
    }
    else {
        tsbkValue = m_siteData.channelId();                                         // Channel ID
    }
    tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                     // Channel Number
    tsbkValue = (tsbkValue << 16) + m_dstId;                                        // Talkgroup Address
    tsbkValue = (tsbkValue << 32) + 0;

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string OSP_GRP_VCH_GRANT_UPD::toString(bool isp)
{
    return std::string("TSBK_OSP_GRP_VCH_GRANT_UPD (Group Voice Channel Grant Update)");
}
