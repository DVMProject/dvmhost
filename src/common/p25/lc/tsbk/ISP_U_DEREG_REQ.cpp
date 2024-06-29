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
#include "p25/lc/tsbk/ISP_U_DEREG_REQ.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ISP_U_DEREG_REQ class. */

ISP_U_DEREG_REQ::ISP_U_DEREG_REQ() : TSBK()
{
    m_lco = TSBKO::ISP_U_DEREG_REQ;
}

/* Decode a trunking signalling block. */

bool ISP_U_DEREG_REQ::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_netId = (uint32_t)((tsbkValue >> 36) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 24) & 0xFFFU);                               // System ID
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void ISP_U_DEREG_REQ::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    /* stub */
}

/* Returns a string that represents the current TSBK. */

std::string ISP_U_DEREG_REQ::toString(bool isp)
{
    return std::string("TSBKO, ISP_U_DEREG_REQ (Unit De-Registration Request)");
}
