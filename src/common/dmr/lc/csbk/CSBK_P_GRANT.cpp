// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/lc/csbk/CSBK_P_GRANT.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_P_GRANT class. */

CSBK_P_GRANT::CSBK_P_GRANT() : CSBK()
{
    m_CSBKO = CSBKO::TV_GRANT;
}

/* Decode a control signalling block. */

bool CSBK_P_GRANT::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a control signalling block. */

void CSBK_P_GRANT::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                        // Logical Physical Channel 1
    csbkValue = (csbkValue << 1) + ((m_slotNo == 2U) ? 1U : 0U);                    // Logical Slot Number
    csbkValue = (csbkValue << 1) + 0U;                                              // High Rate Flag - Always Single Slot Data
    csbkValue = (csbkValue << 1) + ((m_emergency) ? 1U : 0U);                       // Emergency
    csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);                // Site Timing: Aligned or Offset
    csbkValue = (csbkValue << 24) + m_dstId;                                        // Talkgroup ID
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */

std::string CSBK_P_GRANT::toString()
{
    return std::string("CSBKO, P_GRANT (Payload Grant)");
}
