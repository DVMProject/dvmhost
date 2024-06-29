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
#include "p25/lc/tsbk/OSP_DVM_GIT_HASH.h"
#include "HostMain.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_DVM_GIT_HASH class. */

OSP_DVM_GIT_HASH::OSP_DVM_GIT_HASH() : TSBK()
{
    m_lco = TSBKO::OSP_DVM_GIT_HASH;
}

/* Decode a trunking signalling block. */

bool OSP_DVM_GIT_HASH::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a trunking signalling block. */

void OSP_DVM_GIT_HASH::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    m_mfId = MFG_DVM_OCS;

    tsbkValue = g_gitHashBytes[0];                                                  // ...
    tsbkValue = (tsbkValue << 8) + (g_gitHashBytes[1U]);                            // ...
    tsbkValue = (tsbkValue << 8) + (g_gitHashBytes[2U]);                            // ...
    tsbkValue = (tsbkValue << 8) + (g_gitHashBytes[3U]);                            // ...
    tsbkValue = (tsbkValue << 16) + 0U;
    tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                          // Channel ID
    tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                         // Channel Number

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string OSP_DVM_GIT_HASH::toString(bool isp)
{
    return std::string("TSBKO, OSP_DVM_GIT_HASH (DVM Git Hash Identifier)");
}
