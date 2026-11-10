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
#include "p25/lc/tsbk/OSP_SCCB_EXP.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_SCCB_EXP class. */

OSP_SCCB_EXP::OSP_SCCB_EXP() : TSBK(),
    m_sccbRfssId(s_siteData.rfssId()),
    m_sccbSiteId(s_siteData.siteId()),
    m_sccbChannelId1(0U),
    m_sccbChannelNo(0U),
    m_sccbChannelId2(0U),
    m_sccbChannelNo2(0U),
    m_sccbServiceClass(ServiceClass::INVALID)
{
    m_lco = TSBKO::OSP_SCCB_EXP;
}

/* Decode a trunking signalling block. */

bool OSP_SCCB_EXP::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    if (!TSBK::decode(data, tsbk, rawTSBK))
        return false;

    const ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_sccbRfssId = (uint8_t)((tsbkValue >> 56) & 0xFFU);
    m_sccbSiteId = (uint8_t)((tsbkValue >> 48) & 0xFFU);
    m_sccbChannelId1 = (uint8_t)((tsbkValue >> 44) & 0x0FU);
    m_sccbChannelNo = (uint32_t)((tsbkValue >> 32) & 0x0FFFU);
    m_sccbChannelId2 = (uint8_t)((tsbkValue >> 20) & 0x0FU);
    m_sccbChannelNo2 = (uint32_t)((tsbkValue >> 8) & 0x0FFFU);
    m_sccbServiceClass = (uint8_t)(tsbkValue & 0xFFU);

    return true;
}

/* Encode a trunking signalling block. */

void OSP_SCCB_EXP::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = m_sccbRfssId;                                                       // RF Sub-System ID
    tsbkValue = (tsbkValue << 8) + m_sccbSiteId;                                    // Site ID

    tsbkValue = (tsbkValue << 4) + m_sccbChannelId1;                                // Channel (T) ID
    tsbkValue = (tsbkValue << 12) + m_sccbChannelNo;                                // Channel (T) Number
    tsbkValue = (tsbkValue << 12) + m_sccbChannelId2;                               // Channel (R) ID
    tsbkValue = (tsbkValue << 12) + m_sccbChannelNo2;                               // Channel (R) Number

    tsbkValue = (tsbkValue << 8) + m_sccbServiceClass;                              // System Service Class

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string OSP_SCCB_EXP::toString(bool isp)
{
    return std::string("TSBKO, OSP_SCCB_EXP (Secondary Control Channel Broadcast - Explicit)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void OSP_SCCB_EXP::copy(const OSP_SCCB_EXP& data)
{
    TSBK::copy(data);

    m_sccbRfssId = data.m_sccbRfssId;
    m_sccbSiteId = data.m_sccbSiteId;
    m_sccbChannelId1 = data.m_sccbChannelId1;
    m_sccbChannelNo = data.m_sccbChannelNo;
    m_sccbChannelId2 = data.m_sccbChannelId2;
    m_sccbChannelNo2 = data.m_sccbChannelNo2;
    m_sccbServiceClass = data.m_sccbServiceClass;
}
