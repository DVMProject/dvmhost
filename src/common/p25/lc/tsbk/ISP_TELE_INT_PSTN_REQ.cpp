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
#include "p25/lc/tsbk/ISP_TELE_INT_PSTN_REQ.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ISP_TELE_INT_PSTN_REQ class. */

ISP_TELE_INT_PSTN_REQ::ISP_TELE_INT_PSTN_REQ() : TSBK(),
    m_pstnAddress(0U)
{
    m_lco = TSBKO::ISP_TELE_INT_PSTN_REQ;
}

/* Decode a trunking signalling block. */

bool ISP_TELE_INT_PSTN_REQ::decode(const uint8_t* data, bool rawTSBK)
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
    m_pstnAddress = (uint8_t)((tsbkValue >> 24) & 0xFFU);                           // PSTN Address
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void ISP_TELE_INT_PSTN_REQ::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue =
        (m_emergency ? 0x80U : 0x00U) +                                             // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                             // Encrypted Flag
        (m_priority & 0x07U);                                                       // Priority
    tsbkValue = (tsbkValue << 32) + m_pstnAddress;                                  // Reserved + PSTN Address
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string ISP_TELE_INT_PSTN_REQ::toString(bool isp)
{
    return std::string("TSBKO, ISP_TELE_INT_PSTN_REQ (Telephone Interconnect Request - Implicit)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void ISP_TELE_INT_PSTN_REQ::copy(const ISP_TELE_INT_PSTN_REQ& data)
{
    TSBK::copy(data);

    m_pstnAddress = data.m_pstnAddress;
}
