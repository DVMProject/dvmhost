// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Test Suite
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Test Suite
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
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

TEST_CASE("CRC", "[32-bit Test]") {
    SECTION("32_Sanity_Test") {
        bool failed = false;

        INFO("CRC 32-bit CRC Test");

        srand((unsigned int)time(NULL));

        const uint32_t len = 32U;
        uint8_t* random = (uint8_t*)malloc(len);

        for (size_t i = 0; i < len - 4U; i++) {
            random[i] = rand();
        }

        CRC::addCRC32(random, len);

        uint32_t inCrc = (random[len - 4U] << 24) | (random[len - 3U] << 16) | (random[len - 2U] << 8) | (random[len - 1U] << 0);
        ::LogDebug("T", "CRC::checkCRC32(), crc = $%08X", inCrc);

        Utils::dump(2U, "32_Sanity_Test CRC", random, len);

        bool ret = CRC::checkCRC32(random, len);
        if (!ret) {
            ::LogDebug("T", "32_Sanity_Test, failed CRC32 check");
            failed = true;
            goto cleanup;
        }

        random[10U] >>= 8;
        random[11U] >>= 8;

        ret = CRC::checkCRC32(random, len);
        if (ret) {
            ::LogDebug("T", "32_Sanity_Test, failed CRC32 error check");
            failed = true;
            goto cleanup;
        }

cleanup:
        delete random;
        REQUIRE(failed==false);
    }
}
