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

#include "common/dmr/SlotType.h"
#include "common/dmr/DMRDefines.h"

using namespace dmr;
using namespace dmr::defines;

TEST_CASE("SlotType encodes and decodes DataType correctly", "[dmr][slottype]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];
    ::memset(frame, 0x00U, sizeof(frame));

    SlotType slotType;
    slotType.setColorCode(5U);
    slotType.setDataType(DataType::VOICE_LC_HEADER);
    slotType.encode(frame + 2U);

    // Decode and verify
    SlotType decoded;
    decoded.decode(frame + 2U);
    REQUIRE(decoded.getColorCode() == 5U);
    REQUIRE(decoded.getDataType() == DataType::VOICE_LC_HEADER);
}

TEST_CASE("SlotType handles all DataType values", "[dmr][slottype]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];

    const DataType::E types[] = {
        DataType::VOICE_PI_HEADER,
        DataType::VOICE_LC_HEADER,
        DataType::TERMINATOR_WITH_LC,
        DataType::CSBK,
        DataType::DATA_HEADER,
        DataType::RATE_12_DATA,
        DataType::RATE_34_DATA,
        DataType::IDLE,
        DataType::RATE_1_DATA
    };

    for (auto type : types) {
        ::memset(frame, 0x00U, sizeof(frame));
        
        SlotType slotType;
        slotType.setColorCode(3U);
        slotType.setDataType(type);
        slotType.encode(frame + 2U);

        SlotType decoded;
        decoded.decode(frame + 2U);
        REQUIRE(decoded.getColorCode() == 3U);
        REQUIRE(decoded.getDataType() == type);
    }
}

TEST_CASE("SlotType handles all valid ColorCode values", "[dmr][slottype]") {
    uint8_t frame[DMR_FRAME_LENGTH_BYTES + 2U];

    for (uint32_t cc = 0U; cc <= 15U; cc++) {
        ::memset(frame, 0x00U, sizeof(frame));
        
        SlotType slotType;
        slotType.setColorCode(cc);
        slotType.setDataType(DataType::CSBK);
        slotType.encode(frame + 2U);

        SlotType decoded;
        decoded.decode(frame + 2U);
        REQUIRE(decoded.getColorCode() == cc);
    }
}
