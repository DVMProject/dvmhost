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
#include "dmr/lc/csbk/CSBK_RAW.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_RAW class. */

CSBK_RAW::CSBK_RAW() : CSBK(),
    m_csbk(nullptr)
{
    m_CSBKO = CSBKO::RAND;
}

/* Finalizes a new instance of the CSBK_RAW class. */

CSBK_RAW::~CSBK_RAW()
{
    if (m_csbk != nullptr) {
        delete m_csbk;
    }
}

/* Decode a control signalling block. */

bool CSBK_RAW::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a control signalling block. */

void CSBK_RAW::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(m_csbk != nullptr);

    /* stub */

    CSBK::encode(data, m_csbk);
}

/* Sets the CSBK to encode. */

void CSBK_RAW::setCSBK(const uint8_t* csbk)
{
    assert(csbk != nullptr);

    m_CSBKO = csbk[0U] & 0x3FU;                                                     // CSBKO
    m_lastBlock = (csbk[0U] & 0x80U) == 0x80U;                                      // Last Block Marker
    m_FID = csbk[1U];                                                               // Feature ID

    m_csbk = new uint8_t[DMR_CSBK_LENGTH_BYTES];
    ::memset(m_csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    ::memcpy(m_csbk, csbk, DMR_CSBK_LENGTH_BYTES);
}
