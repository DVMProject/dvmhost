// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_SRV_INFO.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MESSAGE_TYPE_SRV_INFO class. */
MESSAGE_TYPE_SRV_INFO::MESSAGE_TYPE_SRV_INFO() : RCCH()
{
    m_messageType = MessageType::SRV_INFO;
}

/* Decode RCCH data. */
void MESSAGE_TYPE_SRV_INFO::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);
}

/* Encode RCCH data. */
void MESSAGE_TYPE_SRV_INFO::encode(uint8_t* data, uint32_t length, uint32_t offset)
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
    rcch[4U] = m_siteData.siteInfo1();                                              // Site Information 1
    rcch[5U] = (m_siteData.netActive() ? SiteInformation2::IP_NETWORK : 0x00U) +    // Site Information 2
        siteInfo2;

    // bryanb: this is currently fixed -- maybe dynamic in the future
    rcch[8U] = 0U;                                                                  // Restriction Information - No access restriction / No cycle restriction
    rcch[9U] = 0U;                                                                  // ...                     - No group restriction / No Location Registration Restriction
    rcch[10U] = (!m_siteData.netActive() ? 0x01U : 0x00U);                          // ...                     - No group ratio restriction / No delay time extension / ISO

    RCCH::encode(data, rcch, length, offset);
}

/* Returns a string that represents the current RCCH. */
std::string MESSAGE_TYPE_SRV_INFO::toString(bool isp)
{
    return std::string("SRV_INFO (Service Information)");
}
