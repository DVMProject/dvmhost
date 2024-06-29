// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/OSP_GRP_VCH_GRANT_UPD.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_GRP_VCH_GRANT_UPD class. */
OSP_GRP_VCH_GRANT_UPD::OSP_GRP_VCH_GRANT_UPD() : TSBK()
{
    m_lco = TSBKO::OSP_GRP_VCH_GRANT_UPD;
}

/* Decode a trunking signalling block. */
bool OSP_GRP_VCH_GRANT_UPD::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_grpVchId = (uint8_t)((tsbkValue >> 60) & 0xFU);                               // Channel ID
    m_grpVchNo = (uint32_t)((tsbkValue >> 48) & 0xFFFU);                            // Channel Number
    m_dstId = (uint32_t)((tsbkValue >> 32) & 0xFFFFU);                              // Talkgroup Address

    return true;
}

/* Encode a trunking signalling block. */
void OSP_GRP_VCH_GRANT_UPD::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    if (m_grpVchId != 0U) {
        tsbkValue = m_grpVchId;                                                     // Channel ID
    }
    else {
        tsbkValue = m_siteData.channelId();                                         // Channel ID
    }
    tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                     // Channel Number
    tsbkValue = (tsbkValue << 16) + m_dstId;                                        // Talkgroup Address
    tsbkValue = (tsbkValue << 32) + 0;

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */
std::string OSP_GRP_VCH_GRANT_UPD::toString(bool isp)
{
    return std::string("TSBKO, OSP_GRP_VCH_GRANT_UPD (Group Voice Channel Grant Update)");
}
