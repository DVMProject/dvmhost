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

TEST_CASE("CRC", "[12-bit Test]") {
    SECTION("12_Sanity_Test") {
        bool failed = false;

        INFO("CRC 12-bit CRC Test");

        srand((unsigned int)time(NULL));

        const uint32_t len = 32U;
        const uint32_t lenBits = len * 8U;
        uint8_t* random = (uint8_t*)malloc(len);

        for (size_t i = 0; i < len - 2U; i++) {
            random[i] = rand();
        }

        CRC::addCRC12(random, lenBits);

        uint16_t inCrc = (random[len - 2U] << 8) | (random[len - 1U] << 0);
        ::LogDebug("T", "CRC::checkCRC12(), crc = $%04X", inCrc);

        Utils::dump(2U, "12_Sanity_Test CRC", random, len);

        bool ret = CRC::checkCRC12(random, lenBits);
        if (!ret) {
            ::LogDebug("T", "12_Sanity_Test, failed CRC12 check");
            failed = true;
            goto cleanup;
        }

        random[10U] >>= 8;
        random[11U] >>= 8;

        ret = CRC::checkCRC12(random, lenBits);
        if (ret) {
            ::LogDebug("T", "12_Sanity_Test, failed CRC12 error check");
            failed = true;
            goto cleanup;
        }

cleanup:
        delete random;
        REQUIRE(failed==false);
    }
}
