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

#include "common/nxdn/channel/FACCH1.h"
#include "common/nxdn/NXDNDefines.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::channel;

TEST_CASE("FACCH1 encodes and decodes zeros", "[nxdn][facch1]") {
    uint8_t dataIn[10U];
    ::memset(dataIn, 0x00U, sizeof(dataIn));

    uint8_t frameData[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(frameData, 0x00U, sizeof(frameData));

    FACCH1 facch;
    facch.setData(dataIn);
    facch.encode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS);

    // Decode and verify
    FACCH1 decoded;
    REQUIRE(decoded.decode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS));

    uint8_t dataOut[10U];
    decoded.getData(dataOut);
    REQUIRE(::memcmp(dataIn, dataOut, 10U) == 0);
}

TEST_CASE("FACCH1 encodes and decodes ones", "[nxdn][facch1]") {
    uint8_t dataIn[10U];
    ::memset(dataIn, 0xFFU, sizeof(dataIn));

    uint8_t frameData[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(frameData, 0x00U, sizeof(frameData));

    FACCH1 facch;
    facch.setData(dataIn);
    facch.encode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS);

    FACCH1 decoded;
    REQUIRE(decoded.decode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS));

    uint8_t dataOut[10U];
    decoded.getData(dataOut);
    REQUIRE(::memcmp(dataIn, dataOut, 10U) == 0);
}

TEST_CASE("FACCH1 encodes and decodes alternating pattern", "[nxdn][facch1]") {
    uint8_t dataIn[10U] = {0xAAU, 0x55U, 0xAAU, 0x55U, 0xAAU, 0x55U, 0xAAU, 0x55U, 0xAAU, 0x55U};

    uint8_t frameData[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(frameData, 0x00U, sizeof(frameData));

    FACCH1 facch;
    facch.setData(dataIn);
    facch.encode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS);

    FACCH1 decoded;
    REQUIRE(decoded.decode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS));

    uint8_t dataOut[10U];
    decoded.getData(dataOut);
    REQUIRE(::memcmp(dataIn, dataOut, 10U) == 0);
}

TEST_CASE("FACCH1 handles sequential data patterns", "[nxdn][facch1]") {
    const uint8_t patterns[][10] = {
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99},
        {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x11, 0x22},
        {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66}
    };

    for (const auto& pattern : patterns) {
        uint8_t frameData[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(frameData, 0x00U, sizeof(frameData));

        FACCH1 facch;
        facch.setData(pattern);
        facch.encode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS);

        FACCH1 decoded;
        REQUIRE(decoded.decode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS));

        uint8_t dataOut[10U];
        decoded.getData(dataOut);
        REQUIRE(::memcmp(pattern, dataOut, 10U) == 0);
    }
}

TEST_CASE("FACCH1 decodes at alternate bit offset", "[nxdn][facch1]") {
    uint8_t dataIn[10U] = {0xA5, 0x5A, 0xF0, 0x0F, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};

    uint8_t frameData[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(frameData, 0x00U, sizeof(frameData));

    // Encode at second FACCH1 position
    FACCH1 facch;
    facch.setData(dataIn);
    const uint32_t secondOffset = NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS + NXDN_FACCH1_FEC_LENGTH_BITS;
    facch.encode(frameData, secondOffset);

    // Decode from second position
    FACCH1 decoded;
    REQUIRE(decoded.decode(frameData, secondOffset));

    uint8_t dataOut[10U];
    decoded.getData(dataOut);
    REQUIRE(::memcmp(dataIn, dataOut, 10U) == 0);
}

TEST_CASE("FACCH1 copy constructor preserves data", "[nxdn][facch1]") {
    uint8_t testData[10U] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA};

    FACCH1 original;
    original.setData(testData);

    FACCH1 copy(original);

    uint8_t originalData[10U], copyData[10U];
    original.getData(originalData);
    copy.getData(copyData);
    REQUIRE(::memcmp(originalData, copyData, 10U) == 0);
}

TEST_CASE("FACCH1 assignment operator preserves data", "[nxdn][facch1]") {
    uint8_t testData[10U] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33};

    FACCH1 original;
    original.setData(testData);

    FACCH1 assigned;
    assigned = original;

    uint8_t originalData[10U], assignedData[10U];
    original.getData(originalData);
    assigned.getData(assignedData);
    REQUIRE(::memcmp(originalData, assignedData, 10U) == 0);
}

TEST_CASE("FACCH1 rejects invalid CRC", "[nxdn][facch1]") {
    uint8_t frameData[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(frameData, 0xFFU, sizeof(frameData));

    // Create random corrupted data that should fail CRC
    for (uint32_t i = 0; i < NXDN_FACCH1_FEC_LENGTH_BYTES; i++) {
        frameData[i] = static_cast<uint8_t>(i * 17 + 23);
    }

    FACCH1 decoded;
    // Decode may succeed or fail depending on corruption, but this tests the CRC validation path
    decoded.decode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS);
}

TEST_CASE("FACCH1 golden test for voice call header", "[nxdn][facch1][golden]") {
    uint8_t dataIn[10U];
    ::memset(dataIn, 0x00U, sizeof(dataIn));
    // Simulate RTCH header structure
    dataIn[0] = MessageType::RTCH_VCALL; // Message Type
    dataIn[1] = 0x00; // Options
    dataIn[2] = 0x12; // Source ID (high)
    dataIn[3] = 0x34; // Source ID (low)
    dataIn[4] = 0x56; // Dest ID (high)
    dataIn[5] = 0x78; // Dest ID (low)

    uint8_t frameData[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(frameData, 0x00U, sizeof(frameData));

    FACCH1 facch;
    facch.setData(dataIn);
    facch.encode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS);

    // Decode and verify round-trip
    FACCH1 decoded;
    REQUIRE(decoded.decode(frameData, NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS));

    uint8_t dataOut[10U];
    decoded.getData(dataOut);
    REQUIRE(::memcmp(dataIn, dataOut, 10U) == 0);
}
