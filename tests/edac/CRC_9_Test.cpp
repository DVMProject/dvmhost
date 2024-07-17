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

TEST_CASE("CRC", "[9-bit Test]") {
    SECTION("9_Sanity_Test") {
        bool failed = false;

        INFO("CRC 9-bit CRC Test");

        srand((unsigned int)time(NULL));

        const uint32_t len = 18U;
        uint8_t* random = (uint8_t*)malloc(len);

        for (size_t i = 0; i < len; i++) {
            random[i] = rand();
        }

        random[0U] = 0;
        random[1U] = 0;

        uint16_t crc = edac::CRC::createCRC9(random, 144U);
        ::LogDebug("T", "crc = %04X", crc);

        random[0U] = random[0U] + ((crc >> 8) & 0x01U);
        random[1U] = (crc & 0xFFU);

        Utils::dump(2U, "9_Sanity_Test CRC", random, len);

        random[10U] >>= 8;
        random[11U] >>= 8;

        uint16_t calculated = edac::CRC::createCRC9(random, 144U);
        if (((crc ^ calculated) == 0)/*|| ((crc ^ calculated) == 0x1FFU)*/) {
            ::LogDebug("T", "9_Sanity_Test, failed CRC9 error check");
            failed = true;
            goto cleanup;
        }

cleanup:
        delete random;
        REQUIRE(failed==false);
    }
}
