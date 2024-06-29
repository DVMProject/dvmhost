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
#include "p25/lc/tsbk/IOSP_U_REG.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the IOSP_U_REG class. */

IOSP_U_REG::IOSP_U_REG() : TSBK()
{
    m_lco = TSBKO::IOSP_U_REG;
}

/* Decode a trunking signalling block. */

bool IOSP_U_REG::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

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

void IOSP_U_REG::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    ulong64_t tsbkValue = 0U;

    tsbkValue = (tsbkValue << 2) + (m_response & 0x3U);                             // Unit Registration Response
    tsbkValue = (tsbkValue << 12) + m_siteData.sysId();                             // System ID
    tsbkValue = (tsbkValue << 24) + m_dstId;                                        // Source ID
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string IOSP_U_REG::toString(bool isp)
{
    return (isp) ? std::string("TSBKO, IOSP_U_REG (Unit Registration Request)") :
        std::string("TSBKO, IOSP_U_REG (Unit Registration Response)");
}
