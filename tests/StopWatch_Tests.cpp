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
#include "common/StopWatch.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

TEST_CASE("StopWatch time increases over time", "[common][stopwatch]")
{
    StopWatch stopWatch;
    uint64_t first = stopWatch.time();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    uint64_t second = stopWatch.time();

    REQUIRE(second >= first);
}

TEST_CASE("StopWatch measures elapsed time after start", "[common][stopwatch]")
{
    StopWatch stopWatch;
    REQUIRE(stopWatch.start() >= 0U);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    uint32_t elapsed = stopWatch.elapsed();
    REQUIRE(elapsed >= 10U);
    REQUIRE(elapsed < 1000U);
}