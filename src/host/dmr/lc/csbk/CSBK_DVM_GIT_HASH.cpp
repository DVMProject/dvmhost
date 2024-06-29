// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/lc/csbk/CSBK_DVM_GIT_HASH.h"
#include "HostMain.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_DVM_GIT_HASH class. */

CSBK_DVM_GIT_HASH::CSBK_DVM_GIT_HASH() : CSBK()
{
    m_CSBKO = CSBKO::DVM_GIT_HASH;
    m_FID = FID_DVM_OCS;
}

/* Decode a control signalling block. */

bool CSBK_DVM_GIT_HASH::decode(const uint8_t* data)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a control signalling block. */

void CSBK_DVM_GIT_HASH::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = g_gitHashBytes[0];                                                  // ...
    csbkValue = (csbkValue << 8) + (g_gitHashBytes[1U]);                            // ...
    csbkValue = (csbkValue << 8) + (g_gitHashBytes[2U]);                            // ...
    csbkValue = (csbkValue << 8) + (g_gitHashBytes[3U]);                            // ...
    csbkValue = (csbkValue << 16) + 0U;
    csbkValue = (csbkValue << 4) + m_siteIdenEntry.channelId();                     // Channel ID
    csbkValue = (csbkValue << 12) + m_logicalCh1;                                   // Channel Number

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */

std::string CSBK_DVM_GIT_HASH::toString()
{
    return std::string("CSBKO, DVM_GIT_HASH (DVM Git Hash Identifier)");
}
