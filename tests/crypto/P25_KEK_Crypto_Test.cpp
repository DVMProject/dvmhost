// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "host/Defines.h"
#include "common/p25/Crypto.h"
#include "common/Log.h"
#include "common/Utils.h"

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("AES_KEK", "[KEK Crypto Test]") {
    SECTION("P25_AES_KEK_Crypto_Test") {
        bool failed = false;

        INFO("P25 AES256 KEK Crypto Test");

        srand((unsigned int)time(NULL));

        // example data taken from TIA-102.AACA-C-2023 Section 14.3.3

        // Encrypted Key Frame
        uint8_t testWrappedKeyFrame[40U] = 
        {
            0x80, 0x28, 0x9C, 0xF6, 0x35, 0xFB, 0x68, 0xD3, 0x45, 0xD3, 0x4F, 0x62, 0xEF, 0x06, 0x3B, 0xA4,
            0xE0, 0x5C, 0xAE, 0x47, 0x56, 0xE7, 0xD3, 0x04, 0x46, 0xD1, 0xF0, 0x7C, 0x6E, 0xB4, 0xE9, 0xE0,
            0x84, 0x09, 0x45, 0x37, 0x23, 0x72, 0xFB, 0x80
        };

        // key (K)
        uint8_t K[32U] = 
        {
            0x49, 0x40, 0x02, 0xBF, 0x16, 0x31, 0x32, 0xA4, 0x21, 0xFB, 0xEF, 0x11, 0x7F, 0x98, 0x5A, 0x0C,
            0xAA, 0xDD, 0xC2, 0x50, 0xA4, 0xC2, 0x19, 0x47, 0xD5, 0x93, 0xE6, 0xC0, 0x67, 0xDE, 0x40, 0x2C
        };

        // message
        uint8_t message[32U] = 
        {
            0x2A, 0x19, 0x38, 0xCD, 0x0B, 0x6B, 0x6B, 0xD0, 0xB7, 0x74, 0x56, 0x92, 0xFE, 0x19, 0x14, 0xF0,
            0x38, 0x76, 0x61, 0x2F, 0xC2, 0x9D, 0x57, 0x77, 0x89, 0xA6, 0x2F, 0x65, 0xFA, 0x05, 0xEF, 0x83
        };

        Utils::dump(2U, "KEK_Crypto_Test, Key", K, 32);
        Utils::dump(2U, "KEK_Crypto_Test, Message", message, 32);

        p25::crypto::P25Crypto crypto;

        UInt8Array wrappedKey = crypto.cryptAES_TEK(K, message, 32U);

        Utils::dump(2U, "KEK_Crypto_Test, Wrapped", wrappedKey.get(), 40);

        for (uint32_t i = 0; i < 40U; i++) {
            if (wrappedKey[i] != testWrappedKeyFrame[i]) {
                ::LogDebug("T", "P25_AES_KEK_Crypto_Test, WRAPPED INVALID AT IDX %d", i);
                failed = true;
            }
        }

        UInt8Array unwrappedKey = crypto.decryptAES_TEK(K, wrappedKey.get(), 40U);

        Utils::dump(2U, "KEK_Crypto_Test, Unwrapped", unwrappedKey.get(), 40);

        for (uint32_t i = 0; i < 32U; i++) {
            if (unwrappedKey[i] != message[i]) {
                ::LogError("T", "P25_AES_KEK_Crypto_Test, UNWRAPPED INVALID AT IDX %d", i);
                failed = true;
            }
        }

        REQUIRE(failed==false);
    }
}
