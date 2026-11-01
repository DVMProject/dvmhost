// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/lc/LC.h"

using namespace p25::defines;
using namespace p25::lc;

#include <catch2/catch_test_macros.hpp>

#include <cstring>

TEST_CASE("P25 User Alias LCW encoding", "[p25][lc][user-alias]")
{
    LC control;
    control.setMFId(MFG_HARRIS);
    control.setUserAlias("TEST");

    REQUIRE(control.getUserAlias() == "TEST");

    uint8_t raw[P25_TDULC_PAYLOAD_LENGTH_BYTES + 1U];

    SECTION("Part A carries the first seven alias octets") {
        ::memset(raw, 0xFFU, sizeof(raw));
        control.setLCO(LCO::HARRIS_USER_ALIAS_A_ODD);
        control.encodeLC(raw);

        REQUIRE(raw[0U] == LCO::HARRIS_USER_ALIAS_A_ODD);
        REQUIRE(raw[1U] == MFG_HARRIS);
        REQUIRE(raw[2U] == 'T');
        REQUIRE(raw[3U] == 'E');
        REQUIRE(raw[4U] == 'S');
        REQUIRE(raw[5U] == 'T');
        REQUIRE(raw[6U] == 0x00U);
        REQUIRE(raw[7U] == 0x00U);
        REQUIRE(raw[8U] == 0x00U);
    }

    SECTION("Part B carries the final seven padded alias octets") {
        ::memset(raw, 0xFFU, sizeof(raw));
        control.setLCO(LCO::HARRIS_USER_ALIAS_B_EVEN);
        control.encodeLC(raw);

        REQUIRE(raw[0U] == LCO::HARRIS_USER_ALIAS_B_EVEN);
        REQUIRE(raw[1U] == MFG_HARRIS);
        for (uint8_t i = 2U; i < 9U; ++i)
            REQUIRE(raw[i] == 0x00U);
    }
}
