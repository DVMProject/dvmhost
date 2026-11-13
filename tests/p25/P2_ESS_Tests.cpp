// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "host/p25/phase2/Slot.h"
#include "host/HostTestHooks.h"
#include "common/BitManipulation.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

using namespace p25::phase2;

TEST_CASE("P25 Phase 2 ESS reconstructs ALGID KID and MI", "[p2][ess]")
{
    edac::RS634717 rs;
    std::array<uint8_t, 44U> codeword{};
    codeword[0U] = 0x80U;
    codeword[1U] = 0x12U;
    codeword[2U] = 0x34U;
    for (uint32_t i = 0U; i < 9U; i++) codeword[3U + i] = static_cast<uint8_t>(0xA0U + i);
    rs.encode441629(codeword.data());

    Slot::init(nullptr, true, 1000U, nullptr, nullptr, nullptr, nullptr, nullptr);
    Slot slot(0U, 1000U, 1000U, 4096U, false, false);

    p25::lc::LC mac;
    mac.setGroup(true);
    mac.setLCO(P25DEF::P2_MAC_MCO::GROUP);
    mac.setP2DUID(P25DEF::P2_DUID::FACCH_UNSCRAMBLED);
    mac.setMACPDUOpcode(P25DEF::P2_MAC_HEADER_OPCODE::PTT);
    std::array<uint8_t, 40U> macBurst{};
    mac.encodeVCH_MACPDU(macBurst.data(), true);

    for (uint32_t burstNo = 0U; burstNo < 4U; burstNo++) {
        std::array<uint8_t, 40U> burst{};
        for (uint32_t bit = 0U; bit < 24U; bit++)
            WRITE_BIT(burst.data(), 148U + bit, READ_BIT(codeword.data(), burstNo * 24U + bit));
        REQUIRE(slot.processNetwork(burst.data(), burst.size(), p25::lc::LC(), P25DEF::P2_DUID::VTCH_4V, 0U, 0U));
        REQUIRE(slot.processNetwork(macBurst.data(), macBurst.size(), mac,
            P25DEF::P2_DUID::FACCH_UNSCRAMBLED, 0U, 0U));
    }

    std::array<uint8_t, 40U> burst2V{};
    for (uint32_t bit = 0U; bit < 168U; bit++)
        WRITE_BIT(burst2V.data(), 148U + bit, READ_BIT(codeword.data(), 96U + bit));
    REQUIRE(slot.processNetwork(burst2V.data(), burst2V.size(), p25::lc::LC(), P25DEF::P2_DUID::VTCH_2V, 0U, 0U));
    REQUIRE(HostTestHooks::p25P2ESSComplete(slot));
    REQUIRE(HostTestHooks::p25P2ESSAlgId(slot) == 0x80U);
    REQUIRE(HostTestHooks::p25P2ESSKeyId(slot) == 0x1234U);

    uint8_t mi[9U] = {0U};
    HostTestHooks::p25P2ESSMI(slot, mi);
    REQUIRE(std::memcmp(mi, codeword.data() + 3U, sizeof(mi)) == 0);
}
