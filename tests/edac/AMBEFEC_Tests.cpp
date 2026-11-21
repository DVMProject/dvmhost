// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include "common/edac/AMBEFEC.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>

using namespace edac;

TEST_CASE("AMBEFEC regenerates a clean P25 IMBE frame", "[edac][ambefec][p25]")
{
    AMBEFEC fec;
    std::array<uint8_t, 18U> frame{};
    fec.regenerateIMBE(frame.data());
    REQUIRE(fec.regenerateIMBE(frame.data()) == 0U);
    REQUIRE(fec.measureP25BER(frame.data()) == 0U);
}

TEST_CASE("AMBEFEC corrects isolated P25 IMBE bit errors", "[edac][ambefec][p25]")
{
    AMBEFEC fec;
    std::array<uint8_t, 18U> clean{};
    fec.regenerateIMBE(clean.data());
    REQUIRE(fec.regenerateIMBE(clean.data()) == 0U);

    for (uint32_t bit = 0U; bit < 144U; bit += 17U) {
        std::array<uint8_t, 18U> frame = clean;
        frame[bit >> 3U] ^= static_cast<uint8_t>(1U << (7U - (bit & 7U)));
        fec.regenerateIMBE(frame.data());
        REQUIRE(std::memcmp(frame.data(), clean.data(), frame.size()) == 0);
    }
}