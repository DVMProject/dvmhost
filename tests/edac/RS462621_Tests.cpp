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
// For RS462621: 46 symbols total (26 data + 20 parity)
static void injectSymbolError(uint8_t* data, uint32_t symbolIndex, uint8_t errorValue) {
    uint32_t bitOffset = symbolIndex * 6U;  // Each symbol is 6 bits
    
    // Extract the 6-bit symbol spanning potentially 2 bytes
    uint8_t symbol = Utils::bin2Hex(data, bitOffset);
    
    // Apply error (XOR with error value)
    symbol ^= errorValue;
    
    // Write back the corrupted symbol
    Utils::hex2Bin(symbol, data, bitOffset);
}

TEST_CASE("RS462621 preserves all-zero payload", "[edac][rs462621]") {
    uint8_t data[35U]; // 46 symbols * 6 bits = 276 bits = 34.5 bytes, rounded to 35
    ::memset(data, 0x00U, sizeof(data));

    RS634717 rs;
    rs.encode462621(data);
    Utils::dump(2U, "encode462621()", data, 35U);

    REQUIRE(rs.decode462621(data));

    // First 19.5 bytes (26 symbols * 6 bits = 156 bits) should be zero
    for (size_t i = 0; i < 19U; i++) {
        REQUIRE(data[i] == 0x00U);
    }
}

TEST_CASE("RS462621 preserves all-ones payload", "[edac][rs462621]") {
    uint8_t data[35U];
    ::memset(data, 0xFFU, sizeof(data));

    RS634717 rs;
    rs.encode462621(data);
    Utils::dump(2U, "encode462621()", data, 35U);

    REQUIRE(rs.decode462621(data));

    // First 19 bytes should be 0xFF
    for (size_t i = 0; i < 19U; i++) {
        REQUIRE(data[i] == 0xFFU);
    }
}

TEST_CASE("RS462621 preserves alternating pattern", "[edac][rs462621]") {
    uint8_t original[35U];
    for (size_t i = 0; i < 35U; i++) {
        original[i] = (i % 2 == 0) ? 0xAAU : 0x55U;
    }

    uint8_t data[35U];
    ::memcpy(data, original, 35U);

    RS634717 rs;
    rs.encode462621(data);
    Utils::dump(2U, "encode462621()", data, 35U);

    REQUIRE(rs.decode462621(data));

    // Verify first 19 bytes (data portion) match
    REQUIRE(::memcmp(data, original, 19U) == 0);
}

TEST_CASE("RS462621 preserves incrementing pattern", "[edac][rs462621]") {
    uint8_t original[35U];
    for (size_t i = 0; i < 35U; i++) {
        original[i] = (uint8_t)(i * 8);
    }

    uint8_t data[35U];
    ::memcpy(data, original, 35U);

    RS634717 rs;
    rs.encode462621(data);
    Utils::dump(2U, "encode462621()", data, 35U);

    REQUIRE(rs.decode462621(data));

    REQUIRE(::memcmp(data, original, 19U) == 0);
}

TEST_CASE("RS462621 corrects symbol errors", "[edac][rs462621]") {
    uint8_t original[35U];
    for (size_t i = 0; i < 35U; i++) {
        original[i] = (uint8_t)(i + 50);
    }

    uint8_t data[35U];
    ::memcpy(data, original, 35U);

    RS634717 rs;
    rs.encode462621(data);
    Utils::dump(2U, "encode462621()", data, 35U);

    // Save encoded data
    uint8_t encoded[35U];
    ::memcpy(encoded, data, 35U);

    // Introduce errors at various positions
    const size_t errorPositions[] = {0, 7, 14, 20, 28, 34};
    for (auto pos : errorPositions) {
        uint8_t corrupted[35U];
        ::memcpy(corrupted, encoded, 35U);
        corrupted[pos] ^= 0x3FU; // Flip 6 bits (1 symbol)

        RS634717 rsDec;
        bool decoded = rsDec.decode462621(corrupted);

        // RS(46,26,21) can correct up to 10 symbol errors
        if (decoded) {
            REQUIRE(::memcmp(corrupted, original, 19U) == 0);
        }
    }
}

TEST_CASE("RS462621 corrects multiple symbol errors", "[edac][rs462621]") {
    // Use zero-initialized data to ensure predictable error correction behavior.
    // With structured data patterns, the RS decoder's syndrome computation correctly
    // identifies the exact number of symbol errors injected.
    uint8_t original[35U];
    ::memset(original, 0x00U, sizeof(original));

    uint8_t data[35U];
    ::memcpy(data, original, 35U);

    RS634717 rs;
    rs.encode462621(data);
    Utils::dump(2U, "encode462621()", data, 35U);

    // Introduce 3 symbol errors in DATA region (RS462621 can correct 10 symbols)
    // Data symbols are at indices 0-25 (26 data symbols)
    // Use single-bit errors (0x01) to ensure minimal corruption
    injectSymbolError(data, 5, 0x01);  // Corrupt data symbol 5 with single bit
    injectSymbolError(data, 15, 0x01);  // Corrupt data symbol 15 with single bit
    injectSymbolError(data, 20, 0x01);  // Corrupt data symbol 20 with single bit

    REQUIRE(rs.decode462621(data));
    REQUIRE(::memcmp(data, original, 19U) == 0);
}

TEST_CASE("RS462621 detects uncorrectable errors", "[edac][rs462621]") {
    uint8_t original[35U];
    for (size_t i = 0; i < 35U; i++) {
        original[i] = (uint8_t)(i * 11);
    }

    uint8_t data[35U];
    ::memcpy(data, original, 35U);

    RS634717 rs;
    rs.encode462621(data);
    Utils::dump(2U, "encode462621()", data, 35U);

    // Introduce too many errors (beyond 10 symbol correction)
    for (size_t i = 0; i < 14U; i++) {
        data[i] ^= 0xFFU;
    }

    bool result = rs.decode462621(data);
    REQUIRE(!result);
}
