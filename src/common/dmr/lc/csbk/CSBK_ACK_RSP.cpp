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

    m_response = (uint8_t)((csbkValue >> 57U) & 0x7FU);                             // Response Information
    m_GI = (m_response & 0x40U) == 0x40U;                                           // Group/Individual Flag
    m_reason = (uint8_t)((csbkValue >> 49U) & 0xFFU);                               // Reason Code
    m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                            // Target Radio Address
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a control signalling block. */

void CSBK_ACK_RSP::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    uint8_t response = m_response & 0x7FU;
    if (m_reason != ReasonCode::TS_ACK_RSN_REG && m_GI)
        response |= 0x40U;

    csbkValue = response;                                                           // Response Information
    csbkValue = (csbkValue << 8) + (m_reason & 0xFFU);                              // Reason Code
    csbkValue = (csbkValue << 1);                                                   // Reserved
    csbkValue = (csbkValue << 24) + (m_dstId & 0xFFFFFFU);                          // Target Radio Address
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */

std::string CSBK_ACK_RSP::toString()
{
    return std::string("CSBKO, ACK_RSP (Acknowledge Response)");
}
