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
#include "p25/lc/tsbk/MBT_OSP_NET_STS_BCAST.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MBT_OSP_NET_STS_BCAST class.
/// </summary>
MBT_OSP_NET_STS_BCAST::MBT_OSP_NET_STS_BCAST() : AMBT()
{
    m_lco = TSBK_OSP_NET_STS_BCAST;
}

/// <summary>
/// Decode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool MBT_OSP_NET_STS_BCAST::decodeMBT(const data::DataHeader dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != NULL);

    /* stub */

    return true;
}

/// <summary>
/// Encode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="pduUserData"></param>
void MBT_OSP_NET_STS_BCAST::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != NULL);

    // pack LRA and system ID into LLID
    uint32_t llId = m_siteData.lra();                                               // Location Registration Area
    llId = (llId << 12) + m_siteData.siteId();                                      // System ID
    dataHeader.setLLId(llId);

    /** Block 1 */
    pduUserData[0U] = (m_siteData.netId() >> 12) & 0xFFU;                           // Network ID (b19-12)
    pduUserData[1U] = (m_siteData.netId() >> 4) & 0xFFU;                            // Network ID (b11-b4)
    pduUserData[2U] = (m_siteData.netId() & 0x0FU) << 4;                            // Network ID (b3-b0)
    pduUserData[3U] = ((m_siteData.channelId() & 0x0FU) << 4) +                     // Transmit Channel ID & Channel Number MSB
        ((m_siteData.channelNo() >> 8) & 0xFFU);
    pduUserData[4U] = (m_siteData.channelNo() >> 0) & 0xFFU;                        // Transmit Channel Number LSB
    pduUserData[5U] = ((m_siteData.channelId() & 0x0FU) << 4) +                     // Receive Channel ID & Channel Number MSB
        ((m_siteData.channelNo() >> 8) & 0xFFU);
    pduUserData[6U] = (m_siteData.channelNo() >> 0) & 0xFFU;                        // Receive Channel Number LSB
    pduUserData[7U] = m_siteData.serviceClass();                                    // System Service Class

    AMBT::encode(dataHeader, pduUserData);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MBT_OSP_NET_STS_BCAST::toString(bool isp)
{
    return std::string("TSBK_OSP_NET_STS_BCAST (Network Status Broadcast - Explicit)");
}
