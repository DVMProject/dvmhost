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
*   Copyright (C) 2017-2019 by Bryan Biedenkapp N2PLL
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
#include "p25/P25Defines.h"
#include "p25/lc/TDULC.h"
#include "p25/P25Utils.h"
#include "edac/CRC.h"
#include "edac/Golay24128.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc;
using namespace p25;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the TDULC class.
/// </summary>
TDULC::TDULC() :
    m_protect(false),
    m_lco(LC_GROUP),
    m_mfId(P25_MFG_STANDARD),
    m_srcId(0U),
    m_dstId(0U),
    m_emergency(false),
    m_encrypted(false),
    m_priority(4U),
    m_group(true),
    m_rs(),
    m_callTimer(0U),
    m_siteData(),
    m_siteNetActive(false)
{
    m_siteIdenEntry = lookups::IdenTable();

    reset();
}

/// <summary>
/// Finalizes a instance of TDULC class.
/// </summary>
TDULC::~TDULC()
{
    /* stub */
}

/// <summary>
/// Decode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
bool TDULC::decode(const uint8_t* data)
{
    assert(data != NULL);

    // deinterleave
    uint8_t rs[P25_TDULC_LENGTH_BYTES + 1U];
    uint8_t raw[P25_TDULC_FEC_LENGTH_BYTES + 1U];
    P25Utils::decode(data, raw, 114U, 410U);

    // decode Golay (24,12,8) FEC
    edac::Golay24128::decode24128(rs, raw, P25_TDULC_LENGTH_BYTES);

    // Utils::dump(2U, "TDULC RS", rs, P25_TDULC_LENGTH_BYTES);

    // decode RS (24,12,13) FEC
    try {
        bool ret = m_rs.decode241213(rs);
        if (!ret)
            return false;
    }
    catch (...) {
        Utils::dump(2U, "P25, RS crashed with input data", rs, P25_TDULC_LENGTH_BYTES);
        return false;
    }

    // Utils::dump(2U, "TDULC", rs, P25_TDULC_LENGTH_BYTES);

    return decodeLC(rs);
}

/// <summary>
/// Encode a terminator data unit w/ link control.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
void TDULC::encode(uint8_t * data)
{
    assert(data != NULL);

    uint8_t rs[P25_TDULC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_TDULC_LENGTH_BYTES);

    encodeLC(rs);

    // Utils::dump(2U, "TDULC", rs, P25_TDULC_LENGTH_BYTES);

    // encode RS (24,12,13) FEC
    m_rs.encode241213(rs);

    // Utils::dump(2U, "TDULC RS", rs, P25_TDULC_LENGTH_BYTES);

    uint8_t raw[P25_TDULC_FEC_LENGTH_BYTES + 1U];
    ::memset(raw, 0x00U, P25_TDULC_FEC_LENGTH_BYTES + 1U);

    // encode Golay (24,12,8) FEC
    edac::Golay24128::encode24128(raw, rs, P25_TDULC_LENGTH_BYTES);

    // interleave
    P25Utils::encode(raw, data, 114U, 410U);

    // Utils::dump(2U, "TDULC Interleave", data, P25_TDULC_FRAME_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
}

/// <summary>
/// Helper to reset data values to defaults.
/// </summary>
void TDULC::reset()
{
    m_protect = false;
    m_lco = LC_GROUP;
    m_mfId = P25_MFG_STANDARD;

    m_srcId = 0U;
    m_dstId = 0U;

    m_callTimer = 0U;

    m_grpVchNo = m_siteData.channelNo();

    m_adjCFVA = P25_CFVA_CONV | P25_CFVA_FAILURE;
    m_adjRfssId = 0U;
    m_adjSiteId = 0U;
    m_adjChannelId = 0U;
    m_adjChannelNo = 0U;

    /* Service Options */
    m_emergency = false;
    m_encrypted = false;
    m_priority = 4U;
    m_group = true;
}

/** Local Site data */
/// <summary>Sets local configured site data.</summary>
/// <param name="siteData"></param>
void TDULC::setSiteData(SiteData siteData)
{
    m_siteData = siteData;
}

/// <summary></summary>
/// <param name="entry"></param>
void TDULC::setIdenTable(lookups::IdenTable entry)
{
    m_siteIdenEntry = entry;
}

/// <summary>Sets a flag indicating whether or not networking is active.</summary>
/// <param name="netActive"></param>
void TDULC::setNetActive(bool netActive)
{
    m_siteNetActive = netActive;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Decode link control.
/// </summary>
/// <param name="rs"></param>
/// <returns></returns>
bool TDULC::decodeLC(const uint8_t* rs)
{
    ulong64_t rsValue = 0U;

    // combine bytes into rs value
    rsValue = rs[1U];
    rsValue = (rsValue << 8) + rs[2U];
    rsValue = (rsValue << 8) + rs[3U];
    rsValue = (rsValue << 8) + rs[4U];
    rsValue = (rsValue << 8) + rs[5U];
    rsValue = (rsValue << 8) + rs[6U];
    rsValue = (rsValue << 8) + rs[7U];
    rsValue = (rsValue << 8) + rs[8U];

    m_protect = (rs[0U] & 0x80U) == 0x80U;                                          // Protect Flag
    m_lco = rs[0U] & 0x3FU;                                                         // LCO

    // standard P25 reference opcodes
    switch (m_lco) {
    case LC_GROUP:
        m_mfId = rs[1U];                                                            // Mfg Id.
        m_group = true;
        m_emergency = (rs[2U] & 0x80U) == 0x80U;                                    // Emergency Flag
        m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                    // Encryption Flag
        m_priority = (rs[2U] & 0x07U);                                              // Priority
        m_dstId = (uint32_t)((rsValue >> 24) & 0xFFFFU);                            // Talkgroup Address
        m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source Radio Address
        break;
    case LC_PRIVATE:
        m_mfId = rs[1U];                                                            // Mfg Id.
        m_group = false;
        m_emergency = (rs[2U] & 0x80U) == 0x80U;                                    // Emergency Flag
        m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                    // Encryption Flag
        m_priority = (rs[2U] & 0x07U);                                              // Priority
        m_dstId = (uint32_t)((rsValue >> 24) & 0xFFFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source Radio Address
        break;
    case LC_TEL_INT_VCH_USER:
        m_emergency = (rs[2U] & 0x80U) == 0x80U;                                    // Emergency Flag
        m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                    // Encryption Flag
        m_priority = (rs[2U] & 0x07U);                                              // Priority
        m_callTimer = (uint32_t)((rsValue >> 24) & 0xFFFFU);                        // Call Timer
        if (m_srcId == 0U) {
            m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                              // Source/Target Address
        }
        break;
    default:
        LogError(LOG_P25, "unknown LC value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
        return false;
    }

    // sanity check priority (per TIA-102.AABC-B) it should never be 0, if its 0, default to 4
    if (m_priority == 0) {
        m_priority = 4U;
    }

    return true;
}

/// <summary>
/// Encode link control.
/// </summary>
/// <param name="rs"></param>
void TDULC::encodeLC(uint8_t* rs)
{
    const uint32_t services = (m_siteNetActive) ? P25_SYS_SRV_NET_ACTIVE : 0U |
        P25_SYS_SRV_GROUP_DATA | P25_SYS_SRV_GROUP_VOICE | P25_SYS_SRV_IND_DATA | P25_SYS_SRV_IND_VOICE;

    ulong64_t rsValue = 0U;
    rs[0U] = m_lco;                                                                 // LCO

    // standard P25 reference opcodes
    switch (m_lco) {
    case LC_GROUP:
        rsValue = m_mfId;
        rsValue = (rsValue << 8) +
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        rsValue = (rsValue << 24) + m_dstId;                                        // Talkgroup Address
        rsValue = (rsValue << 24) + m_srcId;                                        // Source Radio Address
        break;
    case LC_GROUP_UPDT:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = m_siteData.channelId();                                           // Group A - Channel ID
        rsValue = (rsValue << 12) + m_grpVchNo;                                     // Group A - Channel Number
        rsValue = (rsValue << 16) + m_dstId;                                        // Group A - Talkgroup Address
        rsValue = (rsValue << 4) + m_siteData.channelId();                          // Group B - Channel ID
        rsValue = (rsValue << 12) + m_grpVchNo;                                     // Group B - Channel Number
        rsValue = (rsValue << 16) + m_dstId;                                        // Group B - Talkgroup Address
        break;
    case LC_PRIVATE:
        rsValue = m_mfId;
        rsValue = (rsValue << 8) +
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        rsValue = (rsValue << 24) + m_dstId;                                        // Target Radio Address
        rsValue = (rsValue << 24) + m_srcId;                                        // Source Radio Address
        break;
    case LC_TEL_INT_VCH_USER:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = (rsValue << 8) +
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        rsValue = (rsValue << 16) + m_callTimer;                                    // Call Timer
        rsValue = (rsValue << 24) + m_srcId;                                        // Source/Target Radio Address
        break;
    case LC_CALL_TERM:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = 0U;
        rsValue = (rsValue << 24) + P25_WUID_SYS;                                   // System Radio Address
        break;
    case LC_IDEN_UP:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        {
            if ((m_siteIdenEntry.chBandwidthKhz() != 0.0F) && (m_siteIdenEntry.chSpaceKhz() != 0.0F) &&
                (m_siteIdenEntry.txOffsetMhz() != 0U) && (m_siteIdenEntry.baseFrequency() != 0U)) {
                uint32_t calcSpace = (uint32_t)(m_siteIdenEntry.chSpaceKhz() / 0.125);
                uint32_t calcTxOffset = (uint32_t)((abs(m_siteIdenEntry.txOffsetMhz()) / m_siteIdenEntry.chSpaceKhz()) * 1000);
                if (m_siteIdenEntry.txOffsetMhz() > 0.0F)
                    calcTxOffset |= 0x2000U; // this sets a positive offset ...

                uint32_t calcBaseFreq = (uint32_t)(m_siteIdenEntry.baseFrequency() / 5);
                uint8_t chanBw = (m_siteIdenEntry.chBandwidthKhz() >= 12.5F) ? P25_IDEN_UP_VU_BW_125K : P25_IDEN_UP_VU_BW_625K;

                rsValue = m_siteIdenEntry.channelId();                              // Channel ID
                rsValue = (rsValue << 4) + chanBw;                                  // Channel Bandwidth
                rsValue = (rsValue << 14) + calcTxOffset;                           // Transmit Offset
                rsValue = (rsValue << 10) + calcSpace;                              // Channel Spacing
                rsValue = (rsValue << 32) + calcBaseFreq;                           // Base Frequency
            }
            else {
                LogError(LOG_P25, "invalid values for LC_IDEN_UP, baseFrequency = %uHz, txOffsetMhz = %uMHz, chBandwidthKhz = %fKHz, chSpaceKhz = %fKHz",
                    m_siteIdenEntry.baseFrequency(), m_siteIdenEntry.txOffsetMhz(), m_siteIdenEntry.chBandwidthKhz(),
                    m_siteIdenEntry.chSpaceKhz());
                return; // blatently ignore creating this TSBK
            }
        }
        break;
    case LC_SYS_SRV_BCAST:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = 0U;                                                               //
        rsValue = (rsValue << 16) + services;                                       // System Services Available
        rsValue = (rsValue << 24) + services;                                       // System Services Supported
        break;
    case LC_ADJ_STS_BCAST:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        {
            if ((m_adjRfssId != 0U) && (m_adjSiteId != 0U) && (m_adjChannelNo != 0U)) {
                if (m_adjSysId == 0U) {
                    m_adjSysId = m_siteData.sysId();
                }

                rsValue = m_siteData.lra();                                         // Location Registration Area
                rsValue = (rsValue << 12) + m_adjSysId;                             // System ID
                rsValue = (rsValue << 8) + m_adjRfssId;                             // RF Sub-System ID
                rsValue = (rsValue << 8) + m_adjSiteId;                             // Site ID
                rsValue = (rsValue << 4) + m_adjChannelId;                          // Channel ID
                rsValue = (rsValue << 12) + m_adjChannelNo;                         // Channel Number
                rsValue = (rsValue << 8) +                                          // System Service Class
                    (P25_SVC_CLS_COMPOSITE | P25_SVC_CLS_VOICE | P25_SVC_CLS_DATA | P25_SVC_CLS_REG);
            }
            else {
                LogError(LOG_P25, "invalid values for LC_ADJ_STS_BCAST, tsbkAdjSiteRFSSId = $%02X, tsbkAdjSiteId = $%02X, tsbkAdjSiteChannel = $%02X",
                    m_adjRfssId, m_adjSiteId, m_adjChannelNo);
                return; // blatently ignore creating this TSBK
            }
        }
        break;
    case LC_RFSS_STS_BCAST:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = m_siteData.lra();                                                 // Location Registration Area
        rsValue = (rsValue << 12) + m_siteData.sysId();                             // System ID
        rsValue = (rsValue << 8) + m_siteData.rfssId();                             // RF Sub-System ID
        rsValue = (rsValue << 8) + m_siteData.siteId();                             // Site ID
        rsValue = (rsValue << 4) + m_siteData.channelId();                          // Channel ID
        rsValue = (rsValue << 12) + m_siteData.channelNo();                         // Channel Number
        rsValue = (rsValue << 8) +                                                  // System Service Class
            (P25_SVC_CLS_COMPOSITE | P25_SVC_CLS_VOICE | P25_SVC_CLS_DATA | P25_SVC_CLS_REG);
        break;
    case LC_NET_STS_BCAST:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = 0U;                                                               // 
        rsValue = (rsValue << 20) + m_siteData.netId();                             // Network ID
        rsValue = (rsValue << 12) + m_siteData.sysId();                             // System ID
        rsValue = (rsValue << 4) + m_siteData.channelId();                          // Channel ID
        rsValue = (rsValue << 12) + m_siteData.channelNo();                         // Channel Number
        rsValue = (rsValue << 8) +                                                  // System Service Class
            (P25_SVC_CLS_COMPOSITE | P25_SVC_CLS_VOICE | P25_SVC_CLS_DATA | P25_SVC_CLS_REG);
        break;
    default:
        LogError(LOG_P25, "unknown LC value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
        break;
    }

    // split rs value into bytes
    rs[1U] = (uint8_t)((rsValue >> 56) & 0xFFU);
    rs[2U] = (uint8_t)((rsValue >> 48) & 0xFFU);
    rs[3U] = (uint8_t)((rsValue >> 40) & 0xFFU);
    rs[4U] = (uint8_t)((rsValue >> 32) & 0xFFU);
    rs[5U] = (uint8_t)((rsValue >> 24) & 0xFFU);
    rs[6U] = (uint8_t)((rsValue >> 16) & 0xFFU);
    rs[7U] = (uint8_t)((rsValue >> 8) & 0xFFU);
    rs[8U] = (uint8_t)((rsValue >> 0) & 0xFFU);
}
