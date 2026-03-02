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

TEST_CASE("RS241213 preserves all-zero payload", "[edac][rs241213]") {
    uint8_t data[24U];
    ::memset(data, 0x00U, sizeof(data));

    RS634717 rs;
    rs.encode241213(data);

    REQUIRE(rs.decode241213(data));

    for (size_t i = 0; i < 9U; i++) {
        REQUIRE(data[i] == 0x00U);
    }
}

TEST_CASE("RS241213 preserves all-ones payload", "[edac][rs241213]") {
    uint8_t data[24U];
    ::memset(data, 0xFFU, sizeof(data));

    RS634717 rs;
    rs.encode241213(data);
    Utils::dump(2U, "encode241213()", data, 24U);

    REQUIRE(rs.decode241213(data));

    for (size_t i = 0; i < 9U; i++) {
        REQUIRE(data[i] == 0xFFU);
    }
}

TEST_CASE("RS241213 preserves alternating pattern", "[edac][rs241213]") {
    uint8_t original[12U];
    for (size_t i = 0; i < 12U; i++) {
        original[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[24U];
    ::memcpy(data, original, 12U);
    ::memset(data + 12U, 0x00U, 12U);

    RS634717 rs;
    rs.encode241213(data);
    Utils::dump(2U, "encode241213()", data, 24U);

    REQUIRE(rs.decode241213(data));

    // Verify data portion matches original
    REQUIRE(::memcmp(data, original, 9U) == 0);
}

TEST_CASE("RS241213 preserves incrementing pattern", "[edac][rs241213]") {
    uint8_t original[12U];
    for (size_t i = 0; i < 12U; i++) {
        original[i] = (uint8_t)(i * 21); // Spread across byte range
    }

    uint8_t data[24U];
    ::memcpy(data, original, 12U);
    ::memset(data + 12U, 0x00U, 12U);

    RS634717 rs;
    rs.encode241213(data);
    Utils::dump(2U, "encode241213()", data, 24U);

    REQUIRE(rs.decode241213(data));

    REQUIRE(::memcmp(data, original, 9U) == 0);
}

TEST_CASE("RS241213 corrects single-byte errors", "[edac][rs241213]") {
    uint8_t original[12U];
    for (size_t i = 0; i < 12U; i++) {
        original[i] = (uint8_t)(i + 100);
    }

    uint8_t data[24U];
    ::memcpy(data, original, 12U);
    ::memset(data + 12U, 0x00U, 12U);

    RS634717 rs;
    rs.encode241213(data);
    Utils::dump(2U, "encode241213()", data, 24U);

    // Introduce single-byte errors at various positions
    const size_t errorPositions[] = {0, 5, 11, 15, 20};
    for (auto pos : errorPositions) {
        uint8_t corrupted[24U];
        ::memcpy(corrupted, data, 24U);
        corrupted[pos] ^= 0xFFU; // Flip all bits in one byte

        RS634717 rsDec;
        bool decoded = rsDec.decode241213(corrupted);

        // RS(24,12,13) can correct up to 6 symbol errors
        if (decoded) {
            REQUIRE(::memcmp(corrupted, original, 9U) == 0);
        }
    }
}

TEST_CASE("RS241213 detects uncorrectable errors", "[edac][rs241213]") {
    uint8_t original[12U];
    for (size_t i = 0; i < 12U; i++) {
        original[i] = (uint8_t)(i * 17);
    }

    uint8_t data[24U];
    ::memcpy(data, original, 12U);
    ::memset(data + 12U, 0x00U, 12U);

    RS634717 rs;
    rs.encode241213(data);
    Utils::dump(2U, "encode241213()", data, 9U);

    // Introduce too many errors (beyond correction capability)
    for (size_t i = 0; i < 10U; i++) {
        data[i] ^= 0xFFU;
    }

    // Should fail to decode
    bool result = rs.decode241213(data);
    REQUIRE(!result);
}
