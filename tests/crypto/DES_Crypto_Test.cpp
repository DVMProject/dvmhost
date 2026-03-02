// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/DESCrypto.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace crypto;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("DES Crypto Test", "[des][crypto_test]") {
    bool failed = false;

    INFO("DES Crypto Test");

    srand((unsigned int)time(NULL));

    // key (K) - DES uses 8-byte (64-bit) keys
    uint8_t K[8] = 
    {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
    };

    // message - DES operates on 8-byte blocks
    uint8_t message[8] = 
    {
        0x90, 0x56, 0x00, 0x00, 0x2D, 0x75, 0xE6, 0x8D
    };

    // perform crypto
    DES* des = new DES();

    Utils::dump(2U, "DES_Crypto_Test, Message", message, 8);

    uint8_t* crypted = des->encryptBlock(message, K);
    Utils::dump(2U, "DES_Crypto_Test, Encrypted", crypted, 8);

    uint8_t* decrypted = des->decryptBlock(crypted, K);
    Utils::dump(2U, "DES_Crypto_Test, Decrypted", decrypted, 8);

    for (uint32_t i = 0; i < 8U; i++) {
        if (decrypted[i] != message[i]) {
            ::LogError("T", "DES_Crypto_Test, INVALID AT IDX %d", i);
            failed = true;
        }
    }

    delete des;
    REQUIRE(failed==false);
}
