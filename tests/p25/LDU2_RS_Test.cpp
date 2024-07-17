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
using namespace p25::defines;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("LDU2", "[Reed-Soloman 24,16,9 Test]") {
    SECTION("RS_24169_Test") {
        bool failed = false;

        INFO("P25 LDU2 RS (24,16,9) FEC Test");

        srand((unsigned int)time(NULL));
        RS634717 m_rs = RS634717();

        uint8_t* random = (uint8_t*)malloc(15U);

        for (size_t i = 0; i < 15U; i++) {
            random[i] = rand();
        }

        // LDU2 Encode
        uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
        ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

        for (uint32_t i = 0; i < 12U; i++)
            rs[i] = random[i];
        rs[11U] = 0xF0U;

        Utils::dump(2U, "LC::encodeLDU2(), LDU2", rs, P25_LDU_LC_FEC_LENGTH_BYTES);

        // encode RS (24,16,9) FEC
        m_rs.encode24169(rs);

        Utils::dump(2U, "LC::encodeLDU2(), LDU2 RS", rs, P25_LDU_LC_FEC_LENGTH_BYTES);

        // LDU2 Decode
        rs[9U] >>= 4;
        rs[10U] >>= 4;

        Utils::dump(2U, "LC::decodeLDU2(), LDU RS (errors injected)", rs, P25_LDU_LC_FEC_LENGTH_BYTES);

        // decode RS (24,16,9) FEC
        try {
            bool ret = m_rs.decode24169(rs);
            if (!ret) {
                ::LogDebug("T", "LC::decodeLDU2(), failed to decode RS (24,16,9) FEC\n");
                failed = true;
                goto cleanup;
            }
        }
        catch (...) {
            Utils::dump(2U, "P25, RS excepted with input data", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
            failed = true;
            goto cleanup;
        }

        Utils::dump(2U, "LC::decodeLDU2(), LDU2", rs, P25_LDU_LC_FEC_LENGTH_BYTES);

        for (uint32_t i = 0; i < 12U; i++) {
            if (i == 11U) {
                if (rs[i] != 0xF0U) {
                    ::LogDebug("T", "LC::decodeLDU2(), UNCORRECTABLE AT IDX %d\n", i);
                    failed = true;
                }
            }
            else {
                if (rs[i] != random[i]) {
                    ::LogDebug("T", "LC::decodeLDU2(), UNCORRECTABLE AT IDX %d\n", i);
                    failed = true;
                }
            }
        }

cleanup:
        delete random;
        REQUIRE(failed==false);
    }
}
