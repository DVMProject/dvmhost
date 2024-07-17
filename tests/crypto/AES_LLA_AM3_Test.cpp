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
#include "common/AESCrypto.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace crypto;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("AES", "[LLA AM3 Test]") {
    SECTION("LLA_AM3_Test") {
        bool failed = false;

        INFO("AES P25 LLA AM3 Test");

        /*
        ** TIA-102.AACE-A 6.6 AM3 Sample
        */

        srand((unsigned int)time(NULL));

        // key (K)
        uint8_t K[16] = 
        {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
        };

        // result KS
        uint8_t resultKS[16] = 
        {
            0x69, 0xD5, 0xDC, 0x08, 0x02, 0x3C, 0x46, 0x52,
            0xCC, 0x71, 0xD5, 0xCD, 0x1E, 0x74, 0xE1, 0x04
        };

        // RS
        uint8_t RS[10] = 
        {
            0x38, 0xAE, 0xC8, 0x29, 0x33, 0xB1, 0x7F, 0x80,
            0x24, 0x9D
        };

        // expand RS to 16 bytes
        uint8_t expandedRS[16];
        for (uint32_t i = 0; i < 16U; i++)
            expandedRS[i] = 0x00U;
        for (uint32_t i = 0; i < 10U; i++)
            expandedRS[i] = RS[i];

        Utils::dump(2U, "LLA_AM3_Test, Expanded RS", expandedRS, 16);

        // complement RS
        uint8_t complementRS[16];
        for (uint32_t i = 0; i < 16U; i++)
            complementRS[i] = ~expandedRS[i];

        Utils::dump(2U, "LLA_AM3_Test, Complement RS", complementRS, 16);

        // perform crypto
        AES* aes = new AES(AESKeyLength::AES_128);

        uint8_t* KS = aes->encryptECB(complementRS, 16 * sizeof(uint8_t), K);

        Utils::dump(2U, "LLA_AM3_Test, Const Result", resultKS, 16);
        Utils::dump(2U, "LLA_AM3_Test, Result", KS, 16);

        for (uint32_t i = 0; i < 16U; i++) {
            if (KS[i] != resultKS[i]) {
                ::LogDebug("T", "LLA_AM3_Test, INVALID AT IDX %d\n", i);
                failed = true;
            }
        }

        delete aes;
        REQUIRE(failed==false);
    }
}
