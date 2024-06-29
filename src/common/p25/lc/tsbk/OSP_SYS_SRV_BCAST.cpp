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
#include "p25/lc/tsbk/OSP_SYS_SRV_BCAST.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_SYS_SRV_BCAST class. */
OSP_SYS_SRV_BCAST::OSP_SYS_SRV_BCAST() : TSBK()
{
    m_lco = TSBKO::OSP_SYS_SRV_BCAST;
}

/* Decode a trunking signalling block. */
bool OSP_SYS_SRV_BCAST::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a trunking signalling block. */
void OSP_SYS_SRV_BCAST::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    const uint32_t services = (m_siteData.netActive()) ? SystemService::NET_ACTIVE : 0U | SYS_SRV_DEFAULT;

    ulong64_t tsbkValue = 0U;

    tsbkValue = (tsbkValue << 16) + services;                                       // System Services Available
    tsbkValue = (tsbkValue << 24) + services;                                       // System Services Supported

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */
std::string OSP_SYS_SRV_BCAST::toString(bool isp)
{
    return std::string("TSBKO, OSP_SYS_SRV_BCAST (System Service Broadcast)");
}
