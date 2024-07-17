// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022 Natalie Moore
 *
 */
#include "Defines.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("NullTest", "[NullTest]") {
    SECTION("Null") {
        REQUIRE(1==1);
    }
}
