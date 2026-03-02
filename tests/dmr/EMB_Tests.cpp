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

#include "common/dmr/data/EMB.h"

using namespace dmr::data;

TEST_CASE("EMB encodes and decodes without errors", "[dmr][emb]") {
    EMB emb;
    emb.setColorCode(7);
    emb.setPI(true);
    emb.setLCSS(2);

    uint8_t data[24U];
    ::memset(data, 0x00U, sizeof(data));
    
    emb.encode(data);
    
    EMB decoded;
    decoded.decode(data);

    REQUIRE(decoded.getColorCode() == 7);
    REQUIRE(decoded.getPI() == true);
    REQUIRE(decoded.getLCSS() == 2);
}

TEST_CASE("EMB corrects single-bit errors in embedded signaling", "[dmr][emb]") {
    EMB emb;
    emb.setColorCode(5);
    emb.setPI(false);
    emb.setLCSS(1);

    uint8_t data[24U];
    ::memset(data, 0x00U, sizeof(data));
    
    emb.encode(data);
    
    // Save the encoded data
    uint8_t original[24U];
    ::memcpy(original, data, sizeof(data));
    
    // EMB data is stored in nibbles at positions 13, 14, 18, 19
    // This gives us 16 bits total to test
    const uint32_t embPositions[] = {13, 14, 18, 19};
    
    // Test single-bit errors in each nibble
    for (auto pos : embPositions) {
        for (uint32_t bit = 0; bit < 8; bit++) {
            ::memcpy(data, original, sizeof(data));
            
            // Introduce single-bit error
            data[pos] ^= (1U << bit);
            
            EMB decoded;
            decoded.decode(data);
            
            // QR(16,7,6) should correct single-bit errors
            REQUIRE(decoded.getColorCode() == 5);
            REQUIRE(decoded.getPI() == false);
            REQUIRE(decoded.getLCSS() == 1);
        }
    }
}

TEST_CASE("EMB corrects two-bit errors in embedded signaling", "[dmr][emb]") {
    EMB emb;
    emb.setColorCode(12);
    emb.setPI(true);
    emb.setLCSS(3);

    uint8_t data[24U];
    ::memset(data, 0x00U, sizeof(data));
    
    emb.encode(data);
    
    // Save the encoded data
    uint8_t original[24U];
    ::memcpy(original, data, sizeof(data));
    
    // Test two-bit errors in different positions
    const struct {
        uint32_t pos1, bit1, pos2, bit2;
    } errorPairs[] = {
        {13, 0, 13, 7},  // Same byte
        {13, 4, 14, 3},  // Adjacent bytes
        {13, 5, 18, 2},  // Distant bytes
        {14, 1, 19, 6},  // Different nibble pairs
        {18, 0, 19, 7}   // Same nibble pair
    };
    
    for (auto& pair : errorPairs) {
        ::memcpy(data, original, sizeof(data));
        
        // Introduce two-bit errors
        data[pair.pos1] ^= (1U << pair.bit1);
        data[pair.pos2] ^= (1U << pair.bit2);
        
        EMB decoded;
        decoded.decode(data);
        
        // QR(16,7,6) should correct two-bit errors
        REQUIRE(decoded.getColorCode() == 12);
        REQUIRE(decoded.getPI() == true);
        REQUIRE(decoded.getLCSS() == 3);
    }
}

TEST_CASE("EMB tests all color code values", "[dmr][emb]") {
    // Color code is 4 bits (0-15)
    for (uint32_t cc = 0; cc < 16; cc++) {
        EMB emb;
        emb.setColorCode(cc);
        emb.setPI(cc & 1);  // Alternate PI
        emb.setLCSS(cc & 3);  // Cycle through LCSS values

        uint8_t data[24U];
        ::memset(data, 0x00U, sizeof(data));
        
        emb.encode(data);
        
        EMB decoded;
        decoded.decode(data);

        REQUIRE(decoded.getColorCode() == cc);
        REQUIRE(decoded.getPI() == (bool)(cc & 1));
        REQUIRE(decoded.getLCSS() == (cc & 3));
    }
}

TEST_CASE("EMB verifies error correction restores correct values after corruption", "[dmr][emb]") {
    // This test specifically verifies that the bug fix works:
    // Before the fix, decode() would return the corrected value but EMB
    // would read from the uncorrected buffer, causing wrong results.
    
    EMB emb;
    emb.setColorCode(9);
    emb.setPI(false);
    emb.setLCSS(2);

    uint8_t data[24U];
    ::memset(data, 0xAAU, sizeof(data));  // Non-zero background
    
    emb.encode(data);
    
    // Corrupt the EMB data with a single-bit error in position 13, bit 4
    // This should be correctable by QR(16,7,6)
    data[13] ^= 0x10U;
    
    EMB decoded;
    decoded.decode(data);
    
    // Verify the corrected values are read (not the corrupted buffer)
    REQUIRE(decoded.getColorCode() == 9);
    REQUIRE(decoded.getPI() == false);
    REQUIRE(decoded.getLCSS() == 2);
    
    // Now encode again and verify we get the same result as original
    uint8_t reencoded[24U];
    ::memset(reencoded, 0xAAU, sizeof(reencoded));  // Same background as original
    decoded.encode(reencoded);
    
    // The EMB portions should match the original uncorrupted encoding
    uint8_t original[24U];
    ::memset(original, 0xAAU, sizeof(original));
    emb.encode(original);
    
    // EMB data is in nibbles, so we need to mask and compare
    REQUIRE((reencoded[13] & 0x0F) == (original[13] & 0x0F));
    REQUIRE((reencoded[14] & 0xF0) == (original[14] & 0xF0));
    REQUIRE((reencoded[18] & 0x0F) == (original[18] & 0x0F));
    REQUIRE((reencoded[19] & 0xF0) == (original[19] & 0xF0));
}
