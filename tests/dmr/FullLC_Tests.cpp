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
#include <memory>

#include "common/dmr/lc/FullLC.h"
#include "common/dmr/lc/LC.h"
#include "common/dmr/DMRDefines.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;

TEST_CASE("FullLC encodes and decodes VOICE_LC_HEADER for private call", "[dmr][fulllc]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    uint32_t srcId = 12345U;
    uint32_t dstId = 54321U;
    
    LC lc(FLCO::PRIVATE, srcId, dstId);
    lc.setFID(0x10U);

    FullLC fullLC;
    fullLC.encode(lc, frame + 2U, DataType::VOICE_LC_HEADER);

    // Decode and verify
    std::unique_ptr<LC> decoded = fullLC.decode(frame + 2U, DataType::VOICE_LC_HEADER);
    REQUIRE(decoded != nullptr);
    REQUIRE(decoded->getFLCO() == FLCO::PRIVATE);
    REQUIRE(decoded->getSrcId() == srcId);
    REQUIRE(decoded->getDstId() == dstId);
    REQUIRE(decoded->getFID() == 0x10U);
}

TEST_CASE("FullLC encodes and decodes VOICE_LC_HEADER for group call", "[dmr][fulllc]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    uint32_t srcId = 1001U;
    uint32_t dstId = 9999U;
    
    LC lc(FLCO::GROUP, srcId, dstId);
    lc.setFID(0x00U);

    FullLC fullLC;
    fullLC.encode(lc, frame + 2U, DataType::VOICE_LC_HEADER);

    // Decode and verify
    std::unique_ptr<LC> decoded = fullLC.decode(frame + 2U, DataType::VOICE_LC_HEADER);
    REQUIRE(decoded != nullptr);
    REQUIRE(decoded->getFLCO() == FLCO::GROUP);
    REQUIRE(decoded->getSrcId() == srcId);
    REQUIRE(decoded->getDstId() == dstId);
}

TEST_CASE("FullLC encodes and decodes TERMINATOR_WITH_LC", "[dmr][fulllc]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    uint32_t srcId = 7777U;
    uint32_t dstId = 8888U;
    
    LC lc(FLCO::GROUP, srcId, dstId);
    lc.setFID(0x02U);

    FullLC fullLC;
    fullLC.encode(lc, frame + 2U, DataType::TERMINATOR_WITH_LC);

    // Decode and verify
    std::unique_ptr<LC> decoded = fullLC.decode(frame + 2U, DataType::TERMINATOR_WITH_LC);
    REQUIRE(decoded != nullptr);
    REQUIRE(decoded->getFLCO() == FLCO::GROUP);
    REQUIRE(decoded->getSrcId() == srcId);
    REQUIRE(decoded->getDstId() == dstId);
    REQUIRE(decoded->getFID() == 0x02U);
}

TEST_CASE("FullLC preserves service options", "[dmr][fulllc]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    uint32_t srcId = 100U;
    uint32_t dstId = 200U;
    
    LC lc(FLCO::PRIVATE, srcId, dstId);
    lc.setFID(0x01U);
    lc.setEmergency(true);
    lc.setEncrypted(true);
    lc.setPriority(3U);

    FullLC fullLC;
    fullLC.encode(lc, frame + 2U, DataType::VOICE_LC_HEADER);

    // Decode and verify options
    std::unique_ptr<LC> decoded = fullLC.decode(frame + 2U, DataType::VOICE_LC_HEADER);
    REQUIRE(decoded != nullptr);
    REQUIRE(decoded->getEmergency() == true);
    REQUIRE(decoded->getEncrypted() == true);
    REQUIRE(decoded->getPriority() == 3U);
}
