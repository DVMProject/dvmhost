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
#include "common/p25/kmm/KMMRekeyCommand.h"
#include "common/p25/kmm/KMMHello.h"
#include "common/Log.h"
#include "common/Utils.h"

using namespace p25;
using namespace p25::crypto;
using namespace p25::defines;
using namespace p25::kmm;

#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <time.h>

TEST_CASE("KMM ReKey Command CMAC Test", "[p25][kmm_cmac][cap]") {
    bool failed = false;

    INFO("P25 KMM ReKey Test");

    srand((unsigned int)time(NULL));

    // MAC TEK
    uint8_t macTek[] =
    {
        0x16, 0x85, 0x62, 0x45, 0x3B, 0x3E, 0x7F, 0x61, 0x8D, 0x68, 0xB3, 0x87, 0xE0, 0xB9, 0x97, 0xE1,
        0xFB, 0x0F, 0x26, 0x4F, 0xA8, 0x3B, 0x74, 0xE4, 0x3B, 0x17, 0x29, 0x17, 0xBD, 0x39, 0x33, 0x9F
    };

    // data block
    uint8_t dataBlock[] = 
    {
        0x1E, 0x00, 0x4D, 0xA8, 0x64, 0x3B, 0xA8, 0x71, 0x2B, 0x1D, 0x17, 0x72, 0x00, 0x84, 0x50, 0xBC,
        0x01, 0x00, 0x01, 0x84, 0x28, 0x01, 0x00, 0x00, 0x00, 0x49, 0x83, 0x80, 0x28, 0x9C, 0xF6, 0x35,
        0xFB, 0x68, 0xD3, 0x45, 0xD3, 0x4F, 0x62, 0xEF, 0x06, 0x3B, 0xA4, 0xE0, 0x5C, 0xAE, 0x47, 0x56,
        0xE7, 0xD3, 0x04, 0x46, 0xD1, 0xF0, 0x7C, 0x6E, 0xB4, 0xE9, 0xE0, 0x84, 0x09, 0x45, 0x37, 0x23,
        0x72, 0xFB, 0x80, 0xA2, 0xC1, 0xCC, 0xD1, 0x42, 0x01, 0x73, 0x8C, 0x08, 0x84, 0x2F, 0x62, 0x41
    };

    // Encrypted Key Frame
    uint8_t testWrappedKeyFrame[40U] = 
    {
        0x80, 0x28, 0x9C, 0xF6, 0x35, 0xFB, 0x68, 0xD3, 0x45, 0xD3, 0x4F, 0x62, 0xEF, 0x06, 0x3B, 0xA4,
        0xE0, 0x5C, 0xAE, 0x47, 0x56, 0xE7, 0xD3, 0x04, 0x46, 0xD1, 0xF0, 0x7C, 0x6E, 0xB4, 0xE9, 0xE0,
        0x84, 0x09, 0x45, 0x37, 0x23, 0x72, 0xFB, 0x80
    };

    Utils::dump(2U, "P25_KMM_ReKey_CMAC_Test, DataBlock", dataBlock, 80U);

    KMMRekeyCommand outKmm = KMMRekeyCommand();

    outKmm.setDecryptInfoFmt(KMM_DECRYPT_INSTRUCT_NONE);
    outKmm.setSrcLLId(0x712B1DU);
    outKmm.setDstLLId(0x643BA8U);

    outKmm.setMACType(KMM_MAC::ENH_MAC);
    outKmm.setMACAlgId(ALGO_AES_256);
    outKmm.setMACKId(0x2F62U);
    outKmm.setMACFormat(KMM_MAC_FORMAT_CMAC);

    outKmm.setMessageNumber(0x1772U);

    outKmm.setAlgId(ALGO_AES_256);
    outKmm.setKId(0x50BCU);

    KeysetItem ks;
    ks.keysetId(1U);
    ks.algId(ALGO_AES_256); // we currently can only OTAR AES256 keys
    ks.keyLength(P25DEF::MAX_WRAPPED_ENC_KEY_LENGTH_BYTES);

    p25::kmm::KeyItem ki = p25::kmm::KeyItem();
    ki.keyFormat(KMM_KEY_FORMAT_TEK);
    ki.sln(0U);
    ki.kId(0x4983U);

    ki.setKey(testWrappedKeyFrame, 40U);
    ks.push_back(ki);

    std::vector<KeysetItem> keysets;
    keysets.push_back(ks);

    outKmm.setKeysets(keysets);

    UInt8Array kmmFrame = std::make_unique<uint8_t[]>(outKmm.fullLength());
    outKmm.encode(kmmFrame.get());
    outKmm.generateMAC(macTek, kmmFrame.get());

    REQUIRE(outKmm.verifyMAC(macTek, kmmFrame.get(), outKmm.fullLength()));
    kmmFrame[20U] ^= 0x01U;
    REQUIRE_FALSE(outKmm.verifyMAC(macTek, kmmFrame.get(), outKmm.fullLength()));
    kmmFrame[20U] ^= 0x01U;

    Utils::dump(2U, "P25_KMM_ReKey_CMAC_Test, GeneratedDataBlock", kmmFrame.get(), outKmm.fullLength());

    for (uint32_t i = 0; i < outKmm.fullLength(); i++) {
        if (kmmFrame.get()[i] != dataBlock[i]) {
            ::LogError("T", "P25_KMM_ReKey_CMAC_Test, INVALID AT IDX %d", i);
            failed = true;
        }
    }

    REQUIRE(failed==false);
}

TEST_CASE("KMM Hello CMAC Test (No Message Number)", "[p25][kmm_cmac][cap]") {
    bool failed = false;

    // MAC TEK
    uint8_t macTek[] =
    {
        0x16, 0x85, 0x62, 0x45, 0x3B, 0x3E, 0x7F, 0x61, 0x8D, 0x68, 0xB3, 0x87, 0xE0, 0xB9, 0x97, 0xE1,
        0xFB, 0x0F, 0x26, 0x4F, 0xA8, 0x3B, 0x74, 0xE4, 0x3B, 0x17, 0x29, 0x17, 0xBD, 0x39, 0x33, 0x9F
    };

    // AACA-D Sec 14.3 Hello CMAC vector (no MN)
    uint8_t expectedFrame[] =
    {
        0x0C, 0x00, 0x15, 0x08, 0x64, 0x3B, 0xA8, 0x71, 0x2B, 0x1D, 0x00, 0x18, 0x21, 0x6A, 0xD9, 0x1E,
        0x3E, 0xD5, 0xAC, 0x08, 0x84, 0x2F, 0x62, 0x41
    };

    uint8_t frameForMac[] =
    {
        0x0C, 0x00, 0x15, 0x08, 0x64, 0x3B, 0xA8, 0x71, 0x2B, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x08, 0x84, 0x2F, 0x62, 0x41
    };

    P25Crypto crypto;
    UInt8Array macKey = crypto.cryptAES_KMM_CMAC_KDF(macTek, frameForMac, (uint16_t)sizeof(frameForMac), false);
    UInt8Array mac = crypto.cryptAES_KMM_CMAC(macKey.get(), frameForMac, (uint16_t)sizeof(frameForMac));

    ::memcpy(frameForMac + 11U, mac.get(), 8U);

    for (uint32_t i = 0; i < sizeof(expectedFrame); i++) {
        if (frameForMac[i] != expectedFrame[i]) {
            ::LogError("T", "P25_KMM_Hello_CMAC_Test, INVALID AT IDX %d", i);
            failed = true;
        }
    }

    REQUIRE(failed==false);

    KMMHello hello;
    hello.setDstLLId(0x643BA8U);
    hello.setSrcLLId(0x712B1DU);
    hello.setResponseKind(KMM_ResponseKind::NONE);
    hello.setMACType(KMM_MAC::ENH_MAC);
    hello.setMACAlgId(ALGO_AES_256);
    hello.setMACKId(0x2F62U);
    hello.setMACFormat(KMM_MAC_FORMAT_CMAC);
    hello.setFlag(KMM_HelloFlag::IDENT_ONLY);
    REQUIRE(hello.fullLength() == sizeof(expectedFrame));
    UInt8Array encoded = std::make_unique<uint8_t[]>(hello.fullLength());
    ::memset(encoded.get(), 0x00U, hello.fullLength());
    hello.encode(encoded.get());
    hello.generateMAC(macTek, encoded.get());
    REQUIRE(::memcmp(encoded.get(), expectedFrame, sizeof(expectedFrame)) == 0);
}
