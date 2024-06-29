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

ISP_EMERG_ALRM_REQ::ISP_EMERG_ALRM_REQ() : TSBK()
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

    /*
    ** bryanb: this is a bit of a hack -- because the EMERG ALRM and DENY have the same
    ** opcode; the following are used by TSBK_OSP_DENY_RSP; best way to check is for m_response > 0
    */
    m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                     // Additional Info. Flag
    m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                               // Service Type
    m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                              // Reason

    if (m_response == 0U) {
        m_emergency = true;
    } else {
        m_emergency = false;
    }

    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                              // Target Radio Address
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void ISP_EMERG_ALRM_REQ::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    /* stub */
}

/* Returns a string that represents the current TSBK. */

std::string ISP_EMERG_ALRM_REQ::toString(bool isp)
{
    return std::string("TSBKO, ISP_EMERG_ALRM_REQ (Emergency Alarm Request)");
}
