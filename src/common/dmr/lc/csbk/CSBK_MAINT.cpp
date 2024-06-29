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
#include "dmr/lc/csbk/CSBK_MAINT.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_MAINT class. */
CSBK_MAINT::CSBK_MAINT() : CSBK(),
    m_maintKind(0U)
{
    m_CSBKO = CSBKO::MAINT;
}

/* Decode a control signalling block. */
bool CSBK_MAINT::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_maintKind = (uint8_t)(((csbkValue >> 48) & 0xFFU) >> 1);                      // Maintainence Kind
    m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFU);                              // Target Radio Address
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a control signalling block. */
void CSBK_MAINT::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = (csbkValue << 3) + (m_maintKind & 0x07U);                           // Maintainence Kind
    csbkValue = (csbkValue << 25) + m_dstId;                                        // Target Radio Address
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */
std::string CSBK_MAINT::toString()
{
    return std::string("CSBKO, MAINT (Call Maintainence)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */
void CSBK_MAINT::copy(const CSBK_MAINT& data)
{
    CSBK::copy(data);

    m_maintKind = data.m_maintKind;
}
