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

#include "common/nxdn/channel/SACCH.h"
#include "common/nxdn/NXDNDefines.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::channel;

TEST_CASE("SACCH encodes and decodes idle pattern", "[nxdn][sacch]") {
    uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(data, 0x00U, sizeof(data));

    SACCH sacch;
    sacch.setData(SACCH_IDLE);
    sacch.setRAN(1U);
    sacch.setStructure(ChStructure::SR_SINGLE);

    sacch.encode(data);

    // Decode and verify
    SACCH decoded;
    REQUIRE(decoded.decode(data));
    REQUIRE(decoded.getRAN() == 1U);
    REQUIRE(decoded.getStructure() == ChStructure::SR_SINGLE);

    // Verify data matches
    uint8_t decodedData[3U];
    decoded.getData(decodedData);
    REQUIRE(::memcmp(decodedData, SACCH_IDLE, 3U) == 0);
}

TEST_CASE("SACCH preserves all RAN values", "[nxdn][sacch]") {
    for (uint8_t ran = 0U; ran < 64U; ran++) {
        uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(data, 0x00U, sizeof(data));

        SACCH sacch;
        sacch.setData(SACCH_IDLE);
        sacch.setRAN(ran);
        sacch.setStructure(ChStructure::SR_SINGLE);

        sacch.encode(data);

        SACCH decoded;
        REQUIRE(decoded.decode(data));
        REQUIRE(decoded.getRAN() == ran);
    }
}

TEST_CASE("SACCH preserves all ChStructure values", "[nxdn][sacch]") {
    const ChStructure::E structures[] = {
        ChStructure::SR_SINGLE,
        ChStructure::SR_1_4,
        ChStructure::SR_2_4,
        ChStructure::SR_3_4,
        ChStructure::SR_RCCH_SINGLE
    };

    for (auto structure : structures) {
        uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(data, 0x00U, sizeof(data));

        SACCH sacch;
        sacch.setData(SACCH_IDLE);
        sacch.setRAN(1U);
        sacch.setStructure(structure);

        sacch.encode(data);

        SACCH decoded;
        REQUIRE(decoded.decode(data));
        REQUIRE(decoded.getStructure() == structure);
    }
}

TEST_CASE("SACCH copy constructor preserves all fields", "[nxdn][sacch]") {
    SACCH original;
    original.setData(SACCH_IDLE);
    original.setRAN(5U);
    original.setStructure(ChStructure::SR_1_4);

    SACCH copy(original);
    REQUIRE(copy.getRAN() == original.getRAN());
    REQUIRE(copy.getStructure() == original.getStructure());

    // initialize buffers to zero since getData() only writes 18 bits (NXDN_SACCH_LENGTH_BITS - 8)
    uint8_t originalData[3U], copyData[3U];
    ::memset(originalData, 0x00U, 3U);
    ::memset(copyData, 0x00U, 3U);
    original.getData(originalData);
    Utils::dump(2U, "originalData", originalData, 3U);
    copy.getData(copyData);
    Utils::dump(2U, "copyData", copyData, 3U);
    REQUIRE(::memcmp(originalData, copyData, 3U) == 0);
}

TEST_CASE("SACCH assignment operator preserves all fields", "[nxdn][sacch]") {
    SACCH original;
    const uint8_t testData[] = {0x12, 0x34, 0x56};
    original.setData(testData);
    original.setRAN(10U);
    original.setStructure(ChStructure::SR_2_4);

    SACCH assigned;
    assigned = original;
    REQUIRE(assigned.getRAN() == original.getRAN());
    REQUIRE(assigned.getStructure() == original.getStructure());

    // initialize buffers to zero since getData() only writes 18 bits (NXDN_SACCH_LENGTH_BITS - 8)
    uint8_t originalData[3U], assignedData[3U];
    ::memset(originalData, 0x00U, 3U);
    ::memset(assignedData, 0x00U, 3U);
    original.getData(originalData);
    Utils::dump(2U, "originalData", originalData, 3U);
    assigned.getData(assignedData);
    Utils::dump(2U, "assignedData", assignedData, 3U);
    REQUIRE(::memcmp(originalData, assignedData, 3U) == 0);
}

TEST_CASE("SACCH handles multi-part structures", "[nxdn][sacch]") {
    // Test multi-part SACCH structures (SR_1_4, SR_2_4, etc.)
    const ChStructure::E multiPart[] = {
        ChStructure::SR_1_4,
        ChStructure::SR_2_4,
        ChStructure::SR_3_4
    };

    for (auto structure : multiPart) {
        uint8_t data[NXDN_FRAME_LENGTH_BYTES + 2U];
        ::memset(data, 0x00U, sizeof(data));

        SACCH sacch;
        const uint8_t testData[] = {0xA5, 0x5A, 0xC0};
        sacch.setData(testData);
        sacch.setRAN(7U);
        sacch.setStructure(structure);

        sacch.encode(data);

        SACCH decoded;
        REQUIRE(decoded.decode(data));
        REQUIRE(decoded.getStructure() == structure);
        REQUIRE(decoded.getRAN() == 7U);

        uint8_t decodedData[3U];
        decoded.getData(decodedData);
        Utils::dump(2U, "decodedData", decodedData, 3U);
        REQUIRE(::memcmp(decodedData, testData, 3U) == 0);
    }
}
