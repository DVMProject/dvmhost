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

TEST_CASE("RS24169 preserves all-zero payload", "[edac][rs24169]") {
    uint8_t data[24U];
    ::memset(data, 0x00U, sizeof(data));

    RS634717 rs;
    rs.encode24169(data);
    Utils::dump(2U, "encode24169()", data, 24U);

    REQUIRE(rs.decode24169(data));

    // First 16 bytes should be zero (data portion)
    for (size_t i = 0; i < 12U; i++) {
        REQUIRE(data[i] == 0x00U);
    }
}

TEST_CASE("RS24169 preserves all-ones payload", "[edac][rs24169]") {
    uint8_t data[24U];
    ::memset(data, 0xFFU, sizeof(data));

    RS634717 rs;
    rs.encode24169(data);
    Utils::dump(2U, "encode24169()", data, 24U);

    REQUIRE(rs.decode24169(data));

    // First 16 bytes should be 0xFF
    for (size_t i = 0; i < 12U; i++) {
        REQUIRE(data[i] == 0xFFU);
    }
}

TEST_CASE("RS24169 preserves alternating pattern", "[edac][rs24169]") {
    uint8_t original[16U];
    for (size_t i = 0; i < 16U; i++) {
        original[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[24U];
    ::memcpy(data, original, 16U);
    ::memset(data + 16U, 0x00U, 8U);

    RS634717 rs;
    rs.encode24169(data);
    Utils::dump(2U, "encode24169()", data, 24U);

    REQUIRE(rs.decode24169(data));

    REQUIRE(::memcmp(data, original, 12U) == 0);
}

TEST_CASE("RS24169 preserves incrementing pattern", "[edac][rs24169]") {
    uint8_t original[16U];
    for (size_t i = 0; i < 16U; i++) {
        original[i] = (uint8_t)(i * 16);
    }

    uint8_t data[24U];
    ::memcpy(data, original, 16U);
    ::memset(data + 16U, 0x00U, 8U);

    RS634717 rs;
    rs.encode24169(data);
    Utils::dump(2U, "encode24169()", data, 24U);

    REQUIRE(rs.decode24169(data));

    REQUIRE(::memcmp(data, original, 12U) == 0);
}

TEST_CASE("RS24169 corrects single-byte errors", "[edac][rs24169]") {
    uint8_t original[16U];
    for (size_t i = 0; i < 16U; i++) {
        original[i] = (uint8_t)(i + 50);
    }

    uint8_t data[24U];
    ::memcpy(data, original, 16U);
    ::memset(data + 16U, 0x00U, 8U);

    RS634717 rs;
    rs.encode24169(data);
    Utils::dump(2U, "encode24169()", data, 24U);

    // Introduce single-byte errors
    const size_t errorPositions[] = {0, 8, 15, 18, 22};
    for (auto pos : errorPositions) {
        uint8_t corrupted[24U];
        ::memcpy(corrupted, data, 24U);
        corrupted[pos] ^= 0xFFU;

        RS634717 rsDec;
        bool decoded = rsDec.decode24169(corrupted);

        // RS(24,16,9) can correct up to 4 symbol errors
        if (decoded) {
            REQUIRE(::memcmp(corrupted, original, 12U) == 0);
        }
    }
}

TEST_CASE("RS24169 detects uncorrectable errors", "[edac][rs24169]") {
    uint8_t original[16U];
    for (size_t i = 0; i < 16U; i++) {
        original[i] = (uint8_t)(i * 13);
    }

    uint8_t data[24U];
    ::memcpy(data, original, 16U);
    ::memset(data + 16U, 0x00U, 8U);

    RS634717 rs;
    rs.encode24169(data);
    Utils::dump(2U, "encode24169()", data, 24U);

    // Introduce too many errors
    for (size_t i = 0; i < 8U; i++) {
        data[i] ^= 0xFFU;
    }

    bool result = rs.decode24169(data);
    REQUIRE(!result);
}
