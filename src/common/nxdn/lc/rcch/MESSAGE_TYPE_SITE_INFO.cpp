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
#include "nxdn/lc/rcch/MESSAGE_TYPE_SITE_INFO.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

#include <cassert>

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
    m_messageType = MessageType::RCCH_SITE_INFO;
}

/// <summary>
/// Decode layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_SITE_INFO::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
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
    assert(data != nullptr);

    uint8_t siteInfo2 = m_siteData.siteInfo2();
    if ((siteInfo2 & SiteInformation2::IP_NETWORK) == SiteInformation2::IP_NETWORK)
        siteInfo2 &= ~SiteInformation2::IP_NETWORK; // clear the IP_NETWORK bit -- that will be provided by netActive()

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
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

    rcch[6U] = m_siteData.siteInfo1();                                              // Site Information 1
    rcch[7U] = (m_siteData.netActive() ? SiteInformation2::IP_NETWORK : 0x00U) +    // Site Information 2
        siteInfo2;

    // bryanb: this is currently fixed -- maybe dynamic in the future
    rcch[8U] = 0U;                                                                  // Restriction Information - No access restriction / No cycle restriction
    rcch[9U] = 0U;                                                                  // ...                     - No group restriction / No Location Registration Restriction
    //rcch[10U] = (!m_siteData.netActive() ? 0x01U : 0x00U);                        // ...                     - No group ratio restriction / No delay time extension / ISO
    rcch[10U] = 0U;

    // bryanb: this is currently fixed -- maybe dynamic in the future
    //rcch[11U] = ChAccessBase::FREQ_SYS_DEFINED << 2;                              // Channel Access Information - Channel Version / Sys Defined Step / Sys Defined Base Freq
    rcch[11U] = 0U;

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
    return std::string("RCCH_SITE_INFO (Site Information)");
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
