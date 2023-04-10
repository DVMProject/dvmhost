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
#include "p25/lc/tsbk/IOSP_UU_VCH.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the IOSP_UU_VCH class.
/// </summary>
IOSP_UU_VCH::IOSP_UU_VCH() : TSBK()
{
    m_lco = TSBK_IOSP_UU_VCH;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool IOSP_UU_VCH::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_emergency = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                   // Emergency Flag
    m_encrypted = (((tsbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                   // Encryption Flag
    m_priority = (((tsbkValue >> 56) & 0xFFU) & 0x07U);                             // Priority
    m_grpVchId = ((tsbkValue >> 52) & 0x0FU);                                       // Channel ID
    m_grpVchNo = ((tsbkValue >> 40) & 0xFFFU);                                      // Channel Number
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                            // Target ID
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void IOSP_UU_VCH::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    ulong64_t tsbkValue = 0U;

    tsbkValue =
        (m_emergency ? 0x80U : 0x00U) +                                             // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                             // Encrypted Flag
        (m_priority & 0x07U);                                                       // Priority
    tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                          // Channel ID
    tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                     // Channel Number
    tsbkValue = (tsbkValue << 24) + m_dstId;                                        // Target ID
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}
