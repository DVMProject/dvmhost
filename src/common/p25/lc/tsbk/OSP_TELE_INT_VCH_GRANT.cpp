// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Charles Bricker, K3NOP
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/OSP_TELE_INT_VCH_GRANT.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_TELE_INT_VCH_GRANT class. */

OSP_TELE_INT_VCH_GRANT::OSP_TELE_INT_VCH_GRANT() : TSBK(),
    m_callTimer(0U)
{
    m_lco = TSBKO::IOSP_TELE_INT_DIAL;
}

/* Decode a trunking signalling block. */

bool OSP_TELE_INT_VCH_GRANT::decode(const uint8_t* data, bool rawTSBK)
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
    m_callTimer = (uint16_t)((tsbkValue >> 24) & 0xFFFFU);                          // Call Timer
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source/Target Radio Address
    m_dstId = m_srcId;

    return true;
}

/* Encode a trunking signalling block. */

void OSP_TELE_INT_VCH_GRANT::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
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
        tsbkValue = (tsbkValue << 4) + s_siteData.channelId();                      // Channel ID
    }
    tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                     // Channel Number
    tsbkValue = (tsbkValue << 16) + m_callTimer;                                    // Call Timer
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source/Target Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string OSP_TELE_INT_VCH_GRANT::toString(bool isp)
{
    return std::string("TSBKO, OSP_TELE_INT_VCH_GRANT (Telephone Interconnect Voice Channel Grant)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void OSP_TELE_INT_VCH_GRANT::copy(const OSP_TELE_INT_VCH_GRANT& data)
{
    TSBK::copy(data);

    m_callTimer = data.m_callTimer;
}
