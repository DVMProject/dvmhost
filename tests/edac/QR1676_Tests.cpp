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

#include "common/edac/QR1676.h"

using namespace edac;

TEST_CASE("QR1676 preserves all-zero data", "[edac][qr1676]") {
    uint8_t data[2U];
    ::memset(data, 0x00U, sizeof(data));

    QR1676::encode(data);
    uint8_t errors = QR1676::decode(data);

    REQUIRE(errors == 0x00U);
    REQUIRE(data[0U] == 0x00U);
    REQUIRE(data[1U] == 0x00U);
}

TEST_CASE("QR1676 preserves all-ones data", "[edac][qr1676]") {
    // QR(16,7,6): 7 data bits + 9 parity bits = 16 bits
    // Data is stored in upper 7 bits of first byte, shifted left by 1
    uint8_t data[2U];
    data[0U] = 0xFEU;  // 0b11111110 - all 7 data bits set, LSB is parity start
    data[1U] = 0x00U;

    QR1676::encode(data);
    Utils::dump(2U, "QR1676::encode() all ones", data, 2U);

    uint8_t errors = QR1676::decode(data);

    REQUIRE(errors == 0x7FU);  // 7 data bits all set
}

TEST_CASE("QR1676 encodes and decodes specific patterns", "[edac][qr1676]") {
    const uint8_t testValues[] = {0x00U, 0x2AU, 0x54U, 0x0FU, 0x70U, 0x33U, 0x66U, 0x5AU, 0x4BU};

    for (auto value : testValues) {
        uint8_t data[2U];
        ::memset(data, 0x00U, sizeof(data));
        
        // Store 7-bit value in upper bits, shifted left by 1
        data[0U] = (value & 0x7FU) << 1;

        QR1676::encode(data);
        uint8_t decoded = QR1676::decode(data);

        REQUIRE(decoded == (value & 0x7FU));
    }
}

TEST_CASE("QR1676 encodes all 128 possible 7-bit values", "[edac][qr1676]") {
    for (uint32_t value = 0U; value < 128U; value++) {
        uint8_t data[2U];
        ::memset(data, 0x00U, sizeof(data));
        
        data[0U] = (value & 0x7FU) << 1;

        QR1676::encode(data);
        uint8_t decoded = QR1676::decode(data);

        REQUIRE(decoded == value);
    }
}

TEST_CASE("QR1676 corrects single-bit errors", "[edac][qr1676]") {
    uint8_t original = 0x5AU;  // Test pattern
    
    uint8_t data[2U];
    ::memset(data, 0x00U, sizeof(data));
    data[0U] = (original & 0x7FU) << 1;

    QR1676::encode(data);

    // Save encoded data
    uint8_t encoded[2U];
    ::memcpy(encoded, data, 2U);

    // Test single-bit errors in all 16 bit positions
    for (uint32_t bit = 0U; bit < 16U; bit++) {
        ::memcpy(data, encoded, 2U);
        
        // Introduce single-bit error
        uint32_t bytePos = bit / 8;
        uint32_t bitPos = bit % 8;
        data[bytePos] ^= (1U << bitPos);

        uint8_t decoded = QR1676::decode(data);

        // QR(16,7,6) should correct all single-bit errors
        REQUIRE(decoded == (original & 0x7FU));
    }
}

TEST_CASE("QR1676 corrects two-bit errors", "[edac][qr1676]") {
    uint8_t original = 0x3CU;  // Test pattern
    
    uint8_t data[2U];
    ::memset(data, 0x00U, sizeof(data));
    data[0U] = (original & 0x7FU) << 1;

    QR1676::encode(data);

    // Save encoded data
    uint8_t encoded[2U];
    ::memcpy(encoded, data, 2U);

    // Test two-bit error patterns
    const uint32_t errorPairs[][2] = {
        {0, 7}, {1, 8}, {2, 11}, {4, 13}, {6, 15}
    };

    for (auto& pair : errorPairs) {
        ::memcpy(data, encoded, 2U);
        
        // Introduce two-bit errors
        for (auto bitPos : pair) {
            uint32_t bytePos = bitPos / 8;
            uint32_t bit = bitPos % 8;
            data[bytePos] ^= (1U << bit);
        }

        uint8_t decoded = QR1676::decode(data);

        // QR(16,7,6) should correct two-bit errors
        REQUIRE(decoded == (original & 0x7FU));
    }
}

TEST_CASE("QR1676 handles random 7-bit patterns", "[edac][qr1676]") {
    // Test with various pseudo-random patterns
    for (uint32_t test = 0; test < 10; test++) {
        uint8_t value = (uint8_t)((test * 37 + 53) % 128);
        
        uint8_t data[2U];
        ::memset(data, 0x00U, sizeof(data));
        data[0U] = (value & 0x7FU) << 1;

        QR1676::encode(data);
        uint8_t decoded = QR1676::decode(data);

        REQUIRE(decoded == value);
    }
}
