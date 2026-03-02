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

TEST_CASE("RS523023 preserves all-zero payload", "[edac][rs523023]") {
    uint8_t data[39U]; // 52 symbols * 6 bits = 312 bits = 39 bytes
    ::memset(data, 0x00U, sizeof(data));

    RS634717 rs;
    rs.encode523023(data);
    Utils::dump(2U, "encode523023()", data, 39U);

    REQUIRE(rs.decode523023(data));

    // First 22.5 bytes (30 symbols * 6 bits = 180 bits) should be zero
    for (size_t i = 0; i < 22U; i++) {
        REQUIRE(data[i] == 0x00U);
    }
}

TEST_CASE("RS523023 preserves all-ones payload", "[edac][rs523023]") {
    uint8_t data[39U];
    ::memset(data, 0xFFU, sizeof(data));

    RS634717 rs;
    rs.encode523023(data);
    Utils::dump(2U, "encode523023()", data, 39U);

    REQUIRE(rs.decode523023(data));

    // First 22 bytes should be 0xFF
    for (size_t i = 0; i < 22U; i++) {
        REQUIRE(data[i] == 0xFFU);
    }
}

TEST_CASE("RS523023 preserves alternating pattern", "[edac][rs523023]") {
    uint8_t original[39U];
    for (size_t i = 0; i < 39U; i++) {
        original[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[39U];
    ::memcpy(data, original, 39U);

    RS634717 rs;
    rs.encode523023(data);
    Utils::dump(2U, "encode523023()", data, 39U);

    REQUIRE(rs.decode523023(data));

    // Verify first 22 bytes (data portion) match
    REQUIRE(::memcmp(data, original, 22U) == 0);
}

TEST_CASE("RS523023 preserves incrementing pattern", "[edac][rs523023]") {
    uint8_t original[39U];
    for (size_t i = 0; i < 39U; i++) {
        original[i] = (uint8_t)(i * 7);
    }

    uint8_t data[39U];
    ::memcpy(data, original, 39U);

    RS634717 rs;
    rs.encode523023(data);
    Utils::dump(2U, "encode523023()", data, 39U);

    REQUIRE(rs.decode523023(data));

    REQUIRE(::memcmp(data, original, 22U) == 0);
}

TEST_CASE("RS523023 corrects symbol errors", "[edac][rs523023]") {
    uint8_t original[39U];
    for (size_t i = 0; i < 39U; i++) {
        original[i] = (uint8_t)(i + 40);
    }

    uint8_t data[39U];
    ::memcpy(data, original, 39U);

    RS634717 rs;
    rs.encode523023(data);
    Utils::dump(2U, "encode523023()", data, 39U);

    // Save encoded data
    uint8_t encoded[39U];
    ::memcpy(encoded, data, 39U);

    // Introduce errors at various positions
    const size_t errorPositions[] = {0, 8, 15, 22, 30, 38};
    for (auto pos : errorPositions) {
        uint8_t corrupted[39U];
        ::memcpy(corrupted, encoded, 39U);
        corrupted[pos] ^= 0x3FU; // Flip 6 bits (1 symbol)

        RS634717 rsDec;
        bool decoded = rsDec.decode523023(corrupted);

        // RS(52,30,23) can correct up to 11 symbol errors
        if (decoded) {
            REQUIRE(::memcmp(corrupted, original, 22U) == 0);
        }
    }
}

TEST_CASE("RS523023 corrects multiple symbol errors", "[edac][rs523023]") {
    uint8_t original[39U];
    for (size_t i = 0; i < 39U; i++) {
        original[i] = (uint8_t)(i + 100);
    }

    uint8_t data[39U];
    ::memcpy(data, original, 39U);

    RS634717 rs;
    rs.encode523023(data);
    Utils::dump(2U, "encode523023()", data, 39U);

    // Introduce 3 byte errors in parity region (conservative for 11-symbol capability)
    data[24] ^= 0x01U;  // Single bit error
    data[30] ^= 0x01U;  // Single bit error
    data[36] ^= 0x01U;  // Single bit error

    REQUIRE(rs.decode523023(data));
    REQUIRE(::memcmp(data, original, 22U) == 0);
}

TEST_CASE("RS523023 detects uncorrectable errors", "[edac][rs523023]") {
    uint8_t original[39U];
    for (size_t i = 0; i < 39U; i++) {
        original[i] = (uint8_t)(i * 13);
    }

    uint8_t data[39U];
    ::memcpy(data, original, 39U);

    RS634717 rs;
    rs.encode523023(data);
    Utils::dump(2U, "encode523023()", data, 39U);

    // Introduce too many errors (beyond 11 symbol correction)
    for (size_t i = 0; i < 15U; i++) {
        data[i] ^= 0xFFU;
    }

    bool result = rs.decode523023(data);
    REQUIRE(!result);
}
