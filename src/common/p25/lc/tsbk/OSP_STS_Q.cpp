// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/lc/tsbk/OSP_STS_Q.h"

using namespace p25::defines;
using namespace p25::lc::tsbk;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_STS_Q class. */

OSP_STS_Q::OSP_STS_Q() :
    IOSP_STS_UPDT()
{
    m_lco = TSBKO::OSP_STS_Q;
}

/* Encodes a status query trunking signalling block. */

void OSP_STS_Q::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    setStatus(0U);
    IOSP_STS_UPDT::encode(data, rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string OSP_STS_Q::toString(bool isp)
{
    (void)isp;
    return "TSBKO, OSP_STS_Q (Status Query)";
}
