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
#include "p25/lc/tsbk/OSP_SNDCP_CH_GNT.h"
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
/// Initializes a new instance of the OSP_SNDCP_CH_GNT class.
/// </summary>
OSP_SNDCP_CH_GNT::OSP_SNDCP_CH_GNT() : TSBK(),
    m_dataServiceOptions(0U),
    m_dataChannelNo(0U)
{
    m_lco = TSBK_OSP_SNDCP_CH_GNT;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_SNDCP_CH_GNT::decode(const uint8_t* data, bool rawTSBK)
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
void OSP_SNDCP_CH_GNT::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    ulong64_t tsbkValue = 0U;

    uint32_t calcSpace = (uint32_t)(m_siteIdenEntry.chSpaceKhz() / 0.125);
    float calcTxOffset = m_siteIdenEntry.txOffsetMhz() * 1000000;

    uint32_t txFrequency = (uint32_t)((m_siteIdenEntry.baseFrequency() + ((calcSpace * 125) * m_dataChannelNo)));
    uint32_t rxFrequency = (uint32_t)(txFrequency + calcTxOffset);

    uint32_t rootFreq = rxFrequency - m_siteIdenEntry.baseFrequency();
    uint32_t rxChNo = rootFreq / (m_siteIdenEntry.chSpaceKhz() * 1000);

    tsbkValue = 0U;
    tsbkValue = (tsbkValue << 8) + m_dataServiceOptions;                            // Data Service Options
    if (m_grpVchId != 0U) {
        tsbkValue = (tsbkValue << 4) + m_grpVchId;                                  // Channel (T) ID
    }
    else {
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel (T) ID
    }
    tsbkValue = (tsbkValue << 12) + m_dataChannelNo;                                // Channel (T) Number
    if (m_grpVchId != 0U) {
        tsbkValue = (tsbkValue << 4) + m_grpVchId;                                  // Channel (R) ID
    }
    else {
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel (R) ID
    }
    tsbkValue = (tsbkValue << 12) + (rxChNo & 0xFFFU);                              // Channel (R) Number
    tsbkValue = (tsbkValue << 24) + m_dstId;                                        // Target Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string OSP_SNDCP_CH_GNT::toString(bool isp)
{
    return std::string("TSBK_OSP_SNDCP_CH_GNT (SNDCP Data Channel Grant)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void OSP_SNDCP_CH_GNT::copy(const OSP_SNDCP_CH_GNT& data)
{
    TSBK::copy(data);

    m_dataServiceOptions = data.m_dataServiceOptions;
    m_dataChannelNo = data.m_dataChannelNo;
}
