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
#include "p25/lc/tsbk/mbt/MBT_OSP_RFSS_STS_BCAST.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBT_OSP_RFSS_STS_BCAST class. */
MBT_OSP_RFSS_STS_BCAST::MBT_OSP_RFSS_STS_BCAST() : AMBT()
{
    m_lco = TSBKO::OSP_RFSS_STS_BCAST;
}

/* Decode a alternate trunking signalling block. */
bool MBT_OSP_RFSS_STS_BCAST::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    /* stub */

    return true;
}

/* Encode a alternate trunking signalling block. */
void MBT_OSP_RFSS_STS_BCAST::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    // pack LRA and system ID into LLID
    uint32_t llId = m_siteData.lra();                                               // Location Registration Area
    llId = (llId << 12) + m_siteData.siteId();                                      // System ID
    if (m_siteData.netActive()) {
        llId |= 0x1000U;                                                            // Network Active Flag
    }
    dataHeader.setLLId(llId);

    /** Block 1 */
    pduUserData[0U] = (m_siteData.rfssId()) & 0xFFU;                                // RF Sub-System ID
    pduUserData[1U] = (m_siteData.siteId()) & 0xFFU;                                // Site ID
    pduUserData[2U] = ((m_siteData.channelId() & 0x0FU) << 4) +                     // Transmit Channel ID & Channel Number MSB
        ((m_siteData.channelNo() >> 8) & 0xFFU);
    pduUserData[3U] = (m_siteData.channelNo() >> 0) & 0xFFU;                        // Transmit Channel Number LSB
    pduUserData[4U] = ((m_siteData.channelId() & 0x0FU) << 4) +                     // Receive Channel ID & Channel Number MSB
        ((m_siteData.channelNo() >> 8) & 0xFFU);
    pduUserData[5U] = (m_siteData.channelNo() >> 0) & 0xFFU;                        // Receive Channel Number LSB
    pduUserData[6U] = m_siteData.serviceClass();                                    // System Service Class

    AMBT::encode(dataHeader, pduUserData);
}

/* Returns a string that represents the current TSBK. */
std::string MBT_OSP_RFSS_STS_BCAST::toString(bool isp)
{
    return std::string("TSBKO, OSP_RFSS_STS_BCAST (RFSS Status Broadcast)");
}
