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
#include "common/p25/lc/tdulc/LC_GROUP.h"
#include "common/p25/lc/tdulc/LC_PRIVATE.h"
#include "common/p25/P25Defines.h"
#include "common/edac/Golay24128.h"
#include "common/edac/RS634717.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <catch2/catch_test_macros.hpp>

TEST_CASE("TDULC", "[p25][tdulc]") {
    SECTION("Constants_Valid") {
        // Verify TDULC length constants
        REQUIRE(P25_TDULC_LENGTH_BYTES == 18);           // Total length with RS FEC
        REQUIRE(P25_TDULC_PAYLOAD_LENGTH_BYTES == 8);    // Payload only
        REQUIRE(P25_TDULC_FEC_LENGTH_BYTES == 36);       // After Golay encoding
        REQUIRE(P25_TDULC_FRAME_LENGTH_BYTES == 54);     // Full frame with preamble
    }

    SECTION("Golay_Encode_Decode") {
        // Test Golay (24,12,8) FEC encoding/decoding
        uint8_t input[P25_TDULC_LENGTH_BYTES];
        ::memset(input, 0x00, P25_TDULC_LENGTH_BYTES);
        
        // Set test pattern
        input[0] = 0x12;
        input[1] = 0x34;
        input[2] = 0x56;
        input[3] = 0x78;
        
        // Encode with Golay
        uint8_t encoded[P25_TDULC_FEC_LENGTH_BYTES + 1];
        ::memset(encoded, 0x00, P25_TDULC_FEC_LENGTH_BYTES + 1);
        edac::Golay24128::encode24128(encoded, input, P25_TDULC_LENGTH_BYTES);
        
        // Decode with Golay
        uint8_t decoded[P25_TDULC_LENGTH_BYTES];
        ::memset(decoded, 0x00, P25_TDULC_LENGTH_BYTES);
        edac::Golay24128::decode24128(decoded, encoded, P25_TDULC_LENGTH_BYTES);
        
        // Verify round-trip
        for (uint32_t i = 0; i < P25_TDULC_LENGTH_BYTES; i++) {
            REQUIRE(decoded[i] == input[i]);
        }
    }

    SECTION("RS_241213_Encode_Decode") {
        // Test RS (24,12,13) FEC encoding/decoding
        edac::RS634717 rs;
        
        uint8_t input[P25_TDULC_LENGTH_BYTES];
        ::memset(input, 0x00, P25_TDULC_LENGTH_BYTES);
        
        // Set test pattern in first 12 bytes (data portion)
        for (uint32_t i = 0; i < 12; i++) {
            input[i] = (uint8_t)(i * 0x11);
        }
        
        // Encode RS (adds 6 parity bytes)
        rs.encode241213(input);
        
        // Decode RS
        bool result = rs.decode241213(input);
        
        REQUIRE(result == true);
    }

    SECTION("LCO_Values") {
        // Test various LCO values (6 bits)
        uint8_t lcoValues[] = { 0x00, 0x01, 0x02, 0x03, 0x20, 0x3F };
        
        for (auto lco : lcoValues) {
            LC_GROUP tdulc;
            tdulc.setLCO(lco & 0x3F); // Mask to 6 bits
            
            REQUIRE(tdulc.getLCO() == (lco & 0x3F));
        }
    }

    SECTION("Emergency_Flag") {
        // Test emergency flag
        LC_GROUP tdulc;
        
        tdulc.setEmergency(false);
        REQUIRE(tdulc.getEmergency() == false);
        
        tdulc.setEmergency(true);
        REQUIRE(tdulc.getEmergency() == true);
    }

    SECTION("Encrypted_Flag") {
        // Test encrypted flag
        LC_GROUP tdulc;
        
        tdulc.setEncrypted(false);
        REQUIRE(tdulc.getEncrypted() == false);
        
        tdulc.setEncrypted(true);
        REQUIRE(tdulc.getEncrypted() == true);
    }

    SECTION("Priority_Values") {
        // Test priority values (3 bits: 0-7)
        for (uint8_t priority = 0; priority <= 7; priority++) {
            LC_GROUP tdulc;
            tdulc.setPriority(priority);
            
            REQUIRE(tdulc.getPriority() == priority);
        }
    }

    SECTION("Group_Flag") {
        // Test group flag
        LC_GROUP groupTdulc;
        groupTdulc.setGroup(true);
        REQUIRE(groupTdulc.getGroup() == true);
        
        LC_PRIVATE privateTdulc;
        privateTdulc.setGroup(false);
        REQUIRE(privateTdulc.getGroup() == false);
    }

    SECTION("SrcId_Values") {
        // Test source ID values (24 bits)
        uint32_t srcIds[] = { 0x000000, 0x000001, 0x123456, 0xFFFFFE, 0xFFFFFF };
        
        for (auto srcId : srcIds) {
            LC_GROUP tdulc;
            tdulc.setSrcId(srcId & 0xFFFFFF); // Mask to 24 bits
            
            REQUIRE(tdulc.getSrcId() == (srcId & 0xFFFFFF));
        }
    }

    SECTION("DstId_Values") {
        // Test destination ID values (16 bits for group)
        uint32_t dstIds[] = { 0x0000, 0x0001, 0x1234, 0xFFFE, 0xFFFF };
        
        for (auto dstId : dstIds) {
            LC_GROUP tdulc;
            tdulc.setDstId(dstId & 0xFFFF); // Mask to 16 bits
            
            REQUIRE(tdulc.getDstId() == (dstId & 0xFFFF));
        }
    }

    SECTION("MfgId_Values") {
        // Test manufacturer ID values
        uint8_t mfgIds[] = { 0x00, 0x01, 0x90, 0xFF };
        
        for (auto mfgId : mfgIds) {
            LC_GROUP tdulc;
            tdulc.setMFId(mfgId);
            
            REQUIRE(tdulc.getMFId() == mfgId);
        }
    }

    SECTION("AllZeros_Pattern") {
        // Test all-zeros pattern
        LC_GROUP tdulc;
        
        tdulc.setLCO(0x00);
        tdulc.setMFId(0x00);
        tdulc.setSrcId(0x000000);
        tdulc.setDstId(0x0000);
        tdulc.setEmergency(false);
        tdulc.setEncrypted(false);
        tdulc.setPriority(0);
        
        REQUIRE(tdulc.getLCO() == 0x00);
        REQUIRE(tdulc.getMFId() == 0x00);
        REQUIRE(tdulc.getSrcId() == 0x000000);
        REQUIRE(tdulc.getDstId() == 0x0000);
        REQUIRE(tdulc.getEmergency() == false);
        REQUIRE(tdulc.getEncrypted() == false);
        REQUIRE(tdulc.getPriority() == 0);
    }

    SECTION("MaxValues_Pattern") {
        // Test maximum values pattern
        LC_GROUP tdulc;
        
        tdulc.setLCO(0x3F);                    // 6 bits max
        tdulc.setMFId(0xFF);                   // 8 bits max
        tdulc.setSrcId(0xFFFFFF);              // 24 bits max
        tdulc.setDstId(0xFFFF);                // 16 bits max
        tdulc.setEmergency(true);
        tdulc.setEncrypted(true);
        tdulc.setPriority(7);                  // 3 bits max
        
        REQUIRE(tdulc.getLCO() == 0x3F);
        REQUIRE(tdulc.getMFId() == 0xFF);
        REQUIRE(tdulc.getSrcId() == 0xFFFFFF);
        REQUIRE(tdulc.getDstId() == 0xFFFF);
        REQUIRE(tdulc.getEmergency() == true);
        REQUIRE(tdulc.getEncrypted() == true);
        REQUIRE(tdulc.getPriority() == 7);
    }

    SECTION("Group_Copy_Constructor") {
        // Test copy constructor for LC_GROUP
        LC_GROUP tdulc1;
        
        tdulc1.setLCO(0x00);
        tdulc1.setMFId(0x90);
        tdulc1.setSrcId(0x123456);
        tdulc1.setDstId(0xABCD);
        tdulc1.setEmergency(true);
        tdulc1.setEncrypted(false);
        tdulc1.setPriority(5);
        
        LC_GROUP tdulc2(tdulc1);
        
        REQUIRE(tdulc2.getLCO() == tdulc1.getLCO());
        REQUIRE(tdulc2.getMFId() == tdulc1.getMFId());
        REQUIRE(tdulc2.getSrcId() == tdulc1.getSrcId());
        REQUIRE(tdulc2.getDstId() == tdulc1.getDstId());
        REQUIRE(tdulc2.getEmergency() == tdulc1.getEmergency());
        REQUIRE(tdulc2.getEncrypted() == tdulc1.getEncrypted());
        REQUIRE(tdulc2.getPriority() == tdulc1.getPriority());
    }

    SECTION("Private_Copy_Constructor") {
        // Test copy constructor for LC_PRIVATE
        LC_PRIVATE tdulc1;
        
        tdulc1.setLCO(0x03);
        tdulc1.setMFId(0x00);
        tdulc1.setSrcId(0xABCDEF);
        tdulc1.setDstId(0x123456);
        tdulc1.setEmergency(false);
        tdulc1.setEncrypted(true);
        tdulc1.setPriority(3);
        
        LC_PRIVATE tdulc2(tdulc1);
        
        REQUIRE(tdulc2.getLCO() == tdulc1.getLCO());
        REQUIRE(tdulc2.getMFId() == tdulc1.getMFId());
        REQUIRE(tdulc2.getSrcId() == tdulc1.getSrcId());
        REQUIRE(tdulc2.getDstId() == tdulc1.getDstId());
        REQUIRE(tdulc2.getEmergency() == tdulc1.getEmergency());
        REQUIRE(tdulc2.getEncrypted() == tdulc1.getEncrypted());
        REQUIRE(tdulc2.getPriority() == tdulc1.getPriority());
    }

    SECTION("Golay_ErrorCorrection") {
        // Test Golay error correction capability
        uint8_t input[P25_TDULC_LENGTH_BYTES];
        ::memset(input, 0x00, P25_TDULC_LENGTH_BYTES);
        
        // Set known pattern
        input[0] = 0xAA;
        input[1] = 0x55;
        input[2] = 0xF0;
        input[3] = 0x0F;
        
        // Encode
        uint8_t encoded[P25_TDULC_FEC_LENGTH_BYTES + 1];
        ::memset(encoded, 0x00, P25_TDULC_FEC_LENGTH_BYTES + 1);
        edac::Golay24128::encode24128(encoded, input, P25_TDULC_LENGTH_BYTES);
        
        // Introduce single bit error (Golay can correct up to 3 bit errors)
        encoded[5] ^= 0x01;
        
        // Decode (should correct the error)
        uint8_t decoded[P25_TDULC_LENGTH_BYTES];
        ::memset(decoded, 0x00, P25_TDULC_LENGTH_BYTES);
        edac::Golay24128::decode24128(decoded, encoded, P25_TDULC_LENGTH_BYTES);
        
        // Verify correction
        REQUIRE(decoded[0] == input[0]);
        REQUIRE(decoded[1] == input[1]);
        REQUIRE(decoded[2] == input[2]);
        REQUIRE(decoded[3] == input[3]);
    }

    SECTION("RS_ErrorCorrection") {
        // Test RS error correction capability  
        edac::RS634717 rs;
        
        uint8_t data[P25_TDULC_LENGTH_BYTES];
        ::memset(data, 0x00, P25_TDULC_LENGTH_BYTES);
        
        // Set known data pattern in first 12 bytes
        for (uint32_t i = 0; i < 12; i++) {
            data[i] = (uint8_t)(0xAA - i);
        }
        
        // Encode RS
        rs.encode241213(data);
        
        // Save original
        uint8_t original[P25_TDULC_LENGTH_BYTES];
        ::memcpy(original, data, P25_TDULC_LENGTH_BYTES);
        
        // Introduce errors (RS can correct up to 6 byte errors with (24,12,13))
        data[2] ^= 0xFF;
        data[5] ^= 0xFF;
        
        // Decode (should correct the errors)
        bool result = rs.decode241213(data);
        
        REQUIRE(result == true);
        
        // Verify correction
        for (uint32_t i = 0; i < 12; i++) {
            REQUIRE(data[i] == original[i]);
        }
    }
}
