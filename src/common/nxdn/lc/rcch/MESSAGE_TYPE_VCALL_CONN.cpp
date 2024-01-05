/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
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
#include "nxdn/lc/rcch/MESSAGE_TYPE_VCALL_CONN.h"
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
/// Initializes a new instance of the MESSAGE_TYPE_VCALL_CONN class.
/// </summary>
MESSAGE_TYPE_VCALL_CONN::MESSAGE_TYPE_VCALL_CONN() : RCCH()
{
    m_messageType = RCCH_MESSAGE_TYPE_VCALL_CONN;
}

/// <summary>
/// Decode layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_VCALL_CONN::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);

    m_callType = (rcch[2U] >> 5) & 0x07U;                                           // Call Type
    m_emergency = (rcch[1U] & 0x80U) == 0x80U;                                      // Emergency Flag
    m_priority = (rcch[1U] & 0x20U) == 0x20U;                                       // Priority Flag
    m_duplex = (rcch[2U] & 0x10U) == 0x10U;                                         // Half/Full Duplex Flag
    m_transmissionMode = (rcch[2U] & 0x07U);                                        // Transmission Mode
    m_srcId = (uint16_t)((rcch[3U] << 8) | rcch[4U]) & 0xFFFFU;                     // Source Radio Address
    m_dstId = (uint16_t)((rcch[5U] << 8) | rcch[6U]) & 0xFFFFU;                     // Target Radio Address
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_VCALL_CONN::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

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

    rcch[7U] = m_causeRsp;                                                          // Cause (VD)
    rcch[9U] = (m_siteData.locId() >> 8) & 0xFFU;                                   // Location ID
    rcch[10U] = (m_siteData.locId() >> 0) & 0xFFU;                                  // ...

    RCCH::encode(data, rcch, length, offset);
}

/// <summary>
/// Returns a string that represents the current RCCH.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MESSAGE_TYPE_VCALL_CONN::toString(bool isp)
{
    if (isp) return std::string("RCCH_MESSAGE_TYPE_VCALL_CONN (Voice Call Connection Request)");
    else return std::string("RCCH_MESSAGE_TYPE_VCALL_CONN (Voice Call Connection Response)");
}
