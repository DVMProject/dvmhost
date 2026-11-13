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
#include "common/Utils.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>

TEST_CASE("Utils string helpers convert case and format values", "[common][utils]")
{
    REQUIRE(strtolower("AbC123!?") == "abc123!?");
    REQUIRE(strtoupper("AbC123!?") == "ABC123!?");
    REQUIRE(__BOOL_STR(true) == "true");
    REQUIRE(__BOOL_STR(false) == "false");
    REQUIRE(__INT_STR(-42) == "-42");
    REQUIRE(__INT_HEX_STR(42) == "2a");
    REQUIRE(__FLOAT_STR(3.5F) == "3.5");
}

TEST_CASE("Utils IP helpers convert to and from packed values", "[common][utils]")
{
    REQUIRE(__IP_FROM_UINT(0x7F000001U) == "127.0.0.1");
    REQUIRE(__IP_FROM_STR("192.168.1.20") == 0xC0A80114U);
    REQUIRE(__IP_FROM_STR("10.0.0.1") == 0x0A000001U);
}

TEST_CASE("Utils endian helpers reverse byte order", "[common][utils]")
{
    REQUIRE(Utils::reverseEndian(uint16_t(0x1234U)) == 0x3412U);
    REQUIRE(Utils::reverseEndian(uint32_t(0x12345678U)) == 0x78563412U);
    REQUIRE(Utils::reverseEndian(uint64_t(0x0123456789ABCDEFULL)) == 0xEFCDAB8967452301ULL);
}

TEST_CASE("Utils bit helpers round-trip bit buffers", "[common][utils]")
{
    std::array<uint8_t, 4U> source = {0xA5U, 0x5AU, 0xFFU, 0x00U};
    std::array<uint8_t, 4U> target = {};

    REQUIRE(Utils::getBits(source.data(), target.data(), 0U, 16U) == 16U);
    REQUIRE(target[0U] == 0xA5U);
    REQUIRE(target[1U] == 0x5AU);

    std::array<uint8_t, 4U> shifted = {};
    REQUIRE(Utils::setBits(target.data(), shifted.data(), 8U, 24U) == 16U);
    REQUIRE(shifted[1U] == 0xA5U);
    REQUIRE(shifted[2U] == 0x5AU);

    std::array<uint8_t, 4U> ranged = {};
    REQUIRE(Utils::getBitRange(source.data(), ranged.data(), 8U, 16U) == 16U);
    REQUIRE(ranged[0U] == 0x5AU);
    REQUIRE(ranged[1U] == 0xFFU);

    std::array<uint8_t, 4U> back = {};
    REQUIRE(Utils::setBitRange(ranged.data(), back.data(), 4U, 16U) == 16U);

    std::array<uint8_t, 4U> verify = {};
    REQUIRE(Utils::getBitRange(back.data(), verify.data(), 4U, 16U) == 16U);
    REQUIRE(verify[0U] == ranged[0U]);
    REQUIRE(verify[1U] == ranged[1U]);
}

TEST_CASE("Utils binary helpers convert between 6-bit and bit buffers", "[common][utils]")
{
    std::array<uint8_t, 2U> bits = {};
    Utils::hex2Bin(0x2DU, bits.data(), 0U);

    REQUIRE(bits[0U] == 0xB4U);
    REQUIRE(Utils::bin2Hex(bits.data(), 0U) == 0x2DU);

    std::array<uint8_t, 2U> shifted = {};
    Utils::hex2Bin(0x15U, shifted.data(), 3U);
    REQUIRE(Utils::bin2Hex(shifted.data(), 3U) == 0x15U);
}

TEST_CASE("Utils bit count helpers return population counts", "[common][utils]")
{
    REQUIRE(Utils::countBits8(0x00U) == 0U);
    REQUIRE(Utils::countBits8(0xFFU) == 8U);
    REQUIRE(Utils::countBits32(0xF0F0F0F0U) == 16U);
    REQUIRE(Utils::countBits64(0xFFFF0000FFFF0000ULL) == 32U);
}