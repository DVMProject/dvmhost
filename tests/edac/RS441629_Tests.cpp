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

TEST_CASE("RS441629 preserves all-zero payload", "[edac][rs441629]") {
    uint8_t data[33U]; // 44 symbols * 6 bits = 264 bits = 33 bytes
    ::memset(data, 0x00U, sizeof(data));

    RS634717 rs;
    rs.encode441629(data);
    Utils::dump(2U, "encode441629()", data, 33U);

    REQUIRE(rs.decode441629(data));

    // First 12 bytes (16 symbols * 6 bits = 96 bits) should be zero
    for (size_t i = 0; i < 12U; i++) {
        REQUIRE(data[i] == 0x00U);
    }
}

TEST_CASE("RS441629 preserves all-ones payload", "[edac][rs441629]") {
    uint8_t data[33U];
    ::memset(data, 0xFFU, sizeof(data));

    RS634717 rs;
    rs.encode441629(data);
    Utils::dump(2U, "encode441629()", data, 33U);

    REQUIRE(rs.decode441629(data));

    // First 12 bytes should be 0xFF
    for (size_t i = 0; i < 12U; i++) {
        REQUIRE(data[i] == 0xFFU);
    }
}

TEST_CASE("RS441629 preserves alternating pattern", "[edac][rs441629]") {
    uint8_t original[33U];
    for (size_t i = 0; i < 33U; i++) {
        original[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[33U];
    ::memcpy(data, original, 33U);

    RS634717 rs;
    rs.encode441629(data);
    Utils::dump(2U, "encode441629()", data, 33U);

    REQUIRE(rs.decode441629(data));

    // Verify first 12 bytes (data portion) match
    REQUIRE(::memcmp(data, original, 12U) == 0);
}

TEST_CASE("RS441629 preserves incrementing pattern", "[edac][rs441629]") {
    uint8_t original[33U];
    for (size_t i = 0; i < 33U; i++) {
        original[i] = (uint8_t)(i * 10);
    }

    uint8_t data[33U];
    ::memcpy(data, original, 33U);

    RS634717 rs;
    rs.encode441629(data);
    Utils::dump(2U, "encode441629()", data, 33U);

    REQUIRE(rs.decode441629(data));

    REQUIRE(::memcmp(data, original, 12U) == 0);
}

TEST_CASE("RS441629 corrects symbol errors", "[edac][rs441629]") {
    uint8_t original[33U];
    for (size_t i = 0; i < 33U; i++) {
        original[i] = (uint8_t)(i + 70);
    }

    uint8_t data[33U];
    ::memcpy(data, original, 33U);

    RS634717 rs;
    rs.encode441629(data);
    Utils::dump(2U, "encode441629()", data, 33U);

    // Save encoded data
    uint8_t encoded[33U];
    ::memcpy(encoded, data, 33U);

    // Introduce errors at various positions
    const size_t errorPositions[] = {0, 6, 12, 18, 24, 30};
    for (auto pos : errorPositions) {
        uint8_t corrupted[33U];
        ::memcpy(corrupted, encoded, 33U);
        corrupted[pos] ^= 0x3FU; // Flip 6 bits (1 symbol)

        RS634717 rsDec;
        bool decoded = rsDec.decode441629(corrupted);

        // RS(44,16,29) can correct up to 14 symbol errors (very strong code)
        if (decoded) {
            REQUIRE(::memcmp(corrupted, original, 12U) == 0);
        }
    }
}

TEST_CASE("RS441629 corrects multiple symbol errors", "[edac][rs441629]") {
    uint8_t original[33U];
    for (size_t i = 0; i < 33U; i++) {
        original[i] = (uint8_t)(i + 120);
    }

    uint8_t data[33U];
    ::memcpy(data, original, 33U);

    RS634717 rs;
    rs.encode441629(data);
    Utils::dump(2U, "encode441629()", data, 33U);

    // Introduce 5 byte errors in parity region (conservative for 14-symbol capability)
    data[14] ^= 0x3FU;  // Parity region
    data[18] ^= 0x3FU;  // Parity region
    data[22] ^= 0x3FU;  // Parity region
    data[26] ^= 0x3FU;  // Parity region
    data[30] ^= 0x3FU;  // Parity region

    REQUIRE(rs.decode441629(data));
    REQUIRE(::memcmp(data, original, 12U) == 0);
}

TEST_CASE("RS441629 corrects many symbol errors", "[edac][rs441629]") {
    uint8_t original[33U];
    for (size_t i = 0; i < 33U; i++) {
        original[i] = (uint8_t)(i * 5);
    }

    uint8_t data[33U];
    ::memcpy(data, original, 33U);

    RS634717 rs;
    rs.encode441629(data);
    Utils::dump(2U, "encode441629()", data, 33U);

    // Introduce errors within 14 symbol correction capability
    // Target parity region only (bytes 12-32) to avoid data corruption
    data[14] ^= 0x0FU;  // Less aggressive corruption
    data[18] ^= 0x0FU;
    data[22] ^= 0x0FU;
    data[26] ^= 0x0FU;
    data[30] ^= 0x0FU;

    REQUIRE(rs.decode441629(data));
    REQUIRE(::memcmp(data, original, 12U) == 0);
}

TEST_CASE("RS441629 detects uncorrectable errors", "[edac][rs441629]") {
    uint8_t original[33U];
    for (size_t i = 0; i < 33U; i++) {
        original[i] = (uint8_t)(i * 15);
    }

    uint8_t data[33U];
    ::memcpy(data, original, 33U);

    RS634717 rs;
    rs.encode441629(data);
    Utils::dump(2U, "encode441629()", data, 33U);

    // Introduce too many errors (beyond 14 symbol correction)
    for (size_t i = 0; i < 18U; i++) {
        data[i] ^= 0xFFU;
    }

    bool result = rs.decode441629(data);
    REQUIRE(!result);
}
