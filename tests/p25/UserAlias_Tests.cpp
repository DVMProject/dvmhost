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
#include "common/p25/lc/MotTalkerAlias.h"

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

TEST_CASE("P25 Motorola talker alias assembly", "[p25][lc][user-alias][motorola]")
{
    MotTalkerAlias assembler;
    const uint8_t words[][9U] = {
        { 0x15U, 0x90U, 0x1BU, 0xBDU, 0x07U, 0x01U, 0x00U, 0xF8U, 0xBAU },
        { 0x17U, 0x90U, 0x01U, 0xFBU, 0xEEU, 0x00U, 0x2AU, 0xEEU, 0x67U },
        { 0x17U, 0x90U, 0x02U, 0xF8U, 0x52U, 0x83U, 0xEDU, 0x10U, 0x81U },
        { 0x17U, 0x90U, 0x03U, 0xFEU, 0x33U, 0xC0U, 0x3EU, 0x9BU, 0x3EU },
        { 0x17U, 0x90U, 0x04U, 0xF3U, 0x56U, 0x47U, 0xDEU, 0x0CU, 0x00U },
        { 0x17U, 0x90U, 0x05U, 0xFCU, 0x8AU, 0x83U, 0xE3U, 0x51U, 0xE4U },
        { 0x17U, 0x90U, 0x06U, 0xF0U, 0x79U, 0xF5U, 0x92U, 0xCFU, 0x37U },
        { 0x17U, 0x90U, 0x07U, 0xF9U, 0x4BU, 0x30U, 0x00U, 0x00U, 0x00U }
    };

    MotTalkerAliasResult result;
    for (std::size_t i = 0U; i + 1U < sizeof(words) / sizeof(words[0U]); ++i)
        REQUIRE_FALSE(assembler.decode(words[i], result));
    REQUIRE(assembler.decode(words[7U], result));

    REQUIRE(result.netId == 0xBEE00U);
    REQUIRE(result.sysId == 0x2AEU);
    REQUIRE(result.srcId == 0xE67852U);
    REQUIRE(result.dstId == 0x1BBDU);
    REQUIRE(result.sequence == 0x0FU);
    REQUIRE(result.alias == "NYSP-1D82UHFM");
}

TEST_CASE("P25 Motorola talker alias encoding round trip", "[p25][lc][user-alias][motorola]")
{
    const std::vector<std::vector<uint8_t>> words = MotTalkerAlias::encode(
        0xBB800U, 0x123U, 0x456789U, 0x2345U, "SMARTCONNECT", 0x0AU);
    REQUIRE(words.size() > 1U);

    MotTalkerAlias assembler;
    MotTalkerAliasResult result;
    for (std::size_t i = 0U; i < words.size(); ++i)
        REQUIRE(assembler.decode(words[i].data(), result) == (i + 1U == words.size()));

    REQUIRE(result.netId == 0xBB800U);
    REQUIRE(result.sysId == 0x123U);
    REQUIRE(result.srcId == 0x456789U);
    REQUIRE(result.dstId == 0x2345U);
    REQUIRE(result.sequence == 0x0AU);
    REQUIRE(result.alias == "SMARTCONNECT");
}

TEST_CASE("P25 Motorola talker alias captured encoding", "[p25][lc][user-alias][motorola]")
{
    const uint8_t expected[][9U] = {
        { 0x15U, 0x90U, 0x1BU, 0xBDU, 0x07U, 0x01U, 0x00U, 0xF8U, 0xBAU },
        { 0x17U, 0x90U, 0x01U, 0xFBU, 0xEEU, 0x00U, 0x2AU, 0xEEU, 0x67U },
        { 0x17U, 0x90U, 0x02U, 0xF8U, 0x52U, 0x83U, 0xEDU, 0x10U, 0x81U },
        { 0x17U, 0x90U, 0x03U, 0xFEU, 0x33U, 0xC0U, 0x3EU, 0x9BU, 0x3EU },
        { 0x17U, 0x90U, 0x04U, 0xF3U, 0x56U, 0x47U, 0xDEU, 0x0CU, 0x00U },
        { 0x17U, 0x90U, 0x05U, 0xFCU, 0x8AU, 0x83U, 0xE3U, 0x51U, 0xE4U },
        { 0x17U, 0x90U, 0x06U, 0xF0U, 0x79U, 0xF5U, 0x92U, 0xCFU, 0x37U },
        { 0x17U, 0x90U, 0x07U, 0xF9U, 0x4BU, 0x30U, 0x00U, 0x00U, 0x00U }
    };

    const std::vector<std::vector<uint8_t>> words = MotTalkerAlias::encode(
        0xBEE00U, 0x2AEU, 0xE67852U, 0x1BBDU, "NYSP-1D82UHFM", 0x0FU);

    REQUIRE(words.size() == sizeof(expected) / sizeof(expected[0U]));
    for (std::size_t word = 0U; word < words.size(); ++word) {
        REQUIRE(words[word].size() == sizeof(expected[word]));
        REQUIRE(::memcmp(words[word].data(), expected[word], sizeof(expected[word])) == 0);
    }
}
