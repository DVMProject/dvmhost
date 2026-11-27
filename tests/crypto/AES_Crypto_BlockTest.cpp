// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
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

TEST_CASE("AES Crypto Block Test", "[aes][crypto_blocktest]") {
    bool failed = false;

    INFO("AES Crypto Block Test");

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
    uint8_t message[32] = 
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    // perform crypto
    AES* aes = new AES(AESKeyLength::AES_256);

    Utils::dump(2U, "AES_Crypto_BlockTest, Message", message, 32);

    uint8_t* crypted = aes->encryptECB(message, 32 * sizeof(uint8_t), K);
    Utils::dump(2U, "AES_Crypto_BlockTest, Encrypted", crypted, 32);

    uint8_t* decrypted = aes->decryptECB(crypted, 32 * sizeof(uint8_t), K);
    Utils::dump(2U, "AES_Crypto_BlockTest, Decrypted", decrypted, 32);

    for (uint32_t i = 0; i < 32U; i++) {
        if (decrypted[i] != message[i]) {
            ::LogError("T", "AES_Crypto_BlockTest, INVALID AT IDX %d", i);
            failed = true;
        }
    }

    delete aes;
    REQUIRE(failed==false);
}
