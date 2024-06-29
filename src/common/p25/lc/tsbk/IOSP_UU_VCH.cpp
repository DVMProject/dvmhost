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
#include "p25/lc/tsbk/IOSP_UU_VCH.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the IOSP_UU_VCH class. */

IOSP_UU_VCH::IOSP_UU_VCH() : TSBK()
{
    m_lco = TSBKO::IOSP_UU_VCH;
}

/* Decode a trunking signalling block. */

bool IOSP_UU_VCH::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_emergency = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                   // Emergency Flag
    m_encrypted = (((tsbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                   // Encryption Flag
    m_priority = (((tsbkValue >> 56) & 0xFFU) & 0x07U);                             // Priority
    m_grpVchId = ((tsbkValue >> 52) & 0x0FU);                                       // Channel ID
    m_grpVchNo = ((tsbkValue >> 40) & 0xFFFU);                                      // Channel Number
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                            // Target ID
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void IOSP_UU_VCH::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue =
        (m_emergency ? 0x80U : 0x00U) +                                             // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                             // Encrypted Flag
        (m_priority & 0x07U);                                                       // Priority
    if (m_grpVchId != 0U) {
        tsbkValue = (tsbkValue << 4) + m_grpVchId;                                  // Channel ID
    }
    else {
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel ID
    }
    tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                     // Channel Number
    tsbkValue = (tsbkValue << 24) + m_dstId;                                        // Target ID
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string IOSP_UU_VCH::toString(bool isp)
{
    return (isp) ? std::string("TSBKO, IOSP_UU_VCH (Unit-to-Unit Voice Channel Request)") :
        std::string("TSBKO, IOSP_UU_VCH (Unit-to-Unit Voice Channel Grant)");
}
