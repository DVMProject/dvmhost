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
#include "common/p25/P25Defines.h"
#include "common/p25/Crypto.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace p25::defines;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("AES_MAC_CMAC", "[AES256 MAC CMAC Test]") {
    SECTION("P25_MAC_CMAC_Crypto_Test") {
        bool failed = false;

        INFO("P25 AES256 MAC CMAC Test");

        srand((unsigned int)time(NULL));

        // example data taken from TIA-102.AACA-C-2023 Section 14.3.5.1

        // MAC TEK
        uint8_t macTek[] =
        {
            0x16, 0x85, 0x62, 0x45, 0x3B, 0x3E, 0x7F, 0x61, 0x8D, 0x68, 0xB3, 0x87, 0xE0, 0xB9, 0x97, 0xE1,
            0xFB, 0x0F, 0x26, 0x4F, 0xA8, 0x3B, 0x74, 0xE4, 0x3B, 0x17, 0x29, 0x17, 0xBD, 0x39, 0x33, 0x9F
        };

        // expected CMAC key
        uint8_t expectedCMAC[] =
        {
            0x5F, 0xB2, 0x91, 0xD0, 0x9E, 0xE3, 0x99, 0x1E, 0x13, 0x1A, 0x04, 0xB0, 0xE3, 0xA0, 0xBF, 0x58,
            0xB4, 0xA1, 0xCE, 0x46, 0x10, 0x48, 0xEB, 0x14, 0xB4, 0x97, 0xAE, 0x95, 0x22, 0xD0, 0x0D, 0x31
        };

        // data block
        uint8_t dataBlock[] = 
        {
            0x1E, 0x00, 0x4D, 0xA8, 0x64, 0x3B, 0xA8, 0x71, 0x2B, 0x1D, 0x17, 0x72, 0x00, 0x84, 0x50, 0xBC,
            0x01, 0x00, 0x01, 0x84, 0x28, 0x01, 0x00, 0x00, 0x00, 0x49, 0x83, 0x80, 0x28, 0x9C, 0xF6, 0x35,
            0xFB, 0x68, 0xD3, 0x45, 0xD3, 0x4F, 0x62, 0xEF, 0x06, 0x3B, 0xA4, 0xE0, 0x5C, 0xAE, 0x47, 0x56,
            0xE7, 0xD3, 0x04, 0x46, 0xD1, 0xF0, 0x7C, 0x6E, 0xB4, 0xE9, 0xE0, 0x84, 0x09, 0x45, 0x37, 0x23,
            0x72, 0xFB, 0x80, 0x21, 0x85, 0x22, 0x33, 0x41, 0xD9, 0x8A, 0x97, 0x08, 0x84, 0x2F, 0x62, 0x41
        };

        uint8_t expectedMAC[8U];

        Utils::dump(2U, "P25_MAC_CMAC_Crypto_Test, TEK", macTek, 32U);
        Utils::dump(2U, "P25_MAC_CMAC_Crypto_Test, DataBlock", dataBlock, 80U);
        Utils::dump(2U, "P25_MAC_CMAC_Crypto_Test, Expected CMAC Key", expectedCMAC, 32U);

        uint16_t fullLength = 0U;
        uint16_t messageLength = GET_UINT16(dataBlock, 1U);
        fullLength = messageLength + 3U;
        bool hasMN = ((dataBlock[3U] >> 4U) & 0x03U) == 0x02U;
        uint8_t macType = (dataBlock[3U] >> 2U) & 0x03U;

        ::LogInfoEx("T", "P25_MAC_CMAC_Crypto_Test, messageLength = %u, hasMN = %u, macType = $%02X", messageLength, hasMN, macType);

        switch (macType) {
        case KMM_MAC::DES_MAC:
            {
                uint8_t macLength = 4U;
                ::memset(expectedMAC, 0x00U, macLength);

                uint8_t macAlgId = dataBlock[fullLength - 4U];
                uint16_t macKId = GET_UINT16(dataBlock, fullLength - 3U);
                uint8_t macFormat = dataBlock[fullLength - 1U];

                ::memcpy(expectedMAC, dataBlock + fullLength - (macLength + 5U), macLength);

                ::LogInfoEx("T", "P25_MAC_CMAC_Crypto_Test, macAlgId = $%02X, macKId = $%04X, macFormat = $%02X", macAlgId, macKId, macFormat);
                Utils::dump(2U, "P25_MAC_CMAC_Crypto_Test, Expected MAC", expectedMAC, macLength);
            }
            break;

        case KMM_MAC::ENH_MAC:
            {
                uint8_t macLength = 8U;
                ::memset(expectedMAC, 0x00U, macLength);

                uint8_t macAlgId = dataBlock[fullLength - 4U];
                uint16_t macKId = GET_UINT16(dataBlock, fullLength - 3U);
                uint8_t macFormat = dataBlock[fullLength - 1U];

                ::memcpy(expectedMAC, dataBlock + fullLength - (macLength + 5U), macLength);

                ::LogInfoEx("T", "P25_MAC_CMAC_Crypto_Test, macAlgId = $%02X, macKId = $%04X, macFormat = $%02X", macAlgId, macKId, macFormat);
                Utils::dump(2U, "P25_MAC_CMAC_Crypto_Test, Expected MAC", expectedMAC, macLength);
            }
            break;

        case KMM_MAC::NO_MAC:
            break;

        default:
            ::LogError(LOG_P25, "P25_MAC_CMAC_Crypto_Test, unknown KMM MAC inventory type value, macType = $%02X", macType);
            break;
        }

        p25::crypto::P25Crypto crypto;

        UInt8Array macKey = crypto.cryptAES_KMM_CMAC_KDF(macTek, dataBlock, fullLength, true);
        Utils::dump(2U, "P25_MAC_CMAC_Crypto_Test, CMAC MAC Key", macKey.get(), 32U);

        for (uint32_t i = 0; i < 32U; i++) {
            if (macKey[i] != expectedCMAC[i]) {
                ::LogError("T", "P25_MAC_CMAC_Crypto_Test, INVALID AT IDX %d", i);
                failed = true;
            }
        }

        UInt8Array mac = crypto.cryptAES_KMM_CMAC(expectedCMAC/* macKey.get()*/, dataBlock, fullLength);
        Utils::dump(2U, "P25_MAC_CMAC_Crypto_Test, MAC", mac.get(), 8U);

        for (uint32_t i = 0; i < 8U; i++) {
            if (mac[i] != expectedMAC[i]) {
                ::LogError("T", "P25_MAC_CMAC_Crypto_Test, INVALID AT IDX %d", i);
                failed = true;
            }
        }

        REQUIRE(failed==false);
    }
}
