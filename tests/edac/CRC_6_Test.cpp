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

TEST_CASE("CRC 6-bit Test", "[crc][6bit]") {
    bool failed = false;

    INFO("CRC 6-bit CRC Test");

    srand((unsigned int)time(NULL));

    const uint32_t payloadLen = 32U;
    const uint32_t lenBits = payloadLen * 8U;
    const uint32_t totalLen = payloadLen + 1U; // spare byte stores 6-bit CRC at bit offset lenBits
    uint8_t* random = (uint8_t*)calloc(totalLen, sizeof(uint8_t));

    for (size_t i = 0; i < payloadLen; i++) {
        random[i] = rand();
    }

    CRC::addCRC6(random, lenBits);

    uint32_t inCrc = (random[totalLen - 1U] << 0);
    ::LogInfoEx("T", "CRC::checkCRC6(), crc = $%02X", inCrc);

    Utils::dump(2U, "6_Sanity_Test CRC", random, totalLen);

    bool ret = CRC::checkCRC6(random, lenBits);
    if (!ret) {
        ::LogError("T", "6_Sanity_Test, failed CRC6 check");
        failed = true;
        goto cleanup;
    }

    random[10U] ^= 0x01U;
    random[11U] ^= 0x01U;

    ret = CRC::checkCRC6(random, lenBits);
    if (ret) {
        ::LogError("T", "6_Sanity_Test, failed CRC6 error check");
        failed = true;
        goto cleanup;
    }

cleanup:
    free(random);
    REQUIRE(failed==false);
}
