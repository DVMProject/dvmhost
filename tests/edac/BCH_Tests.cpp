// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025,2026 Bryan Biedenkapp, N2PLL
 * 
 */

#include "common/Defines.h"
#include "common/edac/BCH.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>

using namespace edac;

TEST_CASE("BCH encodes the P25 NID generator matrix", "[edac][bch][p25]")
{
    uint8_t nid[8U] = { 0x29U, 0x35U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
    const uint8_t expected[8U] = { 0x29U, 0x35U, 0x55U, 0xEFU, 0x2CU, 0x65U, 0x34U, 0x36U };

    BCH bch;
    bch.encode(nid);

    REQUIRE(::memcmp(nid, expected, sizeof(nid)) == 0);
}

TEST_CASE("BCH corrects every single-bit position", "[edac][bch][p25]")
{
    const uint8_t expected[8U] = { 0x29U, 0x35U, 0x55U, 0xEFU, 0x2CU, 0x65U, 0x34U, 0x36U };
    BCH bch;

    for (uint32_t bit = 0U; bit < 63U; bit++) {
        uint8_t nid[8U];
        ::memcpy(nid, expected, sizeof(nid));
        WRITE_BIT(nid, bit, READ_BIT(nid, bit) == 0U);

        uint32_t errors = 0U;
        REQUIRE(bch.decode(nid, errors));
        REQUIRE(errors == 1U);
        REQUIRE(::memcmp(nid, expected, sizeof(nid)) == 0);
    }
}

TEST_CASE("BCH corrects eleven distributed errors", "[edac][bch][p25]")
{
    const uint8_t expected[8U] = { 0x29U, 0x35U, 0x55U, 0xEFU, 0x2CU, 0x65U, 0x34U, 0x36U };
    const uint32_t errorBits[11U] = { 0U, 3U, 9U, 14U, 20U, 27U, 33U, 39U, 46U, 54U, 62U };
    uint8_t nid[8U];
    ::memcpy(nid, expected, sizeof(nid));
    for (uint32_t bit : errorBits)
        WRITE_BIT(nid, bit, READ_BIT(nid, bit) == 0U);

    BCH bch;
    uint32_t errors = 0U;
    REQUIRE(bch.decode(nid, errors));
    REQUIRE(errors == 11U);
    REQUIRE(::memcmp(nid, expected, sizeof(nid)) == 0);
}

TEST_CASE("BCH leaves the separate P25 parity bit unchanged", "[edac][bch][p25]")
{
    uint8_t nid[8U] = { 0x29U, 0x35U, 0x55U, 0xEFU, 0x2CU, 0x65U, 0x34U, 0x37U };

    BCH bch;
    uint32_t errors = 0U;
    REQUIRE(bch.decode(nid, errors));
    REQUIRE(errors == 0U);
    REQUIRE((nid[7U] & 0x01U) == 0x01U);
}
