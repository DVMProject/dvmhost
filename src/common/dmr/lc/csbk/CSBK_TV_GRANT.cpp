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
#include "dmr/lc/csbk/CSBK_TV_GRANT.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_TV_GRANT class. */

CSBK_TV_GRANT::CSBK_TV_GRANT() : CSBK(),
    m_lateEntry(false)
{
    m_CSBKO = CSBKO::TV_GRANT;
    m_GI = true;
}

/* Decode a control signalling block. */

bool CSBK_TV_GRANT::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    if (!CSBK::decode(data, csbk))
        return false;

    const ulong64_t csbkValue = CSBK::toValue(csbk);
    m_logicalCh1 = (uint32_t)((csbkValue >> 52) & 0xFFFU);                          // Logical Physical Channel 1
    m_slotNo = ((csbkValue >> 51) & 0x01U) == 0x01U ? 2U : 1U;                      // Logical Slot Number
    m_lateEntry = ((csbkValue >> 50) & 0x01U) == 0x01U;                             // Late Entry
    m_emergency = ((csbkValue >> 49) & 0x01U) == 0x01U;                             // Emergency
    m_siteOffsetTiming = ((csbkValue >> 48) & 0x01U) == 0x01U;                      // Site Timing: Aligned or Offset
    m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                            // Talkgroup ID
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address
    m_GI = true;

    return true;
}

/* Encode a control signalling block. */

void CSBK_TV_GRANT::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = (csbkValue << 12) + (m_logicalCh1 & 0xFFFU);                        // Logical Physical Channel 1
    csbkValue = (csbkValue << 1) + ((m_slotNo == 2U) ? 1U : 0U);                    // Logical Slot Number
    csbkValue = (csbkValue << 1) + ((m_lateEntry) ? 1U : 0U);                       // Late Entry
    csbkValue = (csbkValue << 1) + ((m_emergency) ? 1U : 0U);                       // Emergency
    csbkValue = (csbkValue << 1) + ((m_siteOffsetTiming) ? 1U : 0U);                // Site Timing: Aligned or Offset
    csbkValue = (csbkValue << 24) + m_dstId;                                        // Talkgroup ID
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */

std::string CSBK_TV_GRANT::toString()
{
    return std::string("CSBKO, TV_GRANT (Talkgroup Voice Channel Grant)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void CSBK_TV_GRANT::copy(const CSBK_TV_GRANT& data)
{
    CSBK::copy(data);

    m_lateEntry = data.m_lateEntry;
}
