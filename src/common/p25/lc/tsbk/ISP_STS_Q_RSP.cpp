// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/lc/tsbk/ISP_STS_Q_RSP.h"

using namespace p25::defines;
using namespace p25::lc::tsbk;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ISP_STS_Q_RSP class. */

ISP_STS_Q_RSP::ISP_STS_Q_RSP() :
    IOSP_STS_UPDT()
{
    m_lco = TSBKO::ISP_STS_Q_RSP;
}

/* Returns a string that represents the current TSBK. */

std::string ISP_STS_Q_RSP::toString(bool isp)
{
    (void)isp;
    return "TSBKO, ISP_STS_Q_RSP (Status Query Response)";
}
