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
#include "p25/lc/tsbk/IOSP_GRP_AFF.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the IOSP_GRP_AFF class. */
IOSP_GRP_AFF::IOSP_GRP_AFF() : TSBK(),
    m_announceGroup(WUID_ALL)
{
    m_lco = TSBKO::IOSP_GRP_AFF;
}

/* Decode a trunking signalling block. */
bool IOSP_GRP_AFF::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_sysId = (uint32_t)((tsbkValue >> 40) & 0xFFFU);                               // System ID
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                              // Talkgroup Address
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */
void IOSP_GRP_AFF::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = 1U;                                                                 // Local/Global Affiliation Flag (0 = Local, 1 = Global)
    tsbkValue = (tsbkValue << 7) + (m_response & 0x3U);                             // Affiliation Response
    tsbkValue = (tsbkValue << 16) + (m_announceGroup & 0xFFFFU);                    // Announcement Group Address
    tsbkValue = (tsbkValue << 16) + (m_dstId & 0xFFFFU);                            // Talkgroup Address
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */
std::string IOSP_GRP_AFF::toString(bool isp)
{
    return (isp) ? std::string("TSBKO, IOSP_GRP_AFF (Group Affiliation Request)") :
        std::string("TSBKO, IOSP_GRP_AFF (Group Affiliation Response)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */
void IOSP_GRP_AFF::copy(const IOSP_GRP_AFF& data)
{
    TSBK::copy(data);

    m_announceGroup = data.m_announceGroup;
}
