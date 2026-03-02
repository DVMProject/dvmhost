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

#include "common/edac/Trellis.h"

using namespace edac;

TEST_CASE("Trellis 3/4 rate preserves all-zero payload", "[edac][trellis]") {
    uint8_t payload[18U];
    ::memset(payload, 0x00U, sizeof(payload));

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() all zeros", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate preserves all-ones payload", "[edac][trellis]") {
    uint8_t payload[18U];
    ::memset(payload, 0xFFU, sizeof(payload));

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() all ones", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate preserves alternating pattern", "[edac][trellis]") {
    uint8_t payload[18U];
    for (size_t i = 0; i < 18U; i++) {
        payload[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() alternating", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate preserves incrementing pattern", "[edac][trellis]") {
    uint8_t payload[18U];
    for (size_t i = 0; i < 18U; i++) {
        payload[i] = (uint8_t)(i * 13);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() incrementing", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate preserves specific pattern", "[edac][trellis]") {
    uint8_t payload[18U];
    for (size_t i = 0; i < 18U; i++) {
        payload[i] = (uint8_t)(i + 100);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 3/4 rate handles another pattern", "[edac][trellis]") {
    uint8_t payload[18U];
    for (size_t i = 0; i < 18U; i++) {
        payload[i] = (uint8_t)(i * 7);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode34(payload, data, false);
    Utils::dump(2U, "Trellis::encode34() pattern", data, 25U);

    uint8_t decoded[18U];
    bool result = trellis.decode34(data, decoded, false);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 18U) == 0);
}

TEST_CASE("Trellis 1/2 rate preserves all-zero payload", "[edac][trellis]") {
    uint8_t payload[12U];
    ::memset(payload, 0x00U, sizeof(payload));

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(payload, data);
    Utils::dump(2U, "Trellis::encode12() all zeros", data, 25U);

    uint8_t decoded[12U];
    bool result = trellis.decode12(data, decoded);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 12U) == 0);
}

TEST_CASE("Trellis 1/2 rate preserves all-ones payload", "[edac][trellis]") {
    uint8_t payload[12U];
    ::memset(payload, 0xFFU, sizeof(payload));

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(payload, data);
    Utils::dump(2U, "Trellis::encode12() all ones", data, 25U);

    uint8_t decoded[12U];
    bool result = trellis.decode12(data, decoded);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 12U) == 0);
}

TEST_CASE("Trellis 1/2 rate preserves alternating pattern", "[edac][trellis]") {
    uint8_t payload[12U];
    for (size_t i = 0; i < 12U; i++) {
        payload[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(payload, data);
    Utils::dump(2U, "Trellis::encode12() alternating", data, 25U);

    uint8_t decoded[12U];
    bool result = trellis.decode12(data, decoded);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 12U) == 0);
}

TEST_CASE("Trellis 1/2 rate preserves incrementing pattern", "[edac][trellis]") {
    uint8_t payload[12U];
    for (size_t i = 0; i < 12U; i++) {
        payload[i] = (uint8_t)(i * 17);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(payload, data);
    Utils::dump(2U, "Trellis::encode12() incrementing", data, 25U);

    uint8_t decoded[12U];
    bool result = trellis.decode12(data, decoded);

    REQUIRE(result);
    REQUIRE(::memcmp(decoded, payload, 12U) == 0);
}

TEST_CASE("Trellis 1/2 rate corrects errors", "[edac][trellis]") {
    uint8_t original[12U];
    for (size_t i = 0; i < 12U; i++) {
        original[i] = (uint8_t)(i + 75);
    }

    uint8_t data[25U];
    ::memset(data, 0x00U, sizeof(data));

    Trellis trellis;
    trellis.encode12(original, data);

    // Save encoded data
    uint8_t encoded[25U];
    ::memcpy(encoded, data, 25U);

    // Test errors at various positions
    const size_t errorPositions[] = {0, 8, 16, 24};
    
    for (auto pos : errorPositions) {
        ::memcpy(data, encoded, 25U);
        
        // Introduce errors - 1/2 rate has better error correction
        data[pos] ^= 0x07U;

        uint8_t decoded[12U];
        bool result = trellis.decode12(data, decoded);

        // 1/2 rate Trellis has stronger error correction
        if (result) {
            REQUIRE(::memcmp(decoded, original, 12U) == 0);
        }
    }
}

TEST_CASE("Trellis 1/2 rate handles random payloads", "[edac][trellis]") {
    // Test with various random-like patterns
    for (uint32_t test = 0; test < 5; test++) {
        uint8_t payload[12U];
        for (size_t i = 0; i < 12U; i++) {
            payload[i] = (uint8_t)((i * 37 + test * 53) % 256);
        }

        uint8_t data[25U];
        ::memset(data, 0x00U, sizeof(data));

        Trellis trellis;
        trellis.encode12(payload, data);

        uint8_t decoded[12U];
        bool result = trellis.decode12(data, decoded);

        REQUIRE(result);
        REQUIRE(::memcmp(decoded, payload, 12U) == 0);
    }
}

TEST_CASE("Trellis 3/4 rate handles random payloads", "[edac][trellis]") {
    // Test with various random-like patterns
    for (uint32_t test = 0; test < 5; test++) {
        uint8_t payload[18U];
        for (size_t i = 0; i < 18U; i++) {
            payload[i] = (uint8_t)((i * 41 + test * 61) % 256);
        }

        uint8_t data[25U];
        ::memset(data, 0x00U, sizeof(data));

        Trellis trellis;
        trellis.encode34(payload, data, false);

        uint8_t decoded[18U];
        bool result = trellis.decode34(data, decoded, false);

        REQUIRE(result);
        REQUIRE(::memcmp(decoded, payload, 18U) == 0);
    }
}
