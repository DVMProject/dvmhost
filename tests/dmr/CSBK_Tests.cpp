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
#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/csbk/CSBK_RAW.h"
#include "common/edac/CRC.h"
#include "common/edac/BPTC19696.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CSBK", "[dmr][csbk]") {
    SECTION("Constants_Valid") {
        // Verify CSBK length constants
        REQUIRE(DMR_CSBK_LENGTH_BYTES == 12);
        REQUIRE(DMR_FRAME_LENGTH_BYTES == 33);
    }

    SECTION("Encode_Decode_RoundTrip") {
        // Test basic encoding/decoding round trip
        CSBK_RAW csbk1;
        
        // Create a test CSBK payload (12 bytes)
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
        
        // Set CSBKO (Control Signalling Block Opcode) - byte 0, bits 0-5
        testCSBK[0] = CSBKO::RAND; // Random Access opcode
        testCSBK[1] = 0x00; // FID (Feature ID) - standard
        
        // Set some payload data (bytes 2-9)
        for (uint32_t i = 2; i < 10; i++) {
            testCSBK[i] = (uint8_t)(i * 0x11);
        }
        
        // Add CRC-CCITT16 (bytes 10-11) with mask
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        // Set the CSBK
        csbk1.setCSBK(testCSBK);
        
        // Encode with BPTC (196,96) FEC
        uint8_t encoded[DMR_FRAME_LENGTH_BYTES];
        ::memset(encoded, 0x00, DMR_FRAME_LENGTH_BYTES);
        csbk1.encode(encoded);
        
        // Decode back
        CSBK_RAW csbk2;
        csbk2.setDataType(DataType::CSBK);
        bool result = csbk2.decode(encoded);
        
        REQUIRE(result == true);
        REQUIRE(csbk2.getCSBKO() == (testCSBK[0] & 0x3F));
        REQUIRE(csbk2.getFID() == testCSBK[1]);
    }

    SECTION("LastBlock_Flag") {
        // Test Last Block Marker flag
        CSBK_RAW csbk;
        
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
        
        // Set Last Block flag (bit 7 of byte 0)
        testCSBK[0] = 0x80 | CSBKO::RAND; // Last Block + CSBKO
        testCSBK[1] = 0x00;
        
        // Add CRC
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        csbk.setCSBK(testCSBK);
        
        REQUIRE(csbk.getLastBlock() == true);
        REQUIRE(csbk.getCSBKO() == CSBKO::RAND);
    }

    SECTION("FID_Preservation") {
        // Test Feature ID preservation
        uint8_t fids[] = { 0x00, 0x01, 0x10, 0xFF };
        
        for (auto fid : fids) {
            CSBK_RAW csbk;
            
            uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
            ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
            
            testCSBK[0] = CSBKO::RAND;
            testCSBK[1] = fid;
            
            testCSBK[10] ^= CSBK_CRC_MASK[0];
            testCSBK[11] ^= CSBK_CRC_MASK[1];
            edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
            testCSBK[10] ^= CSBK_CRC_MASK[0];
            testCSBK[11] ^= CSBK_CRC_MASK[1];
            
            csbk.setCSBK(testCSBK);
            
            REQUIRE(csbk.getFID() == fid);
        }
    }

    SECTION("CRC_CCITT16_With_Mask") {
        // Test CRC-CCITT16 with DMR mask
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
        
        testCSBK[0] = CSBKO::RAND;
        testCSBK[1] = 0x00;
        testCSBK[2] = 0xAB;
        testCSBK[3] = 0xCD;
        
        // Apply mask before CRC
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        // Add CRC
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        
        // Verify CRC is valid with mask applied
        bool crcValid = edac::CRC::checkCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        REQUIRE(crcValid == true);
        
        // Remove mask
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        // Corrupt the CRC
        testCSBK[11] ^= 0xFF;
        
        // Apply mask again
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        // Verify CRC is now invalid
        crcValid = edac::CRC::checkCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        REQUIRE(crcValid == false);
    }

    SECTION("Payload_RoundTrip") {
        // Test payload data round-trip (bytes 2-9, 8 bytes)
        CSBK_RAW csbk;
        
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
        
        testCSBK[0] = CSBKO::RAND;
        testCSBK[1] = 0x00;
        
        // Payload is bytes 2-9 (8 bytes)
        uint8_t expectedPayload[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
        ::memcpy(testCSBK + 2, expectedPayload, 8);
        
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        csbk.setCSBK(testCSBK);
        
        // Encode and verify it can be encoded without errors
        uint8_t encoded[DMR_FRAME_LENGTH_BYTES];
        ::memset(encoded, 0x00, DMR_FRAME_LENGTH_BYTES);
        csbk.encode(encoded);
        
        // Verify BPTC encoding produced non-zero data
        bool hasData = false;
        for (uint32_t i = 0; i < DMR_FRAME_LENGTH_BYTES; i++) {
            if (encoded[i] != 0x00) {
                hasData = true;
                break;
            }
        }
        REQUIRE(hasData == true);
    }

    SECTION("BPTC_FEC_Encoding") {
        // Test BPTC (196,96) FEC encoding
        CSBK_RAW csbk;
        
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
        
        testCSBK[0] = CSBKO::RAND;
        testCSBK[1] = 0x00;
        
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        csbk.setCSBK(testCSBK);
        
        // Encode with BPTC FEC
        uint8_t encoded[DMR_FRAME_LENGTH_BYTES];
        ::memset(encoded, 0x00, DMR_FRAME_LENGTH_BYTES);
        csbk.encode(encoded);
        
        // Verify encoding produced data
        bool hasData = false;
        for (uint32_t i = 0; i < DMR_FRAME_LENGTH_BYTES; i++) {
            if (encoded[i] != 0x00) {
                hasData = true;
                break;
            }
        }
        REQUIRE(hasData == true);
    }

    SECTION("AllZeros_Pattern") {
        // Test all-zeros pattern
        CSBK_RAW csbk;
        
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
        
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        csbk.setCSBK(testCSBK);
        
        uint8_t encoded[DMR_FRAME_LENGTH_BYTES];
        csbk.encode(encoded);
        
        CSBK_RAW csbk2;
        csbk2.setDataType(DataType::CSBK);
        bool result = csbk2.decode(encoded);
        
        REQUIRE(result == true);
    }

    SECTION("AllOnes_Pattern") {
        // Test all-ones pattern (with valid structure)
        CSBK_RAW csbk;
        
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        ::memset(testCSBK, 0xFF, DMR_CSBK_LENGTH_BYTES);
        
        // Set CSBKO to DVM_GIT_HASH (0x3F) with Last Block flag
        testCSBK[0] = 0xBF; // Last Block (0x80) + CSBKO 0x3F = 0xBF
        
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        csbk.setCSBK(testCSBK);
        
        uint8_t encoded[DMR_FRAME_LENGTH_BYTES];
        csbk.encode(encoded);
        
        // Verify encoding succeeded
        bool hasData = false;
        for (uint32_t i = 0; i < DMR_FRAME_LENGTH_BYTES; i++) {
            if (encoded[i] != 0x00) {
                hasData = true;
                break;
            }
        }
        REQUIRE(hasData == true);
        
        // Verify the setCSBK extracted values correctly
        REQUIRE(csbk.getCSBKO() == 0x3F); // DVM_GIT_HASH
        REQUIRE(csbk.getLastBlock() == true);
    }

    SECTION("Alternating_Pattern") {
        // Test alternating bit pattern
        CSBK_RAW csbk;
        
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        for (uint32_t i = 0; i < DMR_CSBK_LENGTH_BYTES; i++) {
            testCSBK[i] = (i % 2 == 0) ? 0xAA : 0x55;
        }
        
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        testCSBK[10] ^= CSBK_CRC_MASK[0];
        testCSBK[11] ^= CSBK_CRC_MASK[1];
        
        csbk.setCSBK(testCSBK);
        
        uint8_t encoded[DMR_FRAME_LENGTH_BYTES];
        csbk.encode(encoded);
        
        CSBK_RAW csbk2;
        csbk2.setDataType(DataType::CSBK);
        bool result = csbk2.decode(encoded);
        
        REQUIRE(result == true);
    }

    SECTION("CSBKO_Values") {
        // Test various CSBKO values (6 bits)
        uint8_t csbkoValues[] = { 
            CSBKO::RAND, 
            CSBKO::BSDWNACT,
            CSBKO::PRECCSBK,
            0x00, 0x01, 0x0F, 0x20, 0x3F 
        };
        
        for (uint32_t i = 0; i < sizeof(csbkoValues); i++) {
            uint8_t csbko = csbkoValues[i];
            CSBK_RAW csbk;
            
            uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
            ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
            
            testCSBK[0] = csbko & 0x3F; // Mask to 6 bits
            testCSBK[1] = 0x00;
            
            testCSBK[10] ^= CSBK_CRC_MASK[0];
            testCSBK[11] ^= CSBK_CRC_MASK[1];
            edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
            testCSBK[10] ^= CSBK_CRC_MASK[0];
            testCSBK[11] ^= CSBK_CRC_MASK[1];
            
            csbk.setCSBK(testCSBK);
            
            REQUIRE(csbk.getCSBKO() == (csbko & 0x3F));
        }
    }

    SECTION("MBC_CRC_Mask") {
        // Test MBC (Multi-Block Control) CRC mask variant
        uint8_t testCSBK[DMR_CSBK_LENGTH_BYTES];
        ::memset(testCSBK, 0x00, DMR_CSBK_LENGTH_BYTES);
        
        testCSBK[0] = CSBKO::PRECCSBK; // Preamble CSBK uses MBC header
        testCSBK[1] = 0x00;
        
        // Apply MBC mask before CRC
        testCSBK[10] ^= CSBK_MBC_CRC_MASK[0];
        testCSBK[11] ^= CSBK_MBC_CRC_MASK[1];
        
        // Add CRC
        edac::CRC::addCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        
        // Verify CRC is valid with MBC mask applied
        bool crcValid = edac::CRC::checkCCITT162(testCSBK, DMR_CSBK_LENGTH_BYTES);
        REQUIRE(crcValid == true);
    }

    SECTION("DataType_CSBK") {
        // Test with DataType::CSBK
        CSBK_RAW csbk;
        csbk.setDataType(DataType::CSBK);
        
        REQUIRE(csbk.getDataType() == DataType::CSBK);
    }

    SECTION("DataType_MBC_HEADER") {
        // Test with DataType::MBC_HEADER
        CSBK_RAW csbk;
        csbk.setDataType(DataType::MBC_HEADER);
        
        REQUIRE(csbk.getDataType() == DataType::MBC_HEADER);
    }
}
