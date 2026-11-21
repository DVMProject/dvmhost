// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "common/BitManipulation.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

TEST_CASE("BitManipulation bit macros set, clear, and read values", "[common][bitmanipulation]")
{
    std::array<uint8_t, 4U> buffer = {};

    REQUIRE(READ_BIT(buffer.data(), 0U) == 0U);
    WRITE_BIT(buffer.data(), 0U, true);
    REQUIRE(READ_BIT(buffer.data(), 0U) != 0U);
    REQUIRE(buffer[0U] == 0x80U);

    WRITE_BIT(buffer.data(), 0U, false);
    REQUIRE(READ_BIT(buffer.data(), 0U) == 0U);
    REQUIRE(buffer[0U] == 0x00U);

    WRITE_BIT(buffer.data(), 9U, true);
    REQUIRE(READ_BIT(buffer.data(), 9U) != 0U);
    REQUIRE(buffer[1U] == 0x40U);

    WRITE_BIT(buffer.data(), 9U, false);
    REQUIRE(READ_BIT(buffer.data(), 9U) == 0U);
    REQUIRE(buffer[1U] == 0x00U);
}

TEST_CASE("BitManipulation integer macros round-trip values", "[common][bitmanipulation]")
{
    std::array<uint8_t, 16U> buffer = {};

    SET_UINT32(0x12345678U, buffer.data(), 0U);
    uint32_t value32 = GET_UINT32(buffer.data(), 0U);
    REQUIRE(value32 == 0x12345678U);
    REQUIRE(buffer[0U] == 0x12U);
    REQUIRE(buffer[1U] == 0x34U);
    REQUIRE(buffer[2U] == 0x56U);
    REQUIRE(buffer[3U] == 0x78U);

    SET_UINT24(0x00ABCDEFU, buffer.data(), 4U);
    uint32_t value24 = GET_UINT24(buffer.data(), 4U);
    REQUIRE(value24 == 0x00ABCDEFU);
    REQUIRE(buffer[4U] == 0xABU);
    REQUIRE(buffer[5U] == 0xCDU);
    REQUIRE(buffer[6U] == 0xEFU);

    SET_UINT16(0xBEEFU, buffer.data(), 8U);
    uint32_t value16 = GET_UINT16(buffer.data(), 8U);
    REQUIRE(value16 == 0xBEEFU);
    REQUIRE(buffer[8U] == 0xBEU);
    REQUIRE(buffer[9U] == 0xEFU);
}