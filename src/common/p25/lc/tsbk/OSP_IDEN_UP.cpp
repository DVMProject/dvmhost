// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/lc/tsbk/OSP_IDEN_UP.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

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
    m_lco = TSBKO::OSP_IDEN_UP;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_IDEN_UP::decode(const uint8_t* data, bool rawTSBK)
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
void OSP_IDEN_UP::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    if ((m_siteIdenEntry.chBandwidthKhz() != 0.0F) && (m_siteIdenEntry.chSpaceKhz() != 0.0F) &&
        (m_siteIdenEntry.txOffsetMhz() != 0.0F) && (m_siteIdenEntry.baseFrequency() != 0U)) {
        if (m_siteIdenEntry.baseFrequency() < 762000000U) {
            LogError(LOG_P25, "OSP_IDEN_UP::encode(), invalid values for TSBKO::OSP_IDEN_UP, baseFrequency = %uHz",
                m_siteIdenEntry.baseFrequency());
            return; // blatantly ignore creating this TSBK
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
        LogError(LOG_P25, "OSP_IDEN_UP::encode(), invalid values for TSBKO::OSP_IDEN_UP, baseFrequency = %uHz, txOffsetMhz = %fMHz, chBandwidthKhz = %fKHz, chSpaceKhz = %fKHz",
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
    return std::string("TSBKO, OSP_IDEN_UP (Channel Identifier Update)");
}
