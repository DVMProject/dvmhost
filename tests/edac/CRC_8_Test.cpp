/**
* Digital Voice Modem - Host Software (Test Suite)
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software / Test Suite
*
*/
/*
*   Copyright (C) 2023 Bryan Biedenkapp N2PLL
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
#include "Defines.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

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
