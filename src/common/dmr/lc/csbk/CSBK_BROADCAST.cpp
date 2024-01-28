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
#include "dmr/lc/csbk/CSBK_BROADCAST.h"

using namespace dmr::lc::csbk;
using namespace dmr::lc;
using namespace dmr;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK_BROADCAST class.
/// </summary>
CSBK_BROADCAST::CSBK_BROADCAST() : CSBK(),
    m_anncType(BCAST_ANNC_SITE_PARMS),
    m_hibernating(false),
    m_annWdCh1(false),
    m_annWdCh2(false),
    m_backoffNo(1U)
{
    m_CSBKO = CSBKO_BROADCAST;
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_BROADCAST::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_BROADCAST::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    if (!m_Cdef) {
        csbkValue = m_anncType;                                                     // Announcement Type
    }

    switch (m_anncType)
    {
    case BCAST_ANNC_ANN_WD_TSCC:
        // Broadcast Params 1
        csbkValue = (csbkValue << 4) + 0U;                                          // Reserved
        csbkValue = (csbkValue << 4) + (m_colorCode & 0x0FU);                       // Color Code 1
        csbkValue = (csbkValue << 4) + (m_colorCode & 0x0FU);                       // Color Code 2
        csbkValue = (csbkValue << 1) + ((m_annWdCh1) ? 1U : 0U);                    // Announce/Withdraw Channel 1
        csbkValue = (csbkValue << 1) + ((m_annWdCh2) ? 1U : 0U);                    // Announce/Withdraw Channel 2

        csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U);       // Require Registration
        csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);                       // Backoff Number
        csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();                // Site Identity

        // Broadcast Params 2
        csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                    // Logical Channel 1
        csbkValue = (csbkValue << 12) + (m_logicalCh2 & 0xFFFU);                    // Logical Channel 2
        break;
    case BCAST_ANNC_CHAN_FREQ:
    {
        uint32_t calcSpace = (uint32_t)(m_siteIdenEntry.chSpaceKhz() / 0.125);
        float calcTxOffset = m_siteIdenEntry.txOffsetMhz() * 1000000;
        const uint32_t multiple = 100000;

        // calculate Rx frequency
        uint32_t rxFrequency = (uint32_t)((m_siteIdenEntry.baseFrequency() + ((calcSpace * 125) * m_logicalCh1)) + calcTxOffset);

        // generate frequency in mhz
        uint32_t rxFreqMhz = rxFrequency + multiple / 2;
        rxFreqMhz -= rxFreqMhz % multiple;
        rxFreqMhz /= multiple * 10;

        // generate khz offset
        uint32_t rxFreqKhz = rxFrequency - (rxFreqMhz * 1000000);

        // calculate Tx Frequency
        uint32_t txFrequency = (uint32_t)((m_siteIdenEntry.baseFrequency() + ((calcSpace * 125) * m_logicalCh1)));

        // generate frequency in mhz
        uint32_t txFreqMhz = txFrequency + multiple / 2;
        txFreqMhz -= txFreqMhz % multiple;
        txFreqMhz /= multiple * 10;

        // generate khz offset
        uint32_t txFreqKhz = txFrequency - (txFreqMhz * 1000000);

        csbkValue = 0U;                                                             // Cdef Type (always 0 for ANN_WD_TSCC)
        csbkValue = (csbkValue << 2) + 0U;                                          // Reserved
        csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                    // Logical Channel
        csbkValue = (csbkValue << 10) + (txFreqMhz & 0x7FFU);                       // Transmit Freq Mhz
        csbkValue = (csbkValue << 13) + (txFreqKhz & 0x3FFFU);                      // Transmit Freq Offset Khz
        csbkValue = (csbkValue << 10) + (rxFreqMhz & 0x7FFU);                       // Receive Freq Mhz
        csbkValue = (csbkValue << 13) + (rxFreqKhz & 0x3FFFU);                      // Receive Freq Khz
    }
    break;
    case BCAST_ANNC_SITE_PARMS:
        // Broadcast Params 1
        csbkValue = (csbkValue << 14) + m_siteData.systemIdentity(true);            // Site Identity (Broadcast Params 1)

        csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U);       // Require Registration
        csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);                       // Backoff Number
        csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();                // Site Identity

        // Broadcast Params 2
        csbkValue = (csbkValue << 1) + 0U;                                          // Roaming TG Subscription/Attach
        csbkValue = (csbkValue << 1) + ((m_hibernating) ? 1U : 0U);                 // TSCC Hibernating
        csbkValue = (csbkValue << 22) + 0U;                                         // Broadcast Params 2 (Reserved)
        break;
    }

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/// <summary>
/// Returns a string that represents the current CSBK.
/// </summary>
/// <returns></returns>
std::string CSBK_BROADCAST::toString()
{
    switch (m_anncType) {
    case BCAST_ANNC_ANN_WD_TSCC:    return std::string("CSBKO_BROADCAST (Announcement PDU), BCAST_ANNC_ANN_WD_TSCC (Announce-WD TSCC Channel)");
    case BCAST_ANNC_CHAN_FREQ:      return std::string("CSBKO_BROADCAST (Announcement PDU), BCAST_ANNC_CHAN_FREQ (Logical Channel/Frequency)");
    case BCAST_ANNC_SITE_PARMS:     return std::string("CSBKO_BROADCAST (Announcement PDU), BCAST_ANNC_SITE_PARMS (General Site Parameters)");
    default:                        return std::string("CSBKO_BROADCAST (Announcement PDU)");
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void CSBK_BROADCAST::copy(const CSBK_BROADCAST& data)
{
    CSBK::copy(data);

    m_anncType = data.m_anncType;
    m_hibernating = data.m_hibernating;

    m_annWdCh1 = data.m_annWdCh1;
    m_annWdCh2 = data.m_annWdCh2;
}
