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
#include "common/edac/RS634717.h"
#include "common/p25/P25Defines.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace edac;
using namespace p25;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("HDU", "[Reed-Soloman 36,20,17 Test]") {
    SECTION("RS_362017_Test") {
        bool failed = false;

        INFO("P25 HDU RS (36,20,17) FEC Test");

        srand((unsigned int)time(NULL));
        RS634717 m_rs = RS634717();

        uint8_t* random = (uint8_t*)malloc(15U);

        for (size_t i = 0; i < 15U; i++) {
            random[i] = rand();
        }

        // HDU Encode
        uint8_t rs[P25_HDU_LENGTH_BYTES];
        ::memset(rs, 0x00U, P25_HDU_LENGTH_BYTES);

        for (uint32_t i = 0; i < 15U; i++)
            rs[i] = random[i];
        rs[14U] = 0xF0U;

        Utils::dump(2U, "LC::encodeHDU(), HDU", rs, P25_HDU_LENGTH_BYTES);

        // encode RS (36,20,17) FEC
        m_rs.encode362017(rs);

        Utils::dump(2U, "LC::encodeHDU(), HDU RS", rs, P25_HDU_LENGTH_BYTES);

        // HDU Decode
        rs[9U] >>= 8;
        rs[10U] >>= 8;
        rs[11U] >>= 8;
        rs[12U] >>= 8;
        rs[13U] >>= 8;

        Utils::dump(2U, "LC::decodeHDU(), HDU RS (errors injected)", rs, P25_HDU_LENGTH_BYTES);

        // decode RS (36,20,17) FEC
        try {
            bool ret = m_rs.decode362017(rs);
            if (!ret) {
                ::LogDebug("T", "LC::decodeHDU(), failed to decode RS (36,20,17) FEC\n");
                failed = true;
                goto cleanup;
            }
        }
        catch (...) {
            Utils::dump(2U, "P25, RS excepted with input data", rs, P25_HDU_LENGTH_BYTES);
            failed = true;
            goto cleanup;
        }

        Utils::dump(2U, "LC::decodeHDU(), HDU", rs, P25_HDU_LENGTH_BYTES);

        for (uint32_t i = 0; i < 15U; i++) {
            if (i == 14U) {
                if (rs[i] != 0xF0U) {
                    ::LogDebug("T", "LC::decodeHDU(), UNCORRECTABLE AT IDX %d\n", i);
                    failed = true;
                }
            }
            else {
                if (rs[i] != random[i]) {
                    ::LogDebug("T", "LC::decodeHDU(), UNCORRECTABLE AT IDX %d\n", i);
                    failed = true;
                }
            }
        }

cleanup:
        delete random;
        REQUIRE(failed==false);
    }
}
