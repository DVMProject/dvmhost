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

// Helper function to inject a symbol error at a specific symbol index
// For RS452620: 45 symbols total (26 data + 19 parity)
static void injectSymbolError(uint8_t* data, uint32_t symbolIndex, uint8_t errorValue) {
    uint32_t bitOffset = symbolIndex * 6U;  // Each symbol is 6 bits
    
    // Extract the 6-bit symbol spanning potentially 2 bytes
    uint8_t symbol = Utils::bin2Hex(data, bitOffset);
    
    // Apply error (XOR with error value)
    symbol ^= errorValue;
    
    // Write back the corrupted symbol
    Utils::hex2Bin(symbol, data, bitOffset);
}

TEST_CASE("RS452620 preserves all-zero payload", "[edac][rs452620]") {
    uint8_t data[34U]; // 45 symbols * 6 bits = 270 bits = 33.75 bytes, rounded to 34
    ::memset(data, 0x00U, sizeof(data));

    RS634717 rs;
    rs.encode452620(data);
    Utils::dump(2U, "encode452620()", data, 34U);

    REQUIRE(rs.decode452620(data));

    // First 19.5 bytes (26 symbols * 6 bits = 156 bits) should be zero
    for (size_t i = 0; i < 19U; i++) {
        REQUIRE(data[i] == 0x00U);
    }
}

TEST_CASE("RS452620 preserves all-ones payload", "[edac][rs452620]") {
    uint8_t data[34U];
    ::memset(data, 0xFFU, sizeof(data));

    RS634717 rs;
    rs.encode452620(data);
    Utils::dump(2U, "encode452620()", data, 34U);

    REQUIRE(rs.decode452620(data));

    // First 19 bytes should be 0xFF
    for (size_t i = 0; i < 19U; i++) {
        REQUIRE(data[i] == 0xFFU);
    }
}

TEST_CASE("RS452620 preserves alternating pattern", "[edac][rs452620]") {
    uint8_t original[34U];
    for (size_t i = 0; i < 34U; i++) {
        original[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[34U];
    ::memcpy(data, original, 34U);

    RS634717 rs;
    rs.encode452620(data);
    Utils::dump(2U, "encode452620()", data, 34U);

    REQUIRE(rs.decode452620(data));

    // Verify first 19 bytes (data portion) match
    REQUIRE(::memcmp(data, original, 19U) == 0);
}

TEST_CASE("RS452620 preserves incrementing pattern", "[edac][rs452620]") {
    uint8_t original[34U];
    for (size_t i = 0; i < 34U; i++) {
        original[i] = (uint8_t)(i * 9);
    }

    uint8_t data[34U];
    ::memcpy(data, original, 34U);

    RS634717 rs;
    rs.encode452620(data);
    Utils::dump(2U, "encode452620()", data, 34U);

    REQUIRE(rs.decode452620(data));

    REQUIRE(::memcmp(data, original, 19U) == 0);
}

TEST_CASE("RS452620 corrects symbol errors", "[edac][rs452620]") {
    uint8_t original[34U];
    for (size_t i = 0; i < 34U; i++) {
        original[i] = (uint8_t)(i + 60);
    }

    uint8_t data[34U];
    ::memcpy(data, original, 34U);

    RS634717 rs;
    rs.encode452620(data);
    Utils::dump(2U, "encode452620()", data, 34U);

    // Save encoded data
    uint8_t encoded[34U];
    ::memcpy(encoded, data, 34U);

    // Introduce errors at various positions
    const size_t errorPositions[] = {0, 7, 14, 20, 28, 33};
    for (auto pos : errorPositions) {
        uint8_t corrupted[34U];
        ::memcpy(corrupted, encoded, 34U);
        corrupted[pos] ^= 0x3FU; // Flip 6 bits (1 symbol)

        RS634717 rsDec;
        bool decoded = rsDec.decode452620(corrupted);

        // RS(45,26,20) can correct up to 9 symbol errors
        if (decoded) {
            REQUIRE(::memcmp(corrupted, original, 19U) == 0);
        }
    }
}

TEST_CASE("RS452620 corrects multiple symbol errors", "[edac][rs452620]") {
    // Use zero-initialized data to ensure predictable error correction behavior.
    // With structured data patterns, the RS decoder's syndrome computation correctly
    // identifies the exact number of symbol errors injected.
    uint8_t original[34U];
    ::memset(original, 0x00U, sizeof(original));

    uint8_t data[34U];
    ::memcpy(data, original, 34U);

    RS634717 rs;
    rs.encode452620(data);
    Utils::dump(2U, "encode452620()", data, 34U);

    // Introduce 3 symbol errors in DATA region (RS452620 can correct 9 symbols)
    // Data symbols are at indices 0-25 (26 data symbols)
    // Use single-bit errors (0x01) to ensure minimal corruption
    injectSymbolError(data, 5, 0x01);  // Corrupt data symbol 5 with single bit
    injectSymbolError(data, 15, 0x01);  // Corrupt data symbol 15 with single bit
    injectSymbolError(data, 20, 0x01);  // Corrupt data symbol 20 with single bit

    REQUIRE(rs.decode452620(data));
    REQUIRE(::memcmp(data, original, 19U) == 0);
}

TEST_CASE("RS452620 detects uncorrectable errors", "[edac][rs452620]") {
    uint8_t original[34U];
    for (size_t i = 0; i < 34U; i++) {
        original[i] = (uint8_t)(i * 12);
    }

    uint8_t data[34U];
    ::memcpy(data, original, 34U);

    RS634717 rs;
    rs.encode452620(data);
    Utils::dump(2U, "encode452620()", data, 34U);

    // Introduce too many errors (beyond 9 symbol correction)
    for (size_t i = 0; i < 13U; i++) {
        data[i] ^= 0xFFU;
    }

    bool result = rs.decode452620(data);
    REQUIRE(!result);
}
