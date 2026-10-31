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
#include "common/Timer.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Timer defaults to stopped with no timeout", "[common][timer]")
{
    Timer timer;

    REQUIRE(timer.getTimeout() == 0U);
    REQUIRE(timer.getTimer() == 0U);
    REQUIRE(timer.getRemaining() == 0U);
    REQUIRE_FALSE(timer.isRunning());
    REQUIRE_FALSE(timer.hasExpired());
    REQUIRE_FALSE(timer.isPaused());
}

TEST_CASE("Timer counts ticks and expires at the configured timeout", "[common][timer]")
{
    Timer timer(1000U, 1U, 500U);

    REQUIRE(timer.getTimeout() == 1U);
    REQUIRE_FALSE(timer.isRunning());

    timer.start();
    REQUIRE(timer.isRunning());
    REQUIRE_FALSE(timer.hasExpired());
    REQUIRE(timer.getTimer() == 0U);

    timer.clock(500U);
    REQUIRE_FALSE(timer.hasExpired());
    REQUIRE(timer.getTimer() == 0U);
    REQUIRE(timer.getRemaining() == 1U);

    timer.clock(1000U);
    REQUIRE(timer.hasExpired());
    REQUIRE(timer.getTimer() == 1U);
    REQUIRE(timer.getRemaining() == 0U);
}

TEST_CASE("Timer pause resume stop and reset behave consistently", "[common][timer]")
{
    Timer timer(1000U, 2U, 0U);

    timer.start();
    timer.clock(1000U);
    REQUIRE_FALSE(timer.hasExpired());

    timer.pause();
    REQUIRE(timer.isPaused());
    timer.clock(2000U);
    REQUIRE(timer.getTimer() == 1U);

    timer.resume();
    REQUIRE_FALSE(timer.isPaused());
    timer.clock(1000U);
    REQUIRE(timer.hasExpired());

    timer.stop();
    REQUIRE_FALSE(timer.isRunning());
    REQUIRE_FALSE(timer.hasExpired());
    REQUIRE(timer.getTimer() == 0U);

    timer.setTimeout(0U, 0U);
    REQUIRE(timer.getTimeout() == 0U);
    REQUIRE(timer.getRemaining() == 0U);
}