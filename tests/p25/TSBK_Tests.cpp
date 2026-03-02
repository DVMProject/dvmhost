// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/p25/lc/tsbk/OSP_SCCB.h"
#include "common/p25/lc/tsbk/OSP_TSBK_RAW.h"
#include "common/p25/P25Defines.h"
#include "common/edac/CRC.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <catch2/catch_test_macros.hpp>

TEST_CASE("TSBK", "[p25][tsbk]") {
    SECTION("Constants_Valid") {
        // Verify TSBK length constants
        REQUIRE(P25_TSBK_LENGTH_BYTES == 12);
        REQUIRE(P25_TSBK_FEC_LENGTH_BYTES == 25);
        REQUIRE(P25_TSBK_FEC_LENGTH_BITS == (P25_TSBK_FEC_LENGTH_BYTES * 8 - 4)); // 196 bits (Trellis)
    }

    SECTION("RawTSBK_Encode_Decode_NoTrellis") {
        g_logDisplayLevel = 1U;

        // Test raw TSBK encoding/decoding without Trellis
        OSP_TSBK_RAW tsbk1;

        // Create a test TSBK payload
        uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
        ::memset(testTSBK, 0x00, P25_TSBK_LENGTH_BYTES);

        // Set LCO (Link Control Opcode)
        testTSBK[0] = 0x34; // Example LCO (OSP_SCCB)
        testTSBK[1] = 0x00; // Mfg ID (standard)

        tsbk1.setLCO(0x34U);
        tsbk1.setMFId(0x00U);

        // Set some payload data
        for (uint32_t i = 2; i < P25_TSBK_LENGTH_BYTES - 2; i++) {
            testTSBK[i] = (uint8_t)(i * 0x11);
        }

        // Add CRC
        edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

        Utils::dump(2U, "testTSBK", testTSBK, P25_TSBK_LENGTH_BYTES);

        // Set the TSBK
        tsbk1.setTSBK(testTSBK);

        // Encode (raw, no Trellis)
        uint8_t encoded[P25_TSBK_LENGTH_BYTES];
        tsbk1.encode(encoded, true, true);

        Utils::dump(2U, "encoded", encoded, P25_TSBK_LENGTH_BYTES);

        // Decode back and verify roundtrip
        OSP_TSBK_RAW tsbk2;
        bool result = tsbk2.decode(encoded, true);

        REQUIRE(result == true);
        REQUIRE(tsbk2.getLCO() == (testTSBK[0] & 0x3F));
        REQUIRE(tsbk2.getMFId() == testTSBK[1]);
        
        // Encode again and verify it matches the first encode
        uint8_t encoded2[P25_TSBK_LENGTH_BYTES];
        tsbk2.encode(encoded2, true, true);
        
        for (uint32_t i = 0; i < P25_TSBK_LENGTH_BYTES; i++) {
            REQUIRE(encoded2[i] == encoded[i]);
        }
    }

    SECTION("RawTSBK_Encode_Decode_WithTrellis") {
        g_logDisplayLevel = 1U;

        // Test raw TSBK encoding/decoding with Trellis FEC
        OSP_TSBK_RAW tsbk1;

        uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
        ::memset(testTSBK, 0x00, P25_TSBK_LENGTH_BYTES);

        testTSBK[0] = 0x34; // LCO
        testTSBK[1] = 0x00; // Mfg ID

        // Set payload
        testTSBK[2] = 0xAA;
        testTSBK[3] = 0x55;
        testTSBK[4] = 0xF0;
        testTSBK[5] = 0x0F;
        testTSBK[6] = 0xCC;
        testTSBK[7] = 0x33;
        testTSBK[8] = 0x12;
        testTSBK[9] = 0x34;

        edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

        Utils::dump(2U, "testTSBK", testTSBK, P25_TSBK_LENGTH_BYTES);

        tsbk1.setTSBK(testTSBK);

        // Encode with Trellis
        uint8_t encoded[P25_TSDU_FRAME_LENGTH_BYTES];
        tsbk1.encode(encoded);

        // Decode with Trellis
        OSP_TSBK_RAW tsbk2;
        bool result = tsbk2.decode(encoded);

        REQUIRE(result == true);
        REQUIRE(tsbk2.getLCO() == (testTSBK[0] & 0x3F));
        REQUIRE(tsbk2.getMFId() == testTSBK[1]);
    }

    SECTION("LastBlock_Flag") {
        // Test Last Block Marker flag
        OSP_TSBK_RAW tsbk1;

        uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
        ::memset(testTSBK, 0x00, P25_TSBK_LENGTH_BYTES);

        // Set Last Block flag (bit 7 of byte 0)
        testTSBK[0] = 0x80 | 0x34; // Last Block + LCO
        testTSBK[1] = 0x00;

        edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

        tsbk1.setTSBK(testTSBK);

        uint8_t encoded[P25_TSBK_LENGTH_BYTES];
        tsbk1.encode(encoded, true, true);

        OSP_TSBK_RAW tsbk2;
        tsbk2.decode(encoded, true);

        REQUIRE(tsbk2.getLastBlock() == true);
        REQUIRE(tsbk2.getLCO() == 0x34);
    }

    SECTION("MfgId_Preservation") {
        // Test Manufacturer ID preservation
        uint8_t mfgIds[] = { 0x00, 0x01, 0x90, 0xFF };

        for (auto mfgId : mfgIds) {
            OSP_TSBK_RAW tsbk1;

            uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
            ::memset(testTSBK, 0x00, P25_TSBK_LENGTH_BYTES);

            testTSBK[0] = 0x34; // LCO
            testTSBK[1] = mfgId;

            edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

            tsbk1.setTSBK(testTSBK);

            uint8_t encoded[P25_TSBK_LENGTH_BYTES];
            tsbk1.encode(encoded, true, true);

            OSP_TSBK_RAW tsbk2;
            tsbk2.decode(encoded, true);

            REQUIRE(tsbk2.getMFId() == mfgId);
        }
    }

    SECTION("CRC_CCITT16_Validation") {
        // Test CRC-CCITT16 validation
        OSP_TSBK_RAW tsbk;

        uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
        ::memset(testTSBK, 0x00, P25_TSBK_LENGTH_BYTES);

        testTSBK[0] = 0x34;
        testTSBK[1] = 0x00;
        testTSBK[2] = 0xAB;
        testTSBK[3] = 0xCD;

        // Add valid CRC
        edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

        // Verify CRC is valid
        bool crcValid = edac::CRC::checkCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);
        REQUIRE(crcValid == true);

        // Corrupt the CRC
        testTSBK[P25_TSBK_LENGTH_BYTES - 1] ^= 0xFF;

        // Verify CRC is now invalid
        crcValid = edac::CRC::checkCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);
        REQUIRE(crcValid == false);
    }

    SECTION("Payload_RoundTrip") {
        // Test payload data round-trip
        OSP_TSBK_RAW tsbk1;

        uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
        ::memset(testTSBK, 0x00, P25_TSBK_LENGTH_BYTES);

        testTSBK[0] = 0x34;
        testTSBK[1] = 0x00;

        // Payload is bytes 2-9 (8 bytes)
        uint8_t expectedPayload[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
        ::memcpy(testTSBK, expectedPayload, 8);

        edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

        tsbk1.setTSBK(testTSBK);

        // Encode and decode
        uint8_t encoded[P25_TSBK_LENGTH_BYTES];
        tsbk1.encode(encoded, true, true);

        OSP_TSBK_RAW tsbk2;
        tsbk2.decode(encoded, true);

        // Get decoded raw data and verify payload
        uint8_t* decoded = tsbk2.getDecodedRaw();
        REQUIRE(decoded != nullptr);

        for (uint32_t i = 0; i < 8; i++) {
            REQUIRE(decoded[i + 2] == expectedPayload[i]);
        }
    }

    SECTION("AllZeros_Pattern") {
        // Test all-zeros pattern
        OSP_TSBK_RAW tsbk1;

        uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
        ::memset(testTSBK, 0x00, P25_TSBK_LENGTH_BYTES);

        edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

        tsbk1.setTSBK(testTSBK);

        uint8_t encoded[P25_TSBK_FEC_LENGTH_BYTES];
        tsbk1.encode(encoded, true, true);

        OSP_TSBK_RAW tsbk2;
        bool result = tsbk2.decode(encoded, true);

        REQUIRE(result == true);
        REQUIRE(tsbk2.getLCO() == 0x00);
        REQUIRE(tsbk2.getMFId() == 0x00);
    }

    SECTION("AllOnes_Pattern") {
        // Test all-ones pattern
        OSP_TSBK_RAW tsbk1;

        uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
        ::memset(testTSBK, 0xFF, P25_TSBK_LENGTH_BYTES);

        // Keep LCO valid (only 6 bits)
        testTSBK[0] = 0xFF; // Last Block + all LCO bits set

        edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

        tsbk1.setTSBK(testTSBK);

        uint8_t encoded[P25_TSBK_FEC_LENGTH_BYTES];
        tsbk1.encode(encoded, true, true);

        OSP_TSBK_RAW tsbk2;
        bool result = tsbk2.decode(encoded, true);

        REQUIRE(result == true);
        REQUIRE(tsbk2.getLCO() == 0x3F); // Only 6 bits
        REQUIRE(tsbk2.getLastBlock() == true);
    }

    SECTION("Alternating_Pattern") {
        // Test alternating bit pattern
        OSP_TSBK_RAW tsbk1;

        uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
        for (uint32_t i = 0; i < P25_TSBK_LENGTH_BYTES; i++) {
            testTSBK[i] = (i % 2 == 0) ? 0xAA : 0x55;
        }

        edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

        tsbk1.setTSBK(testTSBK);

        uint8_t encoded[P25_TSBK_FEC_LENGTH_BYTES];
        tsbk1.encode(encoded, true, true);

        OSP_TSBK_RAW tsbk2;
        bool result = tsbk2.decode(encoded, true);

        REQUIRE(result == true);
    }

    SECTION("LCO_Values") {
        // Test various LCO values (6 bits)
        uint8_t lcoValues[] = { 0x00, 0x01, 0x0F, 0x20, 0x34, 0x3F };
        
        for (auto lco : lcoValues) {
            OSP_TSBK_RAW tsbk1;

            uint8_t testTSBK[P25_TSBK_LENGTH_BYTES];
            ::memset(testTSBK, 0x00, P25_TSBK_LENGTH_BYTES);

            testTSBK[0] = lco & 0x3F; // Mask to 6 bits
            testTSBK[1] = 0x00;

            edac::CRC::addCCITT162(testTSBK, P25_TSBK_LENGTH_BYTES);

            tsbk1.setTSBK(testTSBK);

            uint8_t encoded[P25_TSBK_LENGTH_BYTES];
            tsbk1.encode(encoded, true, true);

            OSP_TSBK_RAW tsbk2;
            tsbk2.decode(encoded, true);

            REQUIRE(tsbk2.getLCO() == (lco & 0x3F));
        }
    }
}
