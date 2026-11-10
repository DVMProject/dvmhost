// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024,2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/ISP_EMERG_ALRM_REQ.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ISP_EMERG_ALRM_REQ class. */

ISP_EMERG_ALRM_REQ::ISP_EMERG_ALRM_REQ() : TSBK(),
    m_specialInfo1(0U),
    m_specialInfo2(0U)
{
    m_lco = TSBKO::ISP_EMERG_ALRM_REQ;
}

/* Decode a trunking signalling block. */

bool ISP_EMERG_ALRM_REQ::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_specialInfo1 = (uint8_t)((tsbkValue >> 56) & 0xFFU);                          // Emergency Special Information 1
    m_specialInfo2 = (uint8_t)((tsbkValue >> 48) & 0xFFU);                          // Emergency Special Information 2
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                              // Group Address
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    // Emergency Alarm Request and Denial Response share an opcode. Retain the
    // direction discriminator used by network-originated TSBKs; the RF receive
    // path independently identifies this opcode as an inbound alarm request.
    m_emergency = (m_specialInfo2 == 0U) &&
        (m_specialInfo1 == 0U || (m_specialInfo1 & 0x01U) == 0x01U);

    return true;
}

/* Encode a trunking signalling block. */

void ISP_EMERG_ALRM_REQ::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = m_specialInfo1;                                           // Emergency Special Information 1
    tsbkValue = (tsbkValue << 8) + m_specialInfo2;                                  // Emergency Special Information 2
    tsbkValue = (tsbkValue << 8) + 0U;                                              // Reserved
    tsbkValue = (tsbkValue << 16) + (m_dstId & 0xFFFFU);                            // Group Address
    tsbkValue = (tsbkValue << 24) + (m_srcId & 0xFFFFFFU);                          // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string ISP_EMERG_ALRM_REQ::toString(bool isp)
{
    return std::string("TSBKO, ISP_EMERG_ALRM_REQ (Emergency Alarm Request)");
}
