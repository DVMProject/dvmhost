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
#include "p25/lc/tsbk/OSP_IDEN_UP.h"
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
/// Initializes a new instance of the OSP_IDEN_UP class.
/// </summary>
OSP_IDEN_UP::OSP_IDEN_UP() : TSBK()
{
    m_lco = TSBK_OSP_IDEN_UP;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_IDEN_UP::decode(const uint8_t* data, bool rawTSBK)
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
void OSP_IDEN_UP::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    ulong64_t tsbkValue = 0U;

    if ((m_siteIdenEntry.chBandwidthKhz() != 0.0F) && (m_siteIdenEntry.chSpaceKhz() != 0.0F) &&
        (m_siteIdenEntry.txOffsetMhz() != 0.0F) && (m_siteIdenEntry.baseFrequency() != 0U)) {
        if (m_siteIdenEntry.baseFrequency() < 762000000U) {
            LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_IDEN_UP, baseFrequency = %uHz",
                m_siteIdenEntry.baseFrequency());
            return; // blatently ignore creating this TSBK
        }

        uint32_t calcSpace = (uint32_t)(m_siteIdenEntry.chSpaceKhz() / 0.125);

        float fCalcTxOffset = (fabs(m_siteIdenEntry.txOffsetMhz()) * 1000000.0F) / 250000.0F;
        uint32_t uCalcTxOffset = (uint32_t)fCalcTxOffset;
        if (m_siteIdenEntry.txOffsetMhz() > 0.0F)
            uCalcTxOffset |= 0x2000U; // this sets a positive offset ...

        uint32_t calcBaseFreq = (uint32_t)(m_siteIdenEntry.baseFrequency() / 5);
        uint16_t chanBw = (uint16_t)((m_siteIdenEntry.chBandwidthKhz() * 1000) / 125);

        tsbkValue = m_siteIdenEntry.channelId();                                    // Channel ID
        tsbkValue = (tsbkValue << 9) + chanBw;                                      // Channel Bandwidth
        tsbkValue = (tsbkValue << 9) + uCalcTxOffset;                               // Transmit Offset
        tsbkValue = (tsbkValue << 10) + calcSpace;                                  // Channel Spacing
        tsbkValue = (tsbkValue << 32) + calcBaseFreq;                               // Base Frequency
    }
    else {
        LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_IDEN_UP, baseFrequency = %uHz, txOffsetMhz = %fMHz, chBandwidthKhz = %fKHz, chSpaceKhz = %fKHz",
            m_siteIdenEntry.baseFrequency(), m_siteIdenEntry.txOffsetMhz(), m_siteIdenEntry.chBandwidthKhz(),
            m_siteIdenEntry.chSpaceKhz());
        return; // blatently ignore creating this TSBK
    }

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string OSP_IDEN_UP::toString(bool isp)
{
    return std::string("TSBK_OSP_IDEN_UP (Channel Identifier Update)");
}
