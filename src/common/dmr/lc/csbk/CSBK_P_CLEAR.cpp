// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "dmr/lc/csbk/CSBK_P_CLEAR.h"

using namespace dmr::lc::csbk;
using namespace dmr::lc;
using namespace dmr;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK_P_CLEAR class.
/// </summary>
CSBK_P_CLEAR::CSBK_P_CLEAR() : CSBK()
{
    m_CSBKO = CSBKO_P_CLEAR;
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_P_CLEAR::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_P_CLEAR::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                        // Logical Physical Channel 1
    csbkValue = (csbkValue << 1) + 0U;                                              // Reserved
    csbkValue = (csbkValue << 3) + 0U;                                              // Reserved
    csbkValue = (csbkValue << 1) + ((m_GI) ? 1U : 0U);                              // Group/Individual Flag
    csbkValue = (csbkValue << 24) + m_dstId;                                        // Talkgroup ID
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/// <summary>
/// Returns a string that represents the current CSBK.
/// </summary>
/// <returns></returns>
std::string CSBK_P_CLEAR::toString()
{
    return std::string("CSBKO_P_CLEAR (Payload Channel Clear)");
}
