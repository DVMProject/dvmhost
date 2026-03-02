// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "common/edac/BPTC19696.h"

using namespace edac;

TEST_CASE("BPTC19696 preserves all-zero payload", "[dmr][bptc19696]") {
    uint8_t input[12U];
    ::memset(input, 0x00U, sizeof(input));

    uint8_t encoded[196U];
    BPTC19696 bptc;
    bptc.encode(input, encoded);

    uint8_t decoded[12U];
    bptc.decode(encoded, decoded);

    REQUIRE(::memcmp(input, decoded, 12U) == 0);
}

TEST_CASE("BPTC19696 preserves all-ones payload", "[dmr][bptc19696]") {
    uint8_t input[12U];
    ::memset(input, 0xFFU, sizeof(input));

    uint8_t encoded[196U];
    BPTC19696 bptc;
    bptc.encode(input, encoded);

    uint8_t decoded[12U];
    bptc.decode(encoded, decoded);

    REQUIRE(::memcmp(input, decoded, 12U) == 0);
}

TEST_CASE("BPTC19696 preserves alternating bit pattern", "[dmr][bptc19696]") {
    uint8_t input[12U];
    for (size_t i = 0; i < 12U; i++) {
        input[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t encoded[196U];
    BPTC19696 bptc;
    bptc.encode(input, encoded);

    uint8_t decoded[12U];
    bptc.decode(encoded, decoded);

    REQUIRE(::memcmp(input, decoded, 12U) == 0);
}

TEST_CASE("BPTC19696 preserves incrementing pattern", "[dmr][bptc19696]") {
    uint8_t input[12U];
    for (size_t i = 0; i < 12U; i++) {
        input[i] = (uint8_t)(i * 17); // Spread values across byte range
    }

    uint8_t encoded[196U];
    BPTC19696 bptc;
    bptc.encode(input, encoded);

    uint8_t decoded[12U];
    bptc.decode(encoded, decoded);

    REQUIRE(::memcmp(input, decoded, 12U) == 0);
}

TEST_CASE("BPTC19696 corrects single-bit errors", "[dmr][bptc19696]") {
    uint8_t input[12U];
    for (size_t i = 0; i < 12U; i++) {
        input[i] = 0x42U; // Specific pattern
    }

    uint8_t encoded[196U];
    BPTC19696 bptc;
    bptc.encode(input, encoded);

    // Introduce single-bit error in various positions
    const size_t errorPositions[] = {10, 50, 100, 150, 190};
    for (auto pos : errorPositions) {
        uint8_t corrupted[196U];
        ::memcpy(corrupted, encoded, 196U);
        corrupted[pos] ^= 1U; // Flip one bit

        uint8_t decoded[12U];
        BPTC19696 bptcDec;
        bptcDec.decode(corrupted, decoded);

        // Should still match original (or be close - FEC corrects single errors)
        // Note: This assumes BPTC can correct single-bit errors
        REQUIRE(::memcmp(input, decoded, 12U) == 0);
    }
}
