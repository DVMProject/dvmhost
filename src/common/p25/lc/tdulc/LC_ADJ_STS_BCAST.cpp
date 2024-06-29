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
#include "p25/lc/tdulc/LC_ADJ_STS_BCAST.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the LC_ADJ_STS_BCAST class. */

LC_ADJ_STS_BCAST::LC_ADJ_STS_BCAST() : TDULC(),
    m_adjCFVA(CFVA::FAILURE),
    m_adjSysId(0U),
    m_adjRfssId(0U),
    m_adjSiteId(0U),
    m_adjChannelId(0U),
    m_adjChannelNo(0U),
    m_adjServiceClass(ServiceClass::INVALID)
{
    m_lco = LCO::ADJ_STS_BCAST;
}

/* Decode a terminator data unit w/ link control. */

bool LC_ADJ_STS_BCAST::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a terminator data unit w/ link control. */

void LC_ADJ_STS_BCAST::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t rsValue = 0U;

    m_implicit = true;

    if ((m_adjRfssId != 0U) && (m_adjSiteId != 0U) && (m_adjChannelNo != 0U)) {
        if (m_adjSysId == 0U) {
            m_adjSysId = m_siteData.sysId();
        }

        rsValue = m_siteData.lra();                                                 // Location Registration Area
        rsValue = (rsValue << 12) + m_adjSysId;                                     // System ID
        rsValue = (rsValue << 8) + m_adjRfssId;                                     // RF Sub-System ID
        rsValue = (rsValue << 8) + m_adjSiteId;                                     // Site ID
        rsValue = (rsValue << 4) + m_adjChannelId;                                  // Channel ID
        rsValue = (rsValue << 12) + m_adjChannelNo;                                 // Channel Number
        rsValue = (rsValue << 8) + m_adjServiceClass;                               // System Service Class
    }
    else {
        LogError(LOG_P25, "LC_ADJ_STS_BCAST::encodeLC(), invalid values for LC_ADJ_STS_BCAST, adjSiteRFSSId = $%02X, adjSiteId = $%02X, adjSiteChannel = $%02X",
            m_adjRfssId, m_adjSiteId, m_adjChannelNo);
        return; // blatantly ignore creating this TSBK
    }

    std::unique_ptr<uint8_t[]> rs = TDULC::fromValue(rsValue);
    TDULC::encode(data, rs.get());
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void LC_ADJ_STS_BCAST::copy(const LC_ADJ_STS_BCAST& data)
{
    TDULC::copy(data);

    m_adjCFVA = data.m_adjCFVA;
    m_adjSysId = data.m_adjSysId;
    m_adjRfssId = data.m_adjRfssId;
    m_adjSiteId = data.m_adjSiteId;
    m_adjChannelId = data.m_adjChannelId;
    m_adjChannelNo = data.m_adjChannelNo;
    m_adjServiceClass = data.m_adjServiceClass;
}
