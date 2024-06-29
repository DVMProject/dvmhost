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
#include "dmr/lc/csbk/CSBK_ACK_RSP.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_ACK_RSP class. */
CSBK_ACK_RSP::CSBK_ACK_RSP() : CSBK()
{
    m_CSBKO = CSBKO::ACK_RSP;
}

/* Decode a control signalling block. */
bool CSBK_ACK_RSP::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_GI = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                          // Group/Individual Flag
    m_reason = (uint8_t)((csbkValue >> 33) & 0xFFU);                                // Reason Code
    m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                              // Target Radio Address
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a control signalling block. */
void CSBK_ACK_RSP::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    if (m_reason == ReasonCode::TS_ACK_RSN_REG) {
        csbkValue = 0U;
    } else {
        csbkValue = (m_GI ? 0x40U : 0x00U) +                                        // Source Type
            (m_siteData.siteId() & 0x3FU);                                          // Net + Site LSB
    }
    csbkValue = (csbkValue << 7) + (m_response & 0x7FU);                            // Response Information
    csbkValue = (csbkValue << 8) + (m_reason & 0xFFU);                              // Reason Code
    csbkValue = (csbkValue << 25) + m_dstId;                                        // Target Radio Address
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */
std::string CSBK_ACK_RSP::toString()
{
    return std::string("CSBKO, ACK_RSP (Acknowledge Response)");
}
