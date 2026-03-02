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

#include "common/edac/RS129.h"

using namespace edac;

TEST_CASE("RS129 generates valid parity for all-zero data", "[edac][rs129]") {
    uint8_t data[12U];
    ::memset(data, 0x00U, 9U);  // 9 bytes of message data

    uint8_t parity[4U];
    RS129::encode(data, 9U, parity);

    // Store parity in data buffer (reversed order as per implementation)
    data[9U] = parity[2U];
    data[10U] = parity[1U];
    data[11U] = parity[0U];

    Utils::dump(2U, "RS129::encode() all zeros", data, 12U);

    // Verify parity check passes
    bool valid = RS129::check(data);
    REQUIRE(valid);
}

TEST_CASE("RS129 generates valid parity for all-ones data", "[edac][rs129]") {
    uint8_t data[12U];
    ::memset(data, 0xFFU, 9U);  // 9 bytes of message data

    uint8_t parity[4U];
    RS129::encode(data, 9U, parity);

    // Store parity in data buffer
    data[9U] = parity[2U];
    data[10U] = parity[1U];
    data[11U] = parity[0U];

    Utils::dump(2U, "RS129::encode() all ones", data, 12U);

    // Verify parity check passes
    bool valid = RS129::check(data);
    REQUIRE(valid);
}

TEST_CASE("RS129 generates valid parity for alternating pattern", "[edac][rs129]") {
    uint8_t data[12U];
    for (size_t i = 0; i < 9U; i++) {
        data[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t parity[4U];
    RS129::encode(data, 9U, parity);

    data[9U] = parity[2U];
    data[10U] = parity[1U];
    data[11U] = parity[0U];

    Utils::dump(2U, "RS129::encode() alternating", data, 12U);

    bool valid = RS129::check(data);
    REQUIRE(valid);
}

TEST_CASE("RS129 generates valid parity for incrementing pattern", "[edac][rs129]") {
    uint8_t data[12U];
    for (size_t i = 0; i < 9U; i++) {
        data[i] = (uint8_t)(i * 13);
    }

    uint8_t parity[4U];
    RS129::encode(data, 9U, parity);

    data[9U] = parity[2U];
    data[10U] = parity[1U];
    data[11U] = parity[0U];

    Utils::dump(2U, "RS129::encode() incrementing", data, 12U);

    bool valid = RS129::check(data);
    REQUIRE(valid);
}

TEST_CASE("RS129 handles various test patterns", "[edac][rs129]") {
    const uint8_t testPatterns[][9] = {
        {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U},
        {0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU},
        {0x0FU, 0xF0U, 0x0FU, 0xF0U, 0x0FU, 0xF0U, 0x0FU, 0xF0U, 0x0FU},
        {0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU, 0xDEU, 0xF0U, 0x11U},
        {0xA5U, 0x5AU, 0xA5U, 0x5AU, 0xA5U, 0x5AU, 0xA5U, 0x5AU, 0xA5U}
    };

    for (auto& pattern : testPatterns) {
        uint8_t data[12U];
        ::memcpy(data, pattern, 9U);

        uint8_t parity[4U];
        RS129::encode(data, 9U, parity);

        data[9U] = parity[2U];
        data[10U] = parity[1U];
        data[11U] = parity[0U];

        bool valid = RS129::check(data);
        REQUIRE(valid);
    }
}

TEST_CASE("RS129 detects single-byte errors", "[edac][rs129]") {
    uint8_t data[12U];
    for (size_t i = 0; i < 9U; i++) {
        data[i] = (uint8_t)(i + 50);
    }

    uint8_t parity[4U];
    RS129::encode(data, 9U, parity);

    data[9U] = parity[2U];
    data[10U] = parity[1U];
    data[11U] = parity[0U];

    // Save original data
    uint8_t original[12U];
    ::memcpy(original, data, 12U);

    // Test single-byte errors in message portion
    for (size_t pos = 0; pos < 9U; pos++) {
        ::memcpy(data, original, 12U);
        
        // Introduce single-byte error
        data[pos] ^= 0x55U;

        bool valid = RS129::check(data);
        
        // RS(12,9) should detect single-byte errors
        REQUIRE_FALSE(valid);
    }
}

TEST_CASE("RS129 detects errors in parity bytes", "[edac][rs129]") {
    uint8_t data[12U];
    for (size_t i = 0; i < 9U; i++) {
        data[i] = (uint8_t)(i * 7);
    }

    uint8_t parity[4U];
    RS129::encode(data, 9U, parity);

    data[9U] = parity[2U];
    data[10U] = parity[1U];
    data[11U] = parity[0U];

    // Save original data
    uint8_t original[12U];
    ::memcpy(original, data, 12U);

    // Test errors in parity bytes
    for (size_t pos = 9; pos < 12U; pos++) {
        ::memcpy(data, original, 12U);
        
        // Introduce error in parity byte
        data[pos] ^= 0xAAU;

        bool valid = RS129::check(data);
        
        // Should detect parity byte corruption
        REQUIRE_FALSE(valid);
    }
}

TEST_CASE("RS129 handles random payloads", "[edac][rs129]") {
    // Test with various pseudo-random patterns
    for (uint32_t test = 0; test < 10; test++) {
        uint8_t data[12U];
        for (size_t i = 0; i < 9U; i++) {
            data[i] = (uint8_t)((i * 37 + test * 53) % 256);
        }

        uint8_t parity[4U];
        RS129::encode(data, 9U, parity);

        data[9U] = parity[2U];
        data[10U] = parity[1U];
        data[11U] = parity[0U];

        bool valid = RS129::check(data);
        REQUIRE(valid);
    }
}

TEST_CASE("RS129 handles sequential data", "[edac][rs129]") {
    uint8_t data[12U];
    for (size_t i = 0; i < 9U; i++) {
        data[i] = (uint8_t)i;
    }

    uint8_t parity[4U];
    RS129::encode(data, 9U, parity);

    data[9U] = parity[2U];
    data[10U] = parity[1U];
    data[11U] = parity[0U];

    bool valid = RS129::check(data);
    REQUIRE(valid);
}

TEST_CASE("RS129 parity generation is deterministic", "[edac][rs129]") {
    uint8_t data[9U] = {0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU, 0xDEU, 0xF0U, 0xABU};

    uint8_t parity1[4U];
    RS129::encode(data, 9U, parity1);

    uint8_t parity2[4U];
    RS129::encode(data, 9U, parity2);

    // Same input should always produce same parity
    REQUIRE(::memcmp(parity1, parity2, 3U) == 0);
}
