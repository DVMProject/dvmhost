/**
* Digital Voice Modem - Host Software (Test Suite)
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software / Test Suite
*
*/
/*
*   Copyright (C) 2022 by Natalie Moore <https://github.com/jelimoore>
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

TEST_CASE("LDU1", "[Reed-Soloman 24,12,13 Test]") {
    SECTION("RS_241213_Test") {
        bool failed = false;

        INFO("P25 LDU1 RS (24,12,13) FEC Test");

        srand((unsigned int)time(NULL));
        RS634717 m_rs = RS634717();

        uint8_t* random = (uint8_t*)malloc(15U);

        for (size_t i = 0; i < 15U; i++) {
            random[i] = rand();
        }

        // LDU1 Encode
        uint8_t rs[P25_LDU_LC_LENGTH_BYTES];
        ::memset(rs, 0x00U, P25_LDU_LC_LENGTH_BYTES);

        for (uint32_t i = 0; i < 9U; i++)
            rs[i] = random[i];
        rs[8U] = 0xF0U;

        Utils::dump(2U, "LC::encodeLDU1(), LDU1", rs, P25_LDU_LC_LENGTH_BYTES);

        // encode RS (24,12,13) FEC
        m_rs.encode241213(rs);

        Utils::dump(2U, "LC::encodeLDU1(), LDU1 RS", rs, P25_LDU_LC_LENGTH_BYTES);

        // LDU1 Decode
        rs[6U] >>= 8;
        rs[7U] >>= 8;
        rs[8U] >>= 8;

        Utils::dump(2U, "LC::encodeLDU1(), LDU RS (errors injected)", rs, P25_LDU_LC_LENGTH_BYTES);

        // decode RS (24,12,13) FEC
        try {
            bool ret = m_rs.decode241213(rs);
            if (!ret) {
                ::LogDebug("T", "LC::decodeLDU1(), failed to decode RS (24,12,13) FEC\n");
                failed = true;
                goto cleanup;
            }
        }
        catch (...) {
            Utils::dump(2U, "P25, RS excepted with input data", rs, P25_LDU_LC_LENGTH_BYTES);
            failed = true;
            goto cleanup;
        }

        Utils::dump(2U, "LC::decodeLDU1(), LDU1", rs, P25_LDU_LC_LENGTH_BYTES);

        for (uint32_t i = 0; i < 9U; i++) {
            if (i == 8U) {
                if (rs[i] != 0xF0U) {
                    ::LogDebug("T", "LC::decodeLDU1(), UNCORRECTABLE AT IDX %d\n", i);
                    failed = true;
                }
            }
            else {
                if (rs[i] != random[i]) {
                    ::LogDebug("T", "LC::decodeLDU1(), UNCORRECTABLE AT IDX %d\n", i);
                    failed = true;
                }
            }
        }

cleanup:
        delete random;
        REQUIRE(failed==false);
    }
}
