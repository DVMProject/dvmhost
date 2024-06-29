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
#include "dmr/lc/csbk/CSBK_ALOHA.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_ALOHA class. */
CSBK_ALOHA::CSBK_ALOHA() : CSBK(),
    m_siteTSSync(false),
    m_alohaMask(0U),
    m_backoffNo(1U),
    m_nRandWait(DEFAULT_NRAND_WAIT)
{
    m_CSBKO = CSBKO::ALOHA;
}

/* Decode a control signalling block. */
bool CSBK_ALOHA::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a control signalling block. */
void CSBK_ALOHA::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = (csbkValue << 2) + 0U;                                              // Reserved
    csbkValue = (csbkValue << 1) + ((m_siteTSSync) ? 1U : 0U);                      // Site Time Slot Synchronization
    csbkValue = (csbkValue << 3) + DMR_ALOHA_VER_151;                               // DMR Spec. Version (1.5.1)
    csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);                // Site Timing: Aligned or Offset
    csbkValue = (csbkValue << 1) + ((m_siteData.netActive()) ? 1U : 0U);            // Site Networked
    csbkValue = (csbkValue << 5) + (m_alohaMask & 0x1FU);                           // MS Mask
    csbkValue = (csbkValue << 2) + 0U;                                              // Service Function
    csbkValue = (csbkValue << 4) + (m_nRandWait & 0x0FU);                           // Random Access Wait
    csbkValue = (csbkValue << 1) + ((m_siteData.requireReg()) ? 1U : 0U);           // Require Registration
    csbkValue = (csbkValue << 4) + (m_backoffNo & 0x0FU);                           // Backoff Number
    csbkValue = (csbkValue << 16) + m_siteData.systemIdentity();                    // Site Identity
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */
std::string CSBK_ALOHA::toString()
{
    return std::string("CSBKO, ALOHA (Aloha PDU)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */
void CSBK_ALOHA::copy(const CSBK_ALOHA& data)
{
    CSBK::copy(data);

    m_siteTSSync = data.m_siteTSSync;
    m_alohaMask = data.m_alohaMask;

    m_backoffNo = data.m_backoffNo;
    m_nRandWait = data.m_nRandWait;
}
