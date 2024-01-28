// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Test Suite
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Test Suite
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
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

TEST_CASE("AES", "[Crypto Test]") {
    SECTION("AES_Crypto_Test") {
        bool failed = false;

        INFO("AES Crypto Test");

        srand((unsigned int)time(NULL));

        // key (K)
        uint8_t K[32] = 
        {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
        };

        // message
        uint8_t message[48] = 
        {
            0x90, 0x56, 0x00, 0x00, 0x2D, 0x75, 0xE6, 0x8D, 0x00, 0x89, 0x69, 0xCF, 0x00, 0xFE, 0x00, 0x04,
            0x4F, 0xC7, 0x60, 0xFF, 0x30, 0x3E, 0x2B, 0xAD, 0x00, 0x89, 0x69, 0xCF, 0x00, 0x00, 0x00, 0x08,
            0x52, 0x50, 0x54, 0x4C, 0x00, 0x89, 0x69, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00            
        };

        // perform crypto
        AES* aes = new AES(AESKeyLength::AES_256);

        Utils::dump(2U, "AES_Crypto_Test, Message", message, 48);

        uint8_t* crypted = aes->encryptECB(message, 48 * sizeof(uint8_t), K);
        Utils::dump(2U, "AES_Crypto_Test, Encrypted", crypted, 48);

        uint8_t* decrypted = aes->decryptECB(crypted, 48 * sizeof(uint8_t), K);
        Utils::dump(2U, "AES_Crypto_Test, Decrypted", decrypted, 48);

        for (uint32_t i = 0; i < 48U; i++) {
            if (decrypted[i] != message[i]) {
                ::LogDebug("T", "AES_Crypto_Test, INVALID AT IDX %d\n", i);
                failed = true;
            }
        }

        delete aes;
        REQUIRE(failed==false);
    }
}
