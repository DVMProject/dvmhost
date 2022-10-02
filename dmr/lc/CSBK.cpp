/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2019-2021 by Bryan Biedenkapp N2PLL
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
#include "dmr/lc/CSBK.h"
#include "edac/BPTC19696.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr::lc;
using namespace dmr;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK class.
/// </summary>
/// <param name="siteData"></param>
/// <param name="entry"></param>
CSBK::CSBK(SiteData siteData, lookups::IdenTable entry) : CSBK(siteData)
{
    m_siteIdenEntry = entry;
}

/// <summary>
/// Initializes a new instance of the CSBK class.
/// </summary>
/// <param name="siteData"></param>
/// <param name="entry"></param>
/// <param name="verbose"></param>
CSBK::CSBK(SiteData siteData, lookups::IdenTable entry, bool verbose) : CSBK(siteData)
{
    m_verbose = verbose;
    m_siteIdenEntry = entry;
}

/// <summary>
/// Finalizes a instance of the CSBK class.
/// </summary>
CSBK::~CSBK()
{
    /* stub */
}

/// <summary>
/// Decodes a DMR CSBK.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if DMR CSBK was decoded, otherwise false.</returns>
bool CSBK::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(data, csbk);

    // validate the CRC-CCITT 16
    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    bool valid = edac::CRC::checkCCITT162(csbk, DMR_CSBK_LENGTH_BYTES);
    if (!valid) {
        LogError(LOG_DMR, "CSBK::decode(), failed CRC CCITT-162 check");
        return false;
    }

    // restore the checksum
    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    if (m_verbose) {
        Utils::dump(2U, "Decoded CSBK", csbk, DMR_CSBK_LENGTH_BYTES);
    }

    m_CSBKO = csbk[0U] & 0x3FU;                                                     // CSBKO
    m_lastBlock = (csbk[0U] & 0x80U) == 0x80U;                                      // Last Block Marker
    m_FID = csbk[1U];                                                               // Feature ID

    m_dataContent = false;
    m_CBF = 0U;

    ulong64_t csbkValue = 0U;

    // combine bytes into ulong64_t (8 byte) value
    csbkValue = csbk[2U];
    csbkValue = (csbkValue << 8) + csbk[3U];
    csbkValue = (csbkValue << 8) + csbk[4U];
    csbkValue = (csbkValue << 8) + csbk[5U];
    csbkValue = (csbkValue << 8) + csbk[6U];
    csbkValue = (csbkValue << 8) + csbk[7U];
    csbkValue = (csbkValue << 8) + csbk[8U];
    csbkValue = (csbkValue << 8) + csbk[9U];

    switch (m_CSBKO) {
    case CSBKO_BSDWNACT:
        m_bsId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                         // Base Station Address
        m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case CSBKO_UU_V_REQ:
        m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case CSBKO_UU_ANS_RSP:
        m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case CSBKO_PRECCSBK:
        m_GI = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                      // Group/Individual Flag
        m_dataContent = (((csbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;             //
        m_CBF = (uint8_t)((csbkValue >> 48) & 0xFFU);                               // Blocks to Follow
        m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case CSBKO_RAND: // CSBKO_CALL_ALRT when FID == FID_DMRA
        switch (m_FID)
        {
        case FID_DMRA:
            m_GI = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                  // Group/Individual Flag
            m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                      // Target Radio Address
            m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                            // Source Radio Address
            break;
        case FID_ETSI:
        default:
            m_emergency = (((csbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;           // Emergency Flag
            m_privacy = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;             // Privacy Flag
            m_supplementData = (((csbkValue >> 56) & 0xFFU) & 0x20U) == 0x20U;      // Supplementary Data Flag
            m_broadcast = (((csbkValue >> 56) & 0xFFU) & 0x10U) == 0x10U;           // Broadcast Flag
            m_priority = (((csbkValue >> 56) & 0xFFU) & 0x03U);                     // Priority
            m_serviceData = (uint8_t)((csbkValue >> 52U) & 0x0FU);                  // Service Data
            m_serviceType = (uint8_t)((csbkValue >> 48U) & 0x0FU);                  // Service Type
            m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                      // Target Radio Address
            m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                            // Source Radio Address
            break;
        }
    case CSBKO_EXT_FNCT:
        m_dataContent = (((csbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;             //
        m_serviceType = (uint8_t)((csbkValue >> 48) & 0xFFU);                       // Service Type
        m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case CSBKO_NACK_RSP:
        m_GI = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                      // Group/Individual Flag
        m_serviceType = (((csbkValue >> 56) & 0xFFU) & 0x3FU);                      // Service Type
        m_reason = (uint8_t)((csbkValue >> 48) & 0xFFU);                            // Reason Code
        m_srcId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                          // Source Radio Address
        m_dstId = (uint32_t)(csbkValue & 0xFFFFFFU);                                // Target Radio Address
        break;

    /** Tier 3 */
    case CSBKO_ACK_RSP:
        m_GI = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                      // Group/Individual Flag
        m_reason = (uint8_t)((csbkValue >> 33) & 0xFFU);                            // Reason Code
        m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;

    default:
        LogError(LOG_DMR, "CSBK::decode(), unknown CSBK type, csbko = $%02X", m_CSBKO);
        return true;
    }

    return true;
}

/// <summary>
/// Encodes a DMR CSBK.
/// </summary>
/// <param name="data"></param>
void CSBK::encode(uint8_t* data)
{
    assert(data != NULL);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    ulong64_t csbkValue = 0U;
    csbk[0U] = m_CSBKO;                                                             // CSBKO
    csbk[0U] |= (m_lastBlock) ? 0x80U : 0x00U;                                      // Last Block Marker
    if (!m_Cdef) {
        csbk[1U] = m_FID;                                                           // Feature ID
    }
    else {
        csbk[1U] = m_colorCode & 0x0FU;                                             // Cdef uses Color Code
    }

    switch (m_CSBKO) {
    case CSBKO_EXT_FNCT:
        csbkValue = 
            (m_GI ? 0x40U : 0x00U) +                                                // Group or Invididual
            (m_dataContent ? 0x80U : 0x00U);
        csbkValue = (csbkValue << 8) + m_CBF;                                       // Blocks to Follow
        csbkValue = (csbkValue << 24) + m_srcId;                                    // Source Radio Address
        csbkValue = (csbkValue << 24) + m_dstId;                                    // Target Radio Address
        break;

    case CSBKO_NACK_RSP:
        csbkValue = 0x80U +                                                         // Additional Information Field (always 1)
            (m_GI ? 0x40U : 0x00U) +                                                // Source Type
            (m_serviceType & 0x3FU);                                                // Service Type
        csbkValue = (csbkValue << 8) + m_reason;                                    // Reason Code
        csbkValue = (csbkValue << 24) + m_srcId;                                    // Source Radio Address
        csbkValue = (csbkValue << 24) + m_dstId;                                    // Target Radio Address
        break;

    /* Tier III */
    case CSBKO_ACK_RSP:
        if (m_reason == TS_ACK_RSN_REG) {
            csbkValue = 0U;
        } else {
            csbkValue = (m_GI ? 0x40U : 0x00U) +                                    // Source Type
                (m_siteData.siteId() & 0x3FU);                                      // Net + Site LSB
        }
        csbkValue = (csbkValue << 7) + (m_reason & 0xFFU);                          // Reason Code
        csbkValue = (csbkValue << 25) + m_dstId;                                    // Target Radio Address
        csbkValue = (csbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;

    case CSBKO_ALOHA:
        csbkValue = 0U;
        csbkValue = (csbkValue << 2) + 0U;                                          // Reserved
        csbkValue = (csbkValue << 1) + ((m_siteTSSync) ? 1U : 0U);                  // Site Time Slot Synchronization
        csbkValue = (csbkValue << 3) + DMR_ALOHA_VER_151;                           // DMR Spec. Version (1.5.1)
        csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);            // Site Timing: Aligned or Offset
        csbkValue = (csbkValue << 1) + ((m_siteData.netActive()) ? 1U : 0U);        // Site Networked
        csbkValue = (csbkValue << 5) + (m_alohaMask & 0x1FU);                       // MS Mask
        csbkValue = (csbkValue << 2) + 0U;                                          // Service Function
        csbkValue = (csbkValue << 4) + 0U;                                          // 
        csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U);       // Require Registration
        csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);                       // Backoff Number
        csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();                // Site Identity
        csbkValue = (csbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;

    case CSBKO_PV_GRANT:
        csbkValue = 0U;
        csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                    // Logical Physical Channel 1
        csbkValue = (csbkValue << 1) + (m_slotNo & 0x3U);                           // Logical Slot Number
        csbkValue = (csbkValue << 1) + 0U;                                          // Reserved
        csbkValue = (csbkValue << 1) + 0U;                                          // Emergency
        csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);            // Site Timing: Aligned or Offset
        csbkValue = (csbkValue << 24) + m_dstId;                                    // Talkgroup ID
        csbkValue = (csbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;

    case CSBKO_TV_GRANT:
    case CSBKO_BTV_GRANT:
        csbkValue = 0U;
        csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                    // Logical Physical Channel 1
        csbkValue = (csbkValue << 1) + (m_slotNo & 0x3U);                           // Logical Slot Number
        csbkValue = (csbkValue << 1) + 0U;                                          // Late Entry
        csbkValue = (csbkValue << 1) + 0U;                                          // Emergency
        csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);            // Site Timing: Aligned or Offset
        csbkValue = (csbkValue << 24) + m_dstId;                                    // Talkgroup ID
        csbkValue = (csbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;

    case CSBKO_BROADCAST:
    {
        csbkValue = 0U;
        if (!m_Cdef) {
            csbkValue = m_anncType;                                                 // Announcement Type
        }

        switch (m_anncType)
        {
        case BCAST_ANNC_ANN_WD_TSCC:
            // Broadcast Parms 1
            csbkValue = (csbkValue << 4) + 0U;                                      // Reserved
            csbkValue = (csbkValue << 4) + (m_colorCode & 0x0FU);                   // Color Code 1
            csbkValue = (csbkValue << 4) + (m_colorCode & 0x0FU);                   // Color Code 2
            csbkValue = (csbkValue << 1) + ((m_annWdCh1) ? 1U : 0U);                // Announce/Withdraw Channel 1
            csbkValue = (csbkValue << 1) + ((m_annWdCh2) ? 1U : 0U);                // Announce/Withdraw Channel 2

            csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U);   // Require Registration
            csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);                   // Backoff Number
            csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();            // Site Identity

            // Broadcast Parms 2
            csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                // Logical Channel 1
            csbkValue = (csbkValue << 12) + (m_logicalCh2 & 0xFFFU);                // Logical Channel 2
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

            csbkValue = 0U;                                                         // Cdef Type (always 0 for ANN_WD_TSCC)
            csbkValue = (csbkValue << 2) + 0U;                                      // Reserved
            csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                // Logical Channel
            csbkValue = (csbkValue << 10) + (txFreqMhz & 0x7FFU);                   // Transmit Freq Mhz
            csbkValue = (csbkValue << 13) + (txFreqKhz & 0x3FFFU);                  // Transmit Freq Offset Khz
            csbkValue = (csbkValue << 10) + (rxFreqMhz & 0x7FFU);                   // Receive Freq Mhz
            csbkValue = (csbkValue << 13) + (rxFreqKhz & 0x3FFFU);                  // Receive Freq Khz
        }
        break;
        case BCAST_ANNC_SITE_PARMS:
            // Broadcast Parms 1
            csbkValue = (csbkValue << 14) + m_siteData.systemIdentity(true);        // Site Identity (Broadcast Parms 1)

            csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U);   // Require Registration
            csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);                   // Backoff Number
            csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();            // Site Identity

            // Broadcast Parms 2
            csbkValue = (csbkValue << 1) + 0U;                                      // Roaming TG Subscription/Attach
            csbkValue = (csbkValue << 1) + ((m_hibernating) ? 1U : 0U);             // TSCC Hibernating
            csbkValue = (csbkValue << 22) + 0U;                                     // Broadcast Parms 2 (Reserved)
            break;
        }
    }
    break;

    default:
        csbkValue = 
            (m_GI ? 0x40U : 0x00U) +                                                // Group or Invididual
            (m_dataContent ? 0x80U : 0x00U);
        csbkValue = (csbkValue << 8) + m_CBF;                                       // Blocks to Follow
        csbkValue = (csbkValue << 24) + m_srcId;                                    // Source Radio Address
        csbkValue = (csbkValue << 24) + m_dstId;                                    // Target Radio Address

        if ((m_FID == FID_ETSI) || (m_FID == FID_DMRA)) {
            LogError(LOG_DMR, "CSBK::encode(), unknown CSBK type, csbko = $%02X", m_CSBKO);
        }
        break;
    }

    // internal DMR vendor opcodes
    if (m_FID == FID_DVM) {
        switch (m_CSBKO) {
        case CSBKO_DVM_GIT_HASH:
            csbkValue = 0U;
            csbkValue = g_gitHashBytes[0];                                          // ...
            csbkValue = (csbkValue << 8) + (g_gitHashBytes[1U]);                    // ...
            csbkValue = (csbkValue << 8) + (g_gitHashBytes[2U]);                    // ...
            csbkValue = (csbkValue << 8) + (g_gitHashBytes[3U]);                    // ...
            csbkValue = (csbkValue << 16) + 0U;
            csbkValue = (csbkValue << 4) + m_siteIdenEntry.channelId();             // Channel ID
            csbkValue = (csbkValue << 12) + m_logicalCh1;                           // Channel Number
            break;
        default:
            csbkValue = 
                (m_GI ? 0x40U : 0x00U) +                                                // Group or Invididual
                (m_dataContent ? 0x80U : 0x00U);
            csbkValue = (csbkValue << 8) + m_CBF;                                       // Blocks to Follow
            csbkValue = (csbkValue << 24) + m_srcId;                                    // Source Radio Address
            csbkValue = (csbkValue << 24) + m_dstId;                                    // Target Radio Address
            LogError(LOG_DMR, "CSBK::encode(), unknown CSBK type, csbko = $%02X", m_CSBKO);
            break;
        }
    }

    // split ulong64_t (8 byte) value into bytes
    csbk[2U] = (uint8_t)((csbkValue >> 56) & 0xFFU);
    csbk[3U] = (uint8_t)((csbkValue >> 48) & 0xFFU);
    csbk[4U] = (uint8_t)((csbkValue >> 40) & 0xFFU);
    csbk[5U] = (uint8_t)((csbkValue >> 32) & 0xFFU);
    csbk[6U] = (uint8_t)((csbkValue >> 24) & 0xFFU);
    csbk[7U] = (uint8_t)((csbkValue >> 16) & 0xFFU);
    csbk[8U] = (uint8_t)((csbkValue >> 8) & 0xFFU);
    csbk[9U] = (uint8_t)((csbkValue >> 0) & 0xFFU);

    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    edac::CRC::addCCITT162(csbk, 12U);

    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    if (m_verbose) {
        Utils::dump(2U, "Encoded CSBK", csbk, DMR_CSBK_LENGTH_BYTES);
    }

    // encode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.encode(csbk, data);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK class.
/// </summary>
/// <param name="siteData"></param>
CSBK::CSBK(SiteData siteData) :
    m_verbose(false),
    m_colorCode(0U),
    m_lastBlock(true),
    m_Cdef(false),
    m_CSBKO(CSBKO_NONE),
    m_FID(0x00U),
    m_GI(false),
    m_bsId(0U),
    m_srcId(0U),
    m_dstId(0U),
    m_dataContent(false),
    m_CBF(0U),
    m_emergency(false),
    m_privacy(false),
    m_supplementData(false),
    m_priority(0U),
    m_broadcast(false),
    m_backoffNo(1U),
    m_serviceData(0U),
    m_serviceType(0U),
    m_targetAddress(TGT_ADRS_TGID),
    m_response(0U),
    m_reason(0U),
    m_anncType(BCAST_ANNC_SITE_PARMS),
    m_hibernating(false),
    m_annWdCh1(false),
    m_logicalCh1(DMR_CHNULL),
    m_annWdCh2(false),
    m_logicalCh2(DMR_CHNULL),
    m_slotNo(0U),
    m_siteTSSync(false),
    m_siteOffsetTiming(false),
    m_alohaMask(0U),
    m_siteData(siteData),
    m_siteIdenEntry(lookups::IdenTable())
{
    /* stub */
}
