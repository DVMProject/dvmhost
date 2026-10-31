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
#include "common/Clock.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

using namespace system_clock;

TEST_CASE("Clock conversion helpers are consistent", "[common][clock]")
{
    REQUIRE(msToJiffies(0U) == 0U);
    REQUIRE(msToJiffies(1000U) == 65536U);
    REQUIRE(msToJiffies(250U) == 16384U);
    REQUIRE(jiffiesToMs(0U) == 0U);
    REQUIRE(jiffiesToMs(65536U) == 1000U);
    REQUIRE(jiffiesToMs(16384U) == 250U);
}

TEST_CASE("Clock HRC helpers report elapsed time", "[common][clock]")
{
    auto start = hrc::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto end = hrc::now();

    REQUIRE(hrc::diff(end, start) >= 10U);
    REQUIRE(hrc::diffNow(start) >= 10U);

    auto startUS = hrc::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(hrc::diffNowUS(startUS) >= 10000U);
}

TEST_CASE("Clock NTP helpers are self-consistent", "[common][clock]")
{
    uint64_t now = ntp::now();
    REQUIRE(ntp::diff(now, now) == 0U);
    REQUIRE(ntp::diffNow(now) < 1000U);
}