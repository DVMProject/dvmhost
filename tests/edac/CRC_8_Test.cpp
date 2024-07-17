// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/edac/CRC.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace edac;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("CRC", "[8-bit Test]") {
    SECTION("8_Sanity_Test") {
        bool failed = false;

        INFO("CRC 8-bit CRC Test");

        srand((unsigned int)time(NULL));

        const uint32_t len = 32U;
        uint8_t* random = (uint8_t*)malloc(len);

        for (size_t i = 0; i < len; i++) {
            random[i] = rand();
        }

        uint8_t crc = CRC::crc8(random, len);
        ::LogDebug("T", "crc = %02X", crc);

        Utils::dump(2U, "8_Sanity_Test CRC", random, len);

        random[10U] >>= 8;
        random[11U] >>= 8;

        uint8_t calc = CRC::crc8(random, len);
        ::LogDebug("T", "calc = %02X", calc);
        if (crc == calc) {
            ::LogDebug("T", "8_Sanity_Test, failed CRC8 error check");
            failed = true;
            goto cleanup;
        }

cleanup:
        delete random;
        REQUIRE(failed==false);
    }
}
