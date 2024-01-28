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
#include "nxdn/lc/rcch/MESSAGE_TYPE_SRV_INFO.h"

using namespace nxdn::lc::rcch;
using namespace nxdn::lc;
using namespace nxdn;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MESSAGE_TYPE_SRV_INFO class.
/// </summary>
MESSAGE_TYPE_SRV_INFO::MESSAGE_TYPE_SRV_INFO() : RCCH()
{
    m_messageType = nxdn::MESSAGE_TYPE_SRV_INFO;
}

/// <summary>
/// Decode layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_SRV_INFO::decode(const uint8_t* data, uint32_t length, uint32_t offset)
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
void MESSAGE_TYPE_SRV_INFO::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[1U] = (m_siteData.locId() >> 16) & 0xFFU;                                  // Location ID
    rcch[2U] = (m_siteData.locId() >> 8) & 0xFFU;                                   // ...
    rcch[3U] = (m_siteData.locId() >> 0) & 0xFFU;                                   // ...
    rcch[4U] = m_siteData.serviceClass();                                           // Service Information
    rcch[5U] = (m_siteData.netActive() ? NXDN_SIF2_IP_NETWORK : 0x00U);             // ...

    // bryanb: this is currently fixed -- maybe dynamic in the future
    rcch[8U] = 0U;                                                                  // Restriction Information - No access restriction / No cycle restriction
    rcch[9U] = 0U;                                                                  // ...                     - No group restriction / No Location Registration Restriction
    rcch[10U] = (!m_siteData.netActive() ? 0x01U : 0x00U);                          // ...                     - No group ratio restriction / No delay time extension / ISO

    RCCH::encode(data, rcch, length, offset);
}

/// <summary>
/// Returns a string that represents the current RCCH.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MESSAGE_TYPE_SRV_INFO::toString(bool isp)
{
    return std::string("MESSAGE_TYPE_SRV_INFO (Service Information)");
}
