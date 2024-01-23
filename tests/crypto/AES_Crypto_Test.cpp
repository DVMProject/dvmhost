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
