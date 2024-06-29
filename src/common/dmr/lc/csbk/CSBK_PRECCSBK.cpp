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
#include "dmr/lc/csbk/CSBK_PRECCSBK.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_PRECCSBK class. */

CSBK_PRECCSBK::CSBK_PRECCSBK() : CSBK()
{
    m_CSBKO = CSBKO::PRECCSBK;
}

/* Decode a control signalling block. */

bool CSBK_PRECCSBK::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_GI = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                          // Group/Individual Flag
    m_dataContent = (((csbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 //
    m_CBF = (uint8_t)((csbkValue >> 48) & 0xFFU);                                   // Blocks to Follow
    m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                            // Target Radio Address
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a control signalling block. */

void CSBK_PRECCSBK::encode(uint8_t* data)
{
    assert(data != nullptr);

    /* stub */
}

/* Returns a string that represents the current CSBK. */

std::string CSBK_PRECCSBK::toString()
{
    return std::string("CSBKO, PRECCSBK (Preamble CSBK)");
}
