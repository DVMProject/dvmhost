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
#include "nxdn/lc/rcch/MESSAGE_TYPE_SITE_INFO.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn::lc::rcch;
using namespace nxdn::lc;
using namespace nxdn;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MESSAGE_TYPE_SITE_INFO class.
/// </summary>
MESSAGE_TYPE_SITE_INFO::MESSAGE_TYPE_SITE_INFO() : RCCH(),
    m_bcchCnt(1U),
    m_rcchGroupingCnt(1U),
    m_ccchPagingCnt(2U),
    m_ccchMultiCnt(2U),
    m_rcchIterateCnt(2U)
{
    m_messageType = RCCH_MESSAGE_TYPE_SITE_INFO;
}

/// <summary>
/// Decode layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_SITE_INFO::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    uint8_t rcch[22U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_SITE_INFO::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    uint8_t rcch[22U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[1U] = (m_siteData.locId() >> 16) & 0xFFU;                                  // Location ID
    rcch[2U] = (m_siteData.locId() >> 8) & 0xFFU;                                   // ...
    rcch[3U] = (m_siteData.locId() >> 0) & 0xFFU;                                   // ...
    rcch[4U] = ((m_bcchCnt & 0x03U) << 6) +                                         // Channel Structure - Number of BCCH
        ((m_rcchGroupingCnt & 0x07U) << 3) +                                        // ...               - Number of Grouping
        (((m_ccchPagingCnt >> 1) & 0x07U) << 0);                                    // ...               - Number of Paging Frames
    rcch[5U] = ((m_ccchPagingCnt & 0x01U) << 7) +                                   // ...               - Number of Paging Frames
        ((m_ccchMultiCnt & 0x07U) << 4) +                                           // ...               - Number of Multipurpose Frames
        ((m_rcchIterateCnt & 0x0FU) << 0);                                          // ...               - Number of Iteration

    rcch[6U] = m_siteData.serviceClass();                                           // Service Information
    rcch[7U] = (m_siteData.netActive() ? NXDN_SIF2_IP_NETWORK : 0x00U);             // ...

    // bryanb: this is currently fixed -- maybe dynamic in the future
    rcch[8U] = 0U;                                                                  // Restriction Information - No access restriction / No cycle restriction
    rcch[9U] = 0x08U;                                                               // ...                     - No group restriction / GMS; Location Registration Restriction
    rcch[10U] = (!m_siteData.netActive() ? 0x01U : 0x00U);                          // ...                     - No group ratio restriction / No delay time extension / ISO

    // bryanb: this is currently fixed -- maybe dynamic in the future
    rcch[11U] = NXDN_CH_ACCESS_BASE_FREQ_SYS_DEFINED;                               // Channel Access Information - Channel Version / Sys Defined Step / Sys Defined Base Freq

    rcch[14U] = 1U;                                                                 // Version

    uint16_t channelNo = m_siteData.channelNo() & 0x3FFU;
    rcch[15U] = (channelNo >> 6) & 0x0FU;                                           // 1st Control Channel
    rcch[16U] = (channelNo & 0x3FU) << 2;                                           // ...

    RCCH::encode(data, rcch, length, offset);
}

/// <summary>
/// Returns a string that represents the current RCCH.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MESSAGE_TYPE_SITE_INFO::toString(bool isp)
{
    return std::string("RCCH_MESSAGE_TYPE_SITE_INFO (Site Information)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void MESSAGE_TYPE_SITE_INFO::copy(const MESSAGE_TYPE_SITE_INFO& data)
{
    RCCH::copy(data);

    m_bcchCnt = data.m_bcchCnt;
    m_rcchGroupingCnt = data.m_rcchGroupingCnt;
    m_ccchPagingCnt = data.m_ccchPagingCnt;
    m_ccchMultiCnt = data.m_ccchMultiCnt;
    m_rcchIterateCnt = data.m_rcchIterateCnt;
}
