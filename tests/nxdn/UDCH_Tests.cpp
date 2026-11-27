// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "common/nxdn/channel/UDCH.h"
#include "common/nxdn/NXDNDefines.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::channel;

TEST_CASE("UDCH encodes and decodes payload with RAN", "[nxdn][udch]") {
    uint8_t frameData[NXDN_FRAME_LENGTH_BYTES + 2U];
    ::memset(frameData, 0x00U, sizeof(frameData));

    uint8_t payload[NXDN_RTCH_LC_LENGTH_BYTES];
    for (uint32_t i = 0U; i < NXDN_RTCH_LC_LENGTH_BYTES; i++) {
        payload[i] = static_cast<uint8_t>(0xA0U + i);
    }

    UDCH udch;
    udch.setRAN(41U);
    udch.setData(payload);
    udch.encode(frameData);

    UDCH decoded;
    REQUIRE(decoded.decode(frameData));
    REQUIRE(decoded.getRAN() == 41U);

    uint8_t decodedPayload[NXDN_RTCH_LC_LENGTH_BYTES];
    ::memset(decodedPayload, 0x00U, sizeof(decodedPayload));
    decoded.getData(decodedPayload);
    REQUIRE(::memcmp(decodedPayload, payload, sizeof(payload)) == 0);
}

TEST_CASE("UDCH copy and assignment preserve payload", "[nxdn][udch]") {
    uint8_t payload[NXDN_RTCH_LC_LENGTH_BYTES];
    for (uint32_t i = 0U; i < NXDN_RTCH_LC_LENGTH_BYTES; i++) {
        payload[i] = static_cast<uint8_t>(i * 3U + 1U);
    }

    UDCH original;
    original.setRAN(6U);
    original.setData(payload);

    UDCH copy(original);
    REQUIRE(copy.getRAN() == original.getRAN());

    uint8_t originalPayload[NXDN_RTCH_LC_LENGTH_BYTES];
    uint8_t copyPayload[NXDN_RTCH_LC_LENGTH_BYTES];
    original.getData(originalPayload);
    copy.getData(copyPayload);
    REQUIRE(::memcmp(originalPayload, copyPayload, sizeof(originalPayload)) == 0);

    UDCH assigned;
    assigned = original;
    REQUIRE(assigned.getRAN() == original.getRAN());

    uint8_t assignedPayload[NXDN_RTCH_LC_LENGTH_BYTES];
    assigned.getData(assignedPayload);
    REQUIRE(::memcmp(originalPayload, assignedPayload, sizeof(originalPayload)) == 0);
}
