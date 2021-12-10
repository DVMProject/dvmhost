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
    delete[] m_data;
}

/// <summary>
/// Decodes a DMR CSBK.
/// </summary>
/// <param name="bytes"></param>
/// <returns>True, if DMR CSBK was decoded, otherwise false.</returns>
bool CSBK::decode(const uint8_t* bytes)
{
    assert(bytes != NULL);

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(bytes, m_data);

    // validate the CRC-CCITT 16
    m_data[10U] ^= CSBK_CRC_MASK[0U];
    m_data[11U] ^= CSBK_CRC_MASK[1U];

    bool valid = edac::CRC::checkCCITT162(m_data, DMR_LC_HEADER_LENGTH_BYTES);
    if (!valid) {
        LogError(LOG_DMR, "CSBK::decode(), failed CRC CCITT-162 check");
        return false;
    }

    // restore the checksum
    m_data[10U] ^= CSBK_CRC_MASK[0U];
    m_data[11U] ^= CSBK_CRC_MASK[1U];

    if (m_verbose) {
        Utils::dump(2U, "Decoded CSBK", m_data, DMR_LC_HEADER_LENGTH_BYTES);
    }

    m_CSBKO = m_data[0U] & 0x3FU;                                                   // CSBKO
    m_lastBlock = (m_data[0U] & 0x80U) == 0x80U;                                    // Last Block Marker
    m_FID = m_data[1U];                                                             // Feature ID

    switch (m_CSBKO) {
    case CSBKO_BSDWNACT:
        m_GI = false;
        m_bsId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];                   // Base Station ID
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];                  // Source ID
        m_dataContent = false;
        m_CBF = 0U;
        break;

    case CSBKO_UU_V_REQ:
        m_GI = false;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];                  // Destination ID
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];                  // Source ID
        m_dataContent = false;
        m_CBF = 0U;
        break;

    case CSBKO_UU_ANS_RSP:
        m_GI = false;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];                  // Destination ID
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];                  // Source ID
        m_dataContent = false;
        m_CBF = 0U;
        break;

    case CSBKO_PRECCSBK:
        m_GI = (m_data[2U] & 0x40U) == 0x40U;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];                  // Destination ID
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];                  // Source ID
        m_dataContent = (m_data[2U] & 0x80U) == 0x80U;
        m_CBF = m_data[3U];
        break;

    case CSBKO_RAND: // CSBKO_CALL_ALRT when FID == FID_DMRA
        switch (m_FID)
        {
        case FID_DMRA:
            m_GI = (m_data[2U] & 0x40U) == 0x40U;                                   // Group or Individual
            m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];              // Destination ID
            m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];              // Source ID
            m_dataContent = (m_data[2U] & 0x80U) == 0x80U;                          //
            m_CBF = m_data[3U];                                                     // 
            break;

        /* Tier III */
        case FID_ETSI:
        default:
            m_serviceOptions = m_data[2U];                                          // Service Options
            m_targetAddress = (m_data[3U] >> 6 & 0x03U);                            // Target Address
            m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];              // Destination ID
            m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];              // Source ID
            break;
        }

    case CSBKO_ACK_RSP:
        m_GI = (m_data[2U] & 0x40U) == 0x40U;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];                  // Destination ID
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];                  // Source ID
        m_response = m_data[3U];                                                    // Response
        m_dataContent = false;
        break;

    case CSBKO_EXT_FNCT:
        m_GI = false;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];                  // Destination ID
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];                  // Source ID
        m_dataContent = (m_data[2U] & 0x80U) == 0x80U;                              //
        m_CBF = m_data[3U];                                                         //
        break;

    case CSBKO_NACK_RSP:
        m_GI = false;
        m_srcId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];                  // Source ID
        m_dstId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];                  // Destination ID
        m_response = m_data[3U];                                                    // Response
        m_dataContent = false;
        break;

    default:
        m_GI = false;
        m_srcId = 0U;
        m_dstId = 0U;
        m_dataContent = false;
        m_CBF = 0U;
        LogError(LOG_DMR, "CSBK::decode(), unknown CSBK type, csbko = $%02X", m_CSBKO);
        return true;
    }

    return true;
}

/// <summary>
/// Encodes a DMR CSBK.
/// </summary>
/// <param name="bytes"></param>
void CSBK::encode(uint8_t* bytes)
{
    assert(bytes != NULL);

    m_data[0U] = m_CSBKO;                                                           // CSBKO
    m_data[0U] |= (m_lastBlock) ? 0x80U : 0x00U;                                    // Last Block Marker
    if (!m_Cdef) {
        m_data[1U] = m_FID;                                                         // Feature ID
    }
    else {
        m_data[1U] = m_colorCode & 0x0FU;                                           // Cdef uses Color Code
    }

    switch (m_CSBKO) {
    case CSBKO_ACK_RSP:
        m_data[2U] = (uint8_t)(m_serviceType & 0x3FU);                              // Service Type
        m_data[2U] |= 0x80U;                                                        // Additional Information Field (always 1)
        if (m_GI) {
            m_data[2U] |= 0x40U;                                                    // Source Type
        }
        m_data[3U] = m_response;                                                    // Reason Code

        m_data[4U] = (m_srcId >> 16) & 0xFFU;                                       // Source ID
        m_data[5U] = (m_srcId >> 8) & 0xFFU;
        m_data[6U] = (m_srcId >> 0) & 0xFFU;

        m_data[7U] = (m_dstId >> 16) & 0xFFU;                                       // Destination ID
        m_data[8U] = (m_dstId >> 8) & 0xFFU;
        m_data[9U] = (m_dstId >> 0) & 0xFFU;
        break;

    case CSBKO_EXT_FNCT:
        if (m_GI) {
            m_data[2U] |= 0x40U;                                                    // Group or Individual
        }

        if (m_dataContent) {
            m_data[2U] |= 0x80U;                                                    //
        }

        m_data[3U] = m_CBF;                                                         //

        m_data[4U] = (m_srcId >> 16) & 0xFFU;                                       // Source ID
        m_data[5U] = (m_srcId >> 8) & 0xFFU;
        m_data[6U] = (m_srcId >> 0) & 0xFFU;

        m_data[7U] = (m_dstId >> 16) & 0xFFU;                                       // Destination ID
        m_data[8U] = (m_dstId >> 8) & 0xFFU;
        m_data[9U] = (m_dstId >> 0) & 0xFFU;
        break;

    case CSBKO_NACK_RSP:
        m_data[2U] = (uint8_t)(m_serviceType & 0x3FU);                              // Service Type
        m_data[2U] |= 0x80U;                                                        // Additional Information Field (always 1)
        if (m_GI) {
            m_data[2U] |= 0x40U;                                                    // Source Type
        }
        m_data[3U] = m_response;                                                    // Reason Code

        m_data[4U] = (m_srcId >> 16) & 0xFFU;                                       // Source ID
        m_data[5U] = (m_srcId >> 8) & 0xFFU;
        m_data[6U] = (m_srcId >> 0) & 0xFFU;

        m_data[7U] = (m_dstId >> 16) & 0xFFU;                                       // Destination ID
        m_data[8U] = (m_dstId >> 8) & 0xFFU;
        m_data[9U] = (m_dstId >> 0) & 0xFFU;
        break;

    /* Tier III */
    case CSBKO_ALOHA:
        {
            ulong64_t csbkValue = 0U;
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
            csbkValue = (csbkValue << 24) + m_srcId;                                    // Source ID

            // split value into bytes
            m_data[2U] = (uint8_t)((csbkValue >> 56) & 0xFFU);
            m_data[3U] = (uint8_t)((csbkValue >> 48) & 0xFFU);
            m_data[4U] = (uint8_t)((csbkValue >> 40) & 0xFFU);
            m_data[5U] = (uint8_t)((csbkValue >> 32) & 0xFFU);
            m_data[6U] = (uint8_t)((csbkValue >> 24) & 0xFFU);
            m_data[7U] = (uint8_t)((csbkValue >> 16) & 0xFFU);
            m_data[8U] = (uint8_t)((csbkValue >> 8) & 0xFFU);
            m_data[9U] = (uint8_t)((csbkValue >> 0) & 0xFFU);
        }
        break;

    case CSBKO_PV_GRANT:
        {
            ulong64_t csbkValue = 0U;
            csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                    // Logical Physical Channel 1
            csbkValue = (csbkValue << 1) + (m_slotNo & 0x3U);                           // Logical Slot Number
            csbkValue = (csbkValue << 1) + 0U;                                          // Reserved
            csbkValue = (csbkValue << 1) + 0U;                                          // Emergency
            csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);            // Site Timing: Aligned or Offset
            csbkValue = (csbkValue << 24) + m_dstId;                                    // Talkgroup ID
            csbkValue = (csbkValue << 24) + m_srcId;                                    // Source ID

            // split value into bytes
            m_data[2U] = (uint8_t)((csbkValue >> 56) & 0xFFU);
            m_data[3U] = (uint8_t)((csbkValue >> 48) & 0xFFU);
            m_data[4U] = (uint8_t)((csbkValue >> 40) & 0xFFU);
            m_data[5U] = (uint8_t)((csbkValue >> 32) & 0xFFU);
            m_data[6U] = (uint8_t)((csbkValue >> 24) & 0xFFU);
            m_data[7U] = (uint8_t)((csbkValue >> 16) & 0xFFU);
            m_data[8U] = (uint8_t)((csbkValue >> 8) & 0xFFU);
            m_data[9U] = (uint8_t)((csbkValue >> 0) & 0xFFU);
        }
        break;

    case CSBKO_TV_GRANT:
    case CSBKO_BTV_GRANT:
        {
            ulong64_t csbkValue = 0U;
            csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                    // Logical Physical Channel 1
            csbkValue = (csbkValue << 1) + (m_slotNo & 0x3U);                           // Logical Slot Number
            csbkValue = (csbkValue << 1) + 0U;                                          // Late Entry
            csbkValue = (csbkValue << 1) + 0U;                                          // Emergency
            csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);            // Site Timing: Aligned or Offset
            csbkValue = (csbkValue << 24) + m_dstId;                                    // Talkgroup ID
            csbkValue = (csbkValue << 24) + m_srcId;                                    // Source ID

            // split value into bytes
            m_data[2U] = (uint8_t)((csbkValue >> 56) & 0xFFU);
            m_data[3U] = (uint8_t)((csbkValue >> 48) & 0xFFU);
            m_data[4U] = (uint8_t)((csbkValue >> 40) & 0xFFU);
            m_data[5U] = (uint8_t)((csbkValue >> 32) & 0xFFU);
            m_data[6U] = (uint8_t)((csbkValue >> 24) & 0xFFU);
            m_data[7U] = (uint8_t)((csbkValue >> 16) & 0xFFU);
            m_data[8U] = (uint8_t)((csbkValue >> 8) & 0xFFU);
            m_data[9U] = (uint8_t)((csbkValue >> 0) & 0xFFU);
        }
        break;

    case CSBKO_BROADCAST:
        {
            ulong64_t csbkValue = 0U;
            if (!m_Cdef) {
                csbkValue = m_anncType;                                                 // Announcement Type
            }

            switch (m_anncType)
            {
            case BCAST_ANNC_ANN_WD_TSCC:
                {
                    // Broadcast Parms 1
                    csbkValue = (csbkValue << 4) + 0U;                                  // Reserved
                    csbkValue = (csbkValue << 4) + (m_colorCode & 0x0FU);               // Color Code 1
                    csbkValue = (csbkValue << 4) + (m_colorCode & 0x0FU);               // Color Code 2
                    csbkValue = (csbkValue << 1) + ((m_annWdCh1) ? 1U : 0U);            // Announce/Withdraw Channel 1
                    csbkValue = (csbkValue << 1) + ((m_annWdCh2) ? 1U : 0U);            // Announce/Withdraw Channel 2

                    csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U); // Require Registration
                    csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);               // Backoff Number
                    csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();        // Site Identity

                    // Broadcast Parms 2
                    csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);            // Logical Channel 1
                    csbkValue = (csbkValue << 12) + (m_logicalCh2 & 0xFFFU);            // Logical Channel 2
                }
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
                {
                    // Broadcast Parms 1
                    csbkValue = (csbkValue << 14) + m_siteData.systemIdentity(true);        // Site Identity (Broadcast Parms 1)

                    csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U);   // Require Registration
                    csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);                   // Backoff Number
                    csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();            // Site Identity

                    // Broadcast Parms 2
                    csbkValue = (csbkValue << 1) + 0U;                                      // Roaming TG Subscription/Attach
                    csbkValue = (csbkValue << 1) + ((m_hibernating) ? 1U : 0U);             // TSCC Hibernating
                    csbkValue = (csbkValue << 22) + 0U;                                     // Broadcast Parms 2 (Reserved)
                }
                break;
            }

            // split value into bytes
            m_data[2U] = (uint8_t)((csbkValue >> 56) & 0xFFU);
            m_data[3U] = (uint8_t)((csbkValue >> 48) & 0xFFU);
            m_data[4U] = (uint8_t)((csbkValue >> 40) & 0xFFU);
            m_data[5U] = (uint8_t)((csbkValue >> 32) & 0xFFU);
            m_data[6U] = (uint8_t)((csbkValue >> 24) & 0xFFU);
            m_data[7U] = (uint8_t)((csbkValue >> 16) & 0xFFU);
            m_data[8U] = (uint8_t)((csbkValue >> 8) & 0xFFU);
            m_data[9U] = (uint8_t)((csbkValue >> 0) & 0xFFU);
        }
        break;

    default:
        if (m_GI) {
            m_data[2U] |= 0x40U;                                                    // Group or Individual
        }

        if (m_dataContent) {
            m_data[2U] |= 0x80U;                                                    //
        }

        m_data[3U] = m_CBF;                                                         //

        m_data[4U] = (m_dstId >> 16) & 0xFFU;                                       // Destination ID
        m_data[5U] = (m_dstId >> 8) & 0xFFU;
        m_data[6U] = (m_dstId >> 0) & 0xFFU;

        m_data[7U] = (m_srcId >> 16) & 0xFFU;                                       // Source ID
        m_data[8U] = (m_srcId >> 8) & 0xFFU;
        m_data[9U] = (m_srcId >> 0) & 0xFFU;
        LogError(LOG_DMR, "CSBK::encode(), unknown CSBK type, csbko = $%02X", m_CSBKO);
        break;
    }

    m_data[10U] ^= CSBK_CRC_MASK[0U];
    m_data[11U] ^= CSBK_CRC_MASK[1U];

    edac::CRC::addCCITT162(m_data, 12U);

    m_data[10U] ^= CSBK_CRC_MASK[0U];
    m_data[11U] ^= CSBK_CRC_MASK[1U];

    if (m_verbose) {
        Utils::dump(2U, "Encoded CSBK", m_data, DMR_LC_HEADER_LENGTH_BYTES);
    }

    // encode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.encode(m_data, bytes);
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
    m_CSBKO(CSBKO_NONE),
    m_FID(0x00U),
    m_lastBlock(true),
    m_bsId(0U),
    m_GI(false),
    m_Cdef(false),
    m_srcId(0U),
    m_dstId(0U),
    m_dataContent(false),
    m_CBF(0U),
    m_colorCode(0U),
    m_backoffNo(1U),
    m_serviceType(0U),
    m_serviceOptions(0U),
    m_targetAddress(TGT_ADRS_TGID),
    m_response(0U),
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
    m_siteIdenEntry(lookups::IdenTable()),
    m_data(NULL)
{
    m_data = new uint8_t[12U];
}
