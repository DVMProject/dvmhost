// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/lc/mac/MAC_RELEASE.h"
#include "common/p25/P25Defines.h"

using namespace p25::lc;
using namespace p25::lc::mac;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MAC_RELEASE class. */

MAC_RELEASE::MAC_RELEASE() : MACPDU(p25::defines::P2_MAC_MCO::MAC_RELEASE)
{
    /* stub */
}

/* Returns a string that represents the current MAC PDU. */

std::string MAC_RELEASE::toString(bool isp)
{
    return std::string("MAC PDU, RELEASE (Release)");
}
