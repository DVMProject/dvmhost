// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "common/Log.h"
#include "common/Utils.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "common/edac/Golay2087.h"

using namespace edac;

TEST_CASE("Golay2087 preserves all-zero data", "[edac][golay2087]") {
    uint8_t data[3U];
    ::memset(data, 0x00U, sizeof(data));

    Golay2087::encode(data);
    uint8_t decoded = Golay2087::decode(data);

    REQUIRE(decoded == 0x00U);
}

TEST_CASE("Golay2087 preserves all-ones data", "[edac][golay2087]") {
    uint8_t data[3U];
    data[0] = 0xFFU;
    data[1] = 0xF0U;  // Upper 4 bits are data, lower 12 bits are parity
    data[2] = 0x00U;

    Golay2087::encode(data);
    Utils::dump(2U, "Golay2087::encode()", data, 3U);

    uint8_t decoded = Golay2087::decode(data);

    REQUIRE(decoded == 0xFFU);
}

TEST_CASE("Golay2087 encodes and decodes specific patterns", "[edac][golay2087]") {
    const uint8_t testValues[] = {0x00U, 0x55U, 0xAAU, 0x0FU, 0xF0U, 0x33U, 0xCCU, 0x5AU, 0xA5U};

    for (auto value : testValues) {
        uint8_t data[3U];
        ::memset(data, 0x00U, sizeof(data));
        
        // Set the 8-bit data value
        data[0] = value;

        Golay2087::encode(data);
        uint8_t decoded = Golay2087::decode(data);

        REQUIRE(decoded == value);
    }
}

TEST_CASE("Golay2087 corrects single-bit errors", "[edac][golay2087]") {
    uint8_t original = 0xA5U;
    uint8_t data[3U];
    ::memset(data, 0x00U, sizeof(data));
    data[0] = original;

    Golay2087::encode(data);

    // Save encoded data
    uint8_t encoded[3U];
    ::memcpy(encoded, data, 3U);

    // Test single-bit errors in different positions
    for (uint32_t bit = 0U; bit < 20U; bit++) {
        ::memcpy(data, encoded, 3U);
        
        // Inject single-bit error
        uint32_t bytePos = bit / 8U;
        uint32_t bitPos = bit % 8U;
        data[bytePos] ^= (1U << (7U - bitPos));

        uint8_t decoded = Golay2087::decode(data);
        REQUIRE(decoded == original);
    }
}

TEST_CASE("Golay2087 corrects two-bit errors", "[edac][golay2087]") {
    uint8_t original = 0x3CU;
    uint8_t data[3U];
    ::memset(data, 0x00U, sizeof(data));
    data[0] = original;

    Golay2087::encode(data);

    // Save encoded data
    uint8_t encoded[3U];
    ::memcpy(encoded, data, 3U);

    // Test two-bit error patterns
    const uint32_t errorPairs[][2] = {
        {0, 5}, {1, 8}, {3, 12}, {7, 15}, {10, 18}
    };

    for (auto& pair : errorPairs) {
        ::memcpy(data, encoded, 3U);
        
        // Inject two-bit errors
        for (uint32_t i = 0; i < 2; i++) {
            uint32_t bit = pair[i];
            uint32_t bytePos = bit / 8U;
            uint32_t bitPos = bit % 8U;
            data[bytePos] ^= (1U << (7U - bitPos));
        }

        uint8_t decoded = Golay2087::decode(data);
        REQUIRE(decoded == original);
    }
}

TEST_CASE("Golay2087 corrects three-bit errors", "[edac][golay2087]") {
    uint8_t original = 0x7EU;
    uint8_t data[3U];
    ::memset(data, 0x00U, sizeof(data));
    data[0] = original;

    Golay2087::encode(data);

    // Save encoded data
    uint8_t encoded[3U];
    ::memcpy(encoded, data, 3U);

    // Test three-bit error patterns (Golay(20,8,7) can correct up to 3 errors)
    const uint32_t errorTriples[][3] = {
        {0, 5, 10}, {2, 8, 14}, {4, 11, 17}
    };

    for (auto& triple : errorTriples) {
        ::memcpy(data, encoded, 3U);
        
        // Inject three-bit errors
        for (uint32_t i = 0; i < 3; i++) {
            uint32_t bit = triple[i];
            uint32_t bytePos = bit / 8U;
            uint32_t bitPos = bit % 8U;
            data[bytePos] ^= (1U << (7U - bitPos));
        }

        uint8_t decoded = Golay2087::decode(data);
        REQUIRE(decoded == original);
    }
}

TEST_CASE("Golay2087 handles incrementing pattern", "[edac][golay2087]") {
    for (uint32_t i = 0; i < 256; i++) {
        uint8_t original = (uint8_t)i;
        uint8_t data[3U];
        ::memset(data, 0x00U, sizeof(data));
        data[0] = original;

        Golay2087::encode(data);
        uint8_t decoded = Golay2087::decode(data);

        REQUIRE(decoded == original);
    }
}
