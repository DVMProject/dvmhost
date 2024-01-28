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
#include "p25/lc/tdulc/LC_CALL_TERM.h"

using namespace p25::lc::tdulc;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the LC_CALL_TERM class.
/// </summary>
LC_CALL_TERM::LC_CALL_TERM() : TDULC()
{
    m_lco = p25::LC_CALL_TERM;
}

/// <summary>
/// Decode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
bool LC_CALL_TERM::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
void LC_CALL_TERM::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t rsValue = 0U;

    m_implicit = true;

    rsValue = 0U;
    rsValue = (rsValue << 24) + P25_WUID_FNE;                                       // System Radio Address

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}
