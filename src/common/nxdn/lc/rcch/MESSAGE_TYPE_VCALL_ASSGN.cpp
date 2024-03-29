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
#include "nxdn/lc/rcch/MESSAGE_TYPE_VCALL_ASSGN.h"

using namespace nxdn::lc::rcch;
using namespace nxdn::lc;
using namespace nxdn;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MESSAGE_TYPE_VCALL_ASSGN class.
/// </summary>
MESSAGE_TYPE_VCALL_ASSGN::MESSAGE_TYPE_VCALL_ASSGN() : RCCH()
{
    m_messageType = RCCH_MESSAGE_TYPE_VCALL_ASSGN;
}

/// <summary>
/// Decode layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_VCALL_ASSGN::decode(const uint8_t* data, uint32_t length, uint32_t offset)
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
void MESSAGE_TYPE_VCALL_ASSGN::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[1U] = (m_emergency ? 0x80U : 0x00U) +                                      // Emergency Flag
        (m_priority ? 0x20U : 0x00U);                                               // Priority Flag
    rcch[2U] = ((m_callType & 0x07U) << 5) +                                        // Call Type
        (m_duplex ? 0x10U : 0x00U) +                                                // Half/Full Duplex Flag
        (m_transmissionMode & 0x07U);                                               // Transmission Mode

    rcch[3U] = (m_srcId >> 8U) & 0xFFU;                                             // Source Radio Address
    rcch[4U] = (m_srcId >> 0U) & 0xFFU;                                             // ...
    rcch[5U] = (m_dstId >> 8U) & 0xFFU;                                             // Target Radio Address
    rcch[6U] = (m_dstId >> 0U) & 0xFFU;                                             // ...

    rcch[7U] = (m_grpVchNo >> 10) & 0x03U;                                          // Channel
    rcch[8U] = (m_grpVchNo & 0xFFU);                                                // ...

    rcch[10U] = (m_siteData.locId() >> 8) & 0xFFU;                                  // Location ID
    rcch[11U] = (m_siteData.locId() >> 0) & 0xFFU;                                  // ...

    RCCH::encode(data, rcch, length, offset);
}

/// <summary>
/// Returns a string that represents the current RCCH.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MESSAGE_TYPE_VCALL_ASSGN::toString(bool isp)
{
    return std::string("RCCH_MESSAGE_TYPE_VCALL_ASSGN (Voice Call Assignment)");
}
