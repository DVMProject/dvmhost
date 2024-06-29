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
#include "p25/lc/tsbk/OSP_RFSS_STS_BCAST.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_RFSS_STS_BCAST class. */
OSP_RFSS_STS_BCAST::OSP_RFSS_STS_BCAST() : TSBK(),
    m_roamerReaccess(false)
{
    m_lco = TSBKO::OSP_RFSS_STS_BCAST;
}

/* Decode a trunking signalling block. */
bool OSP_RFSS_STS_BCAST::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a trunking signalling block. */
void OSP_RFSS_STS_BCAST::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = m_siteData.lra();                                                   // Location Registration Area
    tsbkValue = (tsbkValue << 4) +
        ((m_roamerReaccess) ? 0x02U : 0x00U) +                                      // Roamer Reaccess Method
        ((m_siteData.netActive()) ? 0x01U : 0x00U);                                 // Network Active
    tsbkValue = (tsbkValue << 12) + m_siteData.sysId();                             // System ID
    tsbkValue = (tsbkValue << 8) + m_siteData.rfssId();                             // RF Sub-System ID
    tsbkValue = (tsbkValue << 8) + m_siteData.siteId();                             // Site ID
    tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                          // Channel ID
    tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                         // Channel Number
    tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();                       // System Service Class

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */
std::string OSP_RFSS_STS_BCAST::toString(bool isp)
{
    return std::string("TSBKO, OSP_RFSS_STS_BCAST (RFSS Status Broadcast)");
}
