// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/lc/mac/MACFactory.h"
#include "common/p25/P25Defines.h"

using namespace p25::lc;
using namespace p25::lc::mac;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Create and decode a MAC PDU from decoded LC data. */

std::unique_ptr<MACPDU> MACFactory::createMACPDU(const LC& control)
{
    switch (control.getLCO()) {
    case p25::defines::P2_MAC_MCO::GROUP:
        return decode(new MAC_GROUP_VCH_USER(), control);
    case p25::defines::P2_MAC_MCO::PRIVATE:
        return decode(new MAC_UU_VCH_USER(), control);
    case p25::defines::P2_MAC_MCO::TEL_INT_VCH_USER:
        return decode(new MAC_TEL_INT_VCH_USER(), control);
    case p25::defines::P2_MAC_MCO::MAC_RELEASE:
        return decode(new MAC_RELEASE(), control);
    default:
        break;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Decode a MAC PDU from decoded LC data. */

std::unique_ptr<MACPDU> MACFactory::decode(MACPDU* mac, const LC& control)
{
    if (mac == nullptr || !mac->decode(control)) {
        delete mac;
        return nullptr;
    }

    return std::unique_ptr<MACPDU>(mac);
}
