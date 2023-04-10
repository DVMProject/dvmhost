/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Jason-UWU
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
#include "p25/lc/tsbk/OSP_TIME_DATE_ANN.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>
#include <chrono>
#include <ctime>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the OSP_TIME_DATE_ANN class.
/// </summary>
OSP_TIME_DATE_ANN::OSP_TIME_DATE_ANN() : TSBK()
{
    m_lco = TSBK_OSP_TIME_DATE_ANN;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_TIME_DATE_ANN::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

    /* stub */

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void OSP_TIME_DATE_ANN::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    ulong64_t tsbkValue = 0U;

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    tm local_tm = *gmtime(&tt);

    uint32_t tmM = (local_tm.tm_mon + 1);
    uint32_t tmY = (local_tm.tm_year - 100);

    uint32_t tmS = 0U;
    uint32_t i = local_tm.tm_sec;
    if (i > 59U) {
        tmS |= 59U;
    } else {
        tmS |= i;
    }

#if DEBUG_P25_TSBK
    LogDebug(LOG_P25, "TSBK_OSP_TIME_DATE_ANN, tmM = %u / %u, tmY = %u / %u", local_tm.tm_mon, tmM, local_tm.tm_year, tmY);
#endif

    uint8_t lto = fabs(m_siteData.lto()) * 2U; // this will cause a bug for half-hour timezone intervals...

    // mark the LTO as valid if its non-zero
    bool vl = false;
    if (lto > 0U)
        vl = true;

    tsbkValue = 0xC0U +                                                         // VD and VT flags set
                (vl ? 0x20U : 0x00U) +                                          // Valid LTO Flag
                ((lto >> 8) & 0x0F);                                            // LTO MSB (Upper 4-bits)
    tsbkValue = (tsbkValue << 8) + (lto & 0xFFU);                               // LTO LSB

    // Date
    tsbkValue = (tsbkValue << 4) + (tmM & 0x0FU);                               // Month
    tsbkValue = (tsbkValue << 5) + (local_tm.tm_mday & 0x1FU);                  // Day of Month
    tsbkValue = (tsbkValue << 13) + (tmY & 0x1FFFU);                            // Year
    tsbkValue = (tsbkValue << 2);                                               // Reserved

    // Time
    tsbkValue = (tsbkValue << 5) + (local_tm.tm_hour & 0x1FU);                  // Hour
    tsbkValue = (tsbkValue << 6) + (local_tm.tm_min & 0x3FU);                   // Minute
    tsbkValue = (tsbkValue << 6) + (local_tm.tm_sec & 0x3FU);                   // Seconds
    tsbkValue = (tsbkValue << 7);                                               // Reserved

#if DEBUG_P25_TSBK
    LogDebug(LOG_P25, "TSBK_OSP_TIME_DATE_ANN, tmM = %u, tmMDAY = %u, tmY = %u, tmH = %u, tmMin = %u, tmS = %u", tmM, tmMDAY, tmY, tmH, tmMin, tmS);
#endif

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}
