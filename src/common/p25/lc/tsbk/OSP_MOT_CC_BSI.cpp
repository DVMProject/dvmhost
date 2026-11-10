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
#include "p25/lc/tsbk/OSP_MOT_CC_BSI.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_MOT_CC_BSI class. */

OSP_MOT_CC_BSI::OSP_MOT_CC_BSI() : TSBK()
{
    m_lco = TSBKO::OSP_MOT_CC_BSI;
}

/* Decode a trunking signalling block. */

bool OSP_MOT_CC_BSI::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a trunking signalling block. */

void OSP_MOT_CC_BSI::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    m_mfId = MFG_MOT;

    // encode the site callsign into the TSBK value using six-bit codes
    for (uint8_t i = 0U; i < MOT_CALLSIGN_LENGTH_BYTES; i++) {
        const uint8_t code = s_siteCallsign[i] == 0U || s_siteCallsign[i] == ' ' ? 0U : (s_siteCallsign[i] - 43U) & 0x3FU;
        tsbkValue = (tsbkValue << 6) + code;
    }

    tsbkValue = (tsbkValue << 4) + s_siteData.channelId();                          // Channel ID
    tsbkValue = (tsbkValue << 12) + s_siteData.channelNo();                         // Channel Number

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string OSP_MOT_CC_BSI::toString(bool isp)
{
    return std::string("TSBKO, OSP_MOT_CC_BSI (Motorola / Control Channel Base Station Identifier)");
}
