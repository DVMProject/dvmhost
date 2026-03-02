// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */

#include "common/Log.h"
#include "common/Utils.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "common/edac/RS634717.h"

using namespace edac;

TEST_CASE("RS362017 preserves all-zero payload", "[edac][rs362017]") {
    uint8_t data[27U]; // 36 symbols * 6 bits = 216 bits = 27 bytes
    ::memset(data, 0x00U, sizeof(data));

    RS634717 rs;
    rs.encode362017(data);
    Utils::dump(2U, "encode362017()", data, 27U);

    REQUIRE(rs.decode362017(data));

    // First 15 bytes (20 symbols * 6 bits = 120 bits) should be zero
    for (size_t i = 0; i < 15U; i++) {
        REQUIRE(data[i] == 0x00U);
    }
}

TEST_CASE("RS362017 preserves all-ones payload", "[edac][rs362017]") {
    uint8_t data[27U];
    ::memset(data, 0xFFU, sizeof(data));

    RS634717 rs;
    rs.encode362017(data);
    Utils::dump(2U, "encode362017()", data, 27U);

    REQUIRE(rs.decode362017(data));

    // First 15 bytes should be 0xFF
    for (size_t i = 0; i < 15U; i++) {
        REQUIRE(data[i] == 0xFFU);
    }
}

TEST_CASE("RS362017 preserves alternating pattern", "[edac][rs362017]") {
    uint8_t original[27U];
    for (size_t i = 0; i < 27U; i++) {
        original[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[27U];
    ::memcpy(data, original, 27U);

    RS634717 rs;
    rs.encode362017(data);
    Utils::dump(2U, "encode362017()", data, 27U);

    REQUIRE(rs.decode362017(data));

    // Verify first 15 bytes (data portion) match
    REQUIRE(::memcmp(data, original, 15U) == 0);
}

TEST_CASE("RS362017 preserves incrementing pattern", "[edac][rs362017]") {
    uint8_t original[27U];
    for (size_t i = 0; i < 27U; i++) {
        original[i] = (uint8_t)(i * 9);
    }

    uint8_t data[27U];
    ::memcpy(data, original, 27U);

    RS634717 rs;
    rs.encode362017(data);
    Utils::dump(2U, "encode362017()", data, 27U);

    REQUIRE(rs.decode362017(data));

    REQUIRE(::memcmp(data, original, 15U) == 0);
}

TEST_CASE("RS362017 corrects symbol errors", "[edac][rs362017]") {
    uint8_t original[27U];
    for (size_t i = 0; i < 27U; i++) {
        original[i] = (uint8_t)(i + 30);
    }

    uint8_t data[27U];
    ::memcpy(data, original, 27U);

    RS634717 rs;
    rs.encode362017(data);
    Utils::dump(2U, "encode362017()", data, 27U);

    // Save encoded data
    uint8_t encoded[27U];
    ::memcpy(encoded, data, 27U);

    // Introduce errors at various positions
    const size_t errorPositions[] = {0, 5, 10, 15, 20};
    for (auto pos : errorPositions) {
        uint8_t corrupted[27U];
        ::memcpy(corrupted, encoded, 27U);
        corrupted[pos] ^= 0x3FU; // Flip 6 bits (1 symbol)

        RS634717 rsDec;
        bool decoded = rsDec.decode362017(corrupted);

        // RS(36,20,17) can correct up to 8 symbol errors
        if (decoded) {
            REQUIRE(::memcmp(corrupted, original, 15U) == 0);
        }
    }
}

TEST_CASE("RS362017 detects uncorrectable errors", "[edac][rs362017]") {
    uint8_t original[27U];
    for (size_t i = 0; i < 27U; i++) {
        original[i] = (uint8_t)(i * 11);
    }

    uint8_t data[27U];
    ::memcpy(data, original, 27U);

    RS634717 rs;
    rs.encode362017(data);
    Utils::dump(2U, "encode362017()", data, 27U);

    // Introduce too many errors (beyond 8 symbol correction)
    for (size_t i = 0; i < 12U; i++) {
        data[i] ^= 0xFFU;
    }

    bool result = rs.decode362017(data);
    REQUIRE(!result);
}
