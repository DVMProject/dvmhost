// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "common/p25/P25Defines.h"
#include "common/p25/lc/LC.h"
#include "common/p25/lc/mac/MACFactory.h"
#include "common/p25/lc/mac/MAC_GROUP_VCH_USER.h"
#include "common/p25/lc/mac/MAC_RELEASE.h"
#include "common/p25/lc/mac/MAC_TEL_INT_VCH_USER.h"
#include "common/p25/lc/mac/MAC_UU_VCH_USER.h"

#include <catch2/catch_test_macros.hpp>

using namespace p25;
using namespace p25::lc;
using namespace p25::lc::mac;

TEST_CASE("P25 Phase 2 group MAC PDU carries LC fields", "[p2][mac]")
{
    LC control;
    control.setLCO(defines::P2_MAC_MCO::GROUP);
    control.setSrcId(0x123456U);
    control.setDstId(0x2345U);
    control.setGroup(true);
    control.setEmergency(true);
    control.setEncrypted(true);
    control.setPriority(5U);

    MAC_GROUP_VCH_USER pdu;
    REQUIRE(pdu.decode(control));
    REQUIRE(pdu.getSrcId() == 0x123456U);
    REQUIRE(pdu.getDstId() == 0x2345U);
    REQUIRE(pdu.getGroup());
    REQUIRE(pdu.getEmergency());
    REQUIRE(pdu.getEncrypted());
    REQUIRE(pdu.getPriority() == 5U);

    LC encoded;
    pdu.encode(encoded);
    REQUIRE(encoded.getLCO() == defines::P2_MAC_MCO::GROUP);
    REQUIRE(encoded.getSrcId() == 0x123456U);
    REQUIRE(encoded.getDstId() == 0x2345U);
}

TEST_CASE("P25 Phase 2 private and release MAC PDUs validate opcodes", "[p2][mac]")
{
    LC privateControl;
    privateControl.setLCO(defines::P2_MAC_MCO::PRIVATE);
    privateControl.setGroup(false);
    privateControl.setSrcId(0x123456U);
    privateControl.setDstId(0x654321U);

    MAC_UU_VCH_USER privatePdu;
    REQUIRE(privatePdu.decode(privateControl));
    REQUIRE(privatePdu.getDstId() == 0x654321U);

    LC releaseControl;
    releaseControl.setLCO(defines::P2_MAC_MCO::MAC_RELEASE);
    MAC_RELEASE release;
    REQUIRE(release.decode(releaseControl));
    REQUIRE_FALSE(privatePdu.decode(releaseControl));

    LC telephoneControl;
    telephoneControl.setLCO(defines::P2_MAC_MCO::TEL_INT_VCH_USER);
    MAC_TEL_INT_VCH_USER telephone;
    REQUIRE(telephone.decode(telephoneControl));
}

TEST_CASE("P25 Phase 2 MAC factory selects by opcode", "[p2][mac]")
{
    LC control;
    control.setLCO(defines::P2_MAC_MCO::GROUP);
    control.setGroup(true);

    std::unique_ptr<MACPDU> mac = MACFactory::createMACPDU(control);
    REQUIRE(mac != nullptr);
    REQUIRE(mac->getOpcode() == defines::P2_MAC_MCO::GROUP);

    control.setLCO(0x7FU);
    REQUIRE(MACFactory::createMACPDU(control) == nullptr);
}
