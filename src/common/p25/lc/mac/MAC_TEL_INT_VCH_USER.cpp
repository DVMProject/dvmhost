// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/lc/mac/MAC_TEL_INT_VCH_USER.h"
#include "common/p25/P25Defines.h"

using namespace p25::lc;
using namespace p25::lc::mac;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MAC_TEL_INT_VCH_USER class. */

MAC_TEL_INT_VCH_USER::MAC_TEL_INT_VCH_USER() : MACPDU(p25::defines::P2_MAC_MCO::TEL_INT_VCH_USER)
{
    /* stub */
}

/* Decodes a MAC PDU from the specified LC control data. */

bool MAC_TEL_INT_VCH_USER::decode(const LC& control)
{
    return MACPDU::decode(control);
}

/* Returns a string that represents the current MAC PDU. */

std::string MAC_TEL_INT_VCH_USER::toString(bool isp)
{
    return std::string("MAC PDU, TEL INT VCH USER (Telephone Interconnect Voice Channel User)");
}
