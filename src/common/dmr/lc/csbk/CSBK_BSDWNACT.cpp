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
#include "dmr/lc/csbk/CSBK_BSDWNACT.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_BSDWNACT class. */
CSBK_BSDWNACT::CSBK_BSDWNACT() : CSBK(),
    m_bsId(0U)
{
    m_CSBKO = CSBKO::BSDWNACT;
}

/* Decode a control signalling block. */
bool CSBK_BSDWNACT::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_bsId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                             // Base Station Address
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a control signalling block. */
void CSBK_BSDWNACT::encode(uint8_t* data)
{
    assert(data != nullptr);

    /* stub */
}

/* Returns a string that represents the current CSBK. */
std::string CSBK_BSDWNACT::toString()
{
    return std::string("CSBKO, BSDWNACT (BS Outbound Activation)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */
void CSBK_BSDWNACT::copy(const CSBK_BSDWNACT& data)
{
    CSBK::copy(data);

    m_bsId = data.m_bsId;
}
