// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 */
#include "host/Defines.h"
#include "common/p25/P25Defines.h"
#include "common/p25/Crypto.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstring>
#include <vector>

using namespace p25::crypto;
using namespace p25::defines;

TEST_CASE("AES PDU OFB covers messages larger than 255 octets", "[p25][crypto][otar]")
{
    uint8_t key[32U];
    uint8_t mi[MI_LENGTH_BYTES];
    for (uint32_t i = 0U; i < sizeof(key); ++i) key[i] = (uint8_t)i;
    for (uint32_t i = 0U; i < sizeof(mi); ++i) mi[i] = (uint8_t)(0xA0U + i);

    std::vector<uint8_t> original(512U), encrypted(512U);
    for (uint32_t i = 0U; i < original.size(); ++i) original[i] = (uint8_t)i;
    encrypted = original;

    P25Crypto enc;
    enc.setMI(mi); enc.setTEKAlgoId(ALGO_AES_256); enc.setKey(key, sizeof(key)); enc.generateKeystream();
    enc.cryptAES_PDU(encrypted.data(), (uint32_t)encrypted.size());
    REQUIRE(encrypted[300U] != original[300U]);

    P25Crypto dec;
    dec.setMI(mi); dec.setTEKAlgoId(ALGO_AES_256); dec.setKey(key, sizeof(key)); dec.generateKeystream();
    dec.cryptAES_PDU(encrypted.data(), (uint32_t)encrypted.size());
    REQUIRE(encrypted == original);
}

TEST_CASE("AES voice encryption retains the AAAD-B IMBE keystream mapping", "[p25][crypto][voice]")
{
    // TIA-102.AAAD-B Annex C.4 AES-256 key and MI. The expected first
    // LDU1 IMBE mask is octets 12-22 of the usable FDMA keystream after
    // discarding the first OFB iteration (AAAD-B Table 5-5).
    const uint8_t key[32U] = {
        0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU,
        0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU, 0x01U,
        0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU, 0x01U, 0x23U,
        0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU, 0x01U, 0x23U, 0x45U
    };
    const uint8_t mi[MI_LENGTH_BYTES] = {
        0x12U, 0x34U, 0x56U, 0x78U, 0x90U, 0xABU, 0xCDU, 0xEFU, 0x00U
    };
    const uint8_t expected[RAW_IMBE_LENGTH_BYTES] = {
        0xF5U, 0x71U, 0xCBU, 0x02U, 0x9AU, 0x93U,
        0xAEU, 0xFFU, 0x9BU, 0xDBU, 0x7BU
    };

    uint8_t imbe[RAW_IMBE_LENGTH_BYTES] = { 0U };
    P25Crypto vectorCrypto;
    vectorCrypto.setMI(mi);
    vectorCrypto.setTEKAlgoId(ALGO_AES_256);
    vectorCrypto.setKey(key, sizeof(key));
    vectorCrypto.generateKeystream();
    vectorCrypto.cryptAES_IMBE(imbe, DUID::LDU1);
    REQUIRE(::memcmp(imbe, expected, sizeof(expected)) == 0);

    for (DUID::E duid : { DUID::LDU1, DUID::LDU2 }) {
        std::array<std::array<uint8_t, RAW_IMBE_LENGTH_BYTES>, 9U> plain{};
        for (size_t frame = 0U; frame < plain.size(); ++frame)
            for (size_t octet = 0U; octet < plain[frame].size(); ++octet)
                plain[frame][octet] = (uint8_t)(frame * plain[frame].size() + octet);
        auto encrypted = plain;

        P25Crypto tx;
        tx.setMI(mi); tx.setTEKAlgoId(ALGO_AES_256); tx.setKey(key, sizeof(key)); tx.generateKeystream();
        for (auto& frame : encrypted)
            tx.cryptAES_IMBE(frame.data(), duid);
        REQUIRE(encrypted != plain);

        P25Crypto rx;
        rx.setMI(mi); rx.setTEKAlgoId(ALGO_AES_256); rx.setKey(key, sizeof(key)); rx.generateKeystream();
        for (auto& frame : encrypted)
            rx.cryptAES_IMBE(frame.data(), duid);
        REQUIRE(encrypted == plain);
    }
}
