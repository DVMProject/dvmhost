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
#include "host/Defines.h"
#include "common/AESCrypto.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace crypto;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("AES", "[LLA AM4 Test]") {
    SECTION("LLA_AM4_Test") {
        bool failed = false;

        INFO("AES P25 LLA AM4 Test");

        /*
        ** TIA-102.AACE-A 6.6 AM4 Sample
        */

        srand((unsigned int)time(NULL));

        // key (KS)
        uint8_t KS[16] = 
        {
            0x69, 0xD5, 0xDC, 0x08, 0x02, 0x3C, 0x46, 0x52,
            0xCC, 0x71, 0xD5, 0xCD, 0x1E, 0x74, 0xE1, 0x04
        };

        // RES2
        uint8_t resultRES2[4] = 
        {
            0xB3, 0xAD, 0x16, 0xE1
        };

        // RAND2
        uint8_t RAND2[5] = 
        {
            0x6E, 0x78, 0x4F, 0x75, 0xBD
        };

        // expand RAND2 to 16 bytes
        uint8_t expandedRAND2[16];
        for (uint32_t i = 0; i < 16U; i++)
            expandedRAND2[i] = 0x00U;
        for (uint32_t i = 0; i < 5U; i++)
            expandedRAND2[i] = RAND2[i];

        // perform crypto
        AES* aes = new AES(AESKeyLength::AES_128);

        uint8_t* aesOut = aes->encryptECB(expandedRAND2, 16 * sizeof(uint8_t), KS);

        // reduce AES output
        uint8_t RES2[4];
        for (uint32_t i = 0; i < 4U; i++)
            RES2[i] = aesOut[i];

        Utils::dump(2U, "LLA_AM4_Test, Const Result", resultRES2, 4);
        Utils::dump(2U, "LLA_AM4_Test, AES Out", aesOut, 16);
        Utils::dump(2U, "LLA_AM4_Test, Result", RES2, 4);

        for (uint32_t i = 0; i < 4U; i++) {
            if (RES2[i] != resultRES2[i]) {
                ::LogDebug("T", "LLA_AM4_Test, INVALID AT IDX %d\n", i);
                failed = true;
            }
        }

        delete aes;
        REQUIRE(failed==false);
    }
}
