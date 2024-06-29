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
#include "p25/lc/tsbk/OSP_ADJ_STS_BCAST.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_ADJ_STS_BCAST class. */
OSP_ADJ_STS_BCAST::OSP_ADJ_STS_BCAST() : TSBK(),
    m_adjCFVA(CFVA::FAILURE),
    m_adjSysId(0U),
    m_adjRfssId(0U),
    m_adjSiteId(0U),
    m_adjChannelId(0U),
    m_adjChannelNo(0U),
    m_adjServiceClass(ServiceClass::INVALID)
{
    m_lco = TSBKO::OSP_ADJ_STS_BCAST;
}

/* Decode a trunking signalling block. */
bool OSP_ADJ_STS_BCAST::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_adjSysId = (uint32_t)((tsbkValue >> 40) & 0xFFFU);                            // Site System ID
    m_adjRfssId = (uint8_t)((tsbkValue >> 32) & 0xFFU);                             // Site RFSS ID
    m_adjSiteId = (uint8_t)((tsbkValue >> 24) & 0xFFU);                             // Site ID
    m_adjChannelId = (uint8_t)((tsbkValue >> 20) & 0xFU);                           // Site Channel ID
    m_adjChannelNo = (uint32_t)((tsbkValue >> 8) & 0xFFFU);                         // Site Channel Number
    m_adjServiceClass = (uint8_t)(tsbkValue & 0xFFU);                               // Site Service Class

    return true;
}

/* Encode a trunking signalling block. */
void OSP_ADJ_STS_BCAST::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = m_siteData.lra();                                                   // Location Registration Area
    tsbkValue = (tsbkValue << 4) + m_adjCFVA;                                       // CFVA
    tsbkValue = (tsbkValue << 12) + m_siteData.sysId();                             // System ID
    tsbkValue = (tsbkValue << 8) + m_adjRfssId;                                     // RF Sub-System ID
    tsbkValue = (tsbkValue << 8) + m_adjSiteId;                                     // Site ID
    tsbkValue = (tsbkValue << 4) + m_adjChannelId;                                  // Channel ID
    tsbkValue = (tsbkValue << 12) + m_adjChannelNo;                                 // Channel Number
    tsbkValue = (tsbkValue << 8) + m_adjServiceClass;                               // System Service Class

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */
std::string OSP_ADJ_STS_BCAST::toString(bool isp)
{
    return std::string("TSBKO, OSP_ADJ_STS_BCAST (Adjacent Site Status Broadcast)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */
void OSP_ADJ_STS_BCAST::copy(const OSP_ADJ_STS_BCAST& data)
{
    TSBK::copy(data);

    m_adjCFVA = data.m_adjCFVA;
    m_adjSysId = data.m_adjSysId;
    m_adjRfssId = data.m_adjRfssId;
    m_adjSiteId = data.m_adjSiteId;
    m_adjChannelId = data.m_adjChannelId;
    m_adjChannelNo = data.m_adjChannelNo;
    m_adjServiceClass = data.m_adjServiceClass;
}
