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
#include "dmr/lc/csbk/CSBK_EXT_FNCT.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_EXT_FNCT class. */

CSBK_EXT_FNCT::CSBK_EXT_FNCT() : CSBK(),
    m_extendedFunction(ExtendedFunctions::CHECK)
{
    m_CSBKO = CSBKO::EXT_FNCT;
    m_FID = FID_MOT;
}

/* Decode a control signalling block. */

bool CSBK_EXT_FNCT::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_dataContent = (((csbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 //
    m_extendedFunction = (uint8_t)((csbkValue >> 48) & 0xFFU);                      // Function
    m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                            // Target Radio Address
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a control signalling block. */

void CSBK_EXT_FNCT::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue =
        (m_GI ? 0x40U : 0x00U) +                                                    // Group or Invididual
        (m_dataContent ? 0x80U : 0x00U);
    csbkValue = (csbkValue << 8) + m_extendedFunction;                              // Function
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address
    csbkValue = (csbkValue << 24) + m_dstId;                                        // Target Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */

std::string CSBK_EXT_FNCT::toString()
{
    return std::string("CSBKO, EXT_FNCT (Extended Function)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void CSBK_EXT_FNCT::copy(const CSBK_EXT_FNCT& data)
{
    CSBK::copy(data);

    m_extendedFunction = data.m_extendedFunction;
}
