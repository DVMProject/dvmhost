// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Test Suite
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "common/nxdn/Crypto.h"

using namespace nxdn::crypto;
using namespace nxdn::defines;

/**
 * @brief Crypt an AMBE word using the specified cipher type.
 * @param crypto The NXDN crypto instance to use.
 * @param cipher The cipher type to use.
 * @param word The AMBE word to crypt.
 * @param index The index of the AMBE word within the session.
 */
static void cryptWord(NXDNCrypto& crypto, uint8_t cipher, uint8_t* word, uint8_t index)
{
    if (cipher == CIPHER_TYPE_EHR)
        crypto.cryptEHR_AMBE(word, index);
    else if (cipher == CIPHER_TYPE_DES)
        crypto.cryptDES_AMBE(word, index);
    else if (cipher == CIPHER_TYPE_AES)
        crypto.cryptAES_AMBE(word, index);
}

/**
 * @brief Require that the first AMBE word produced by the given cipher matches the expected bit pattern.
 * @param crypto The NXDN crypto instance to use.
 * @param cipher The cipher type to test.
 * @param expected The expected bit pattern for the first AMBE word.
 */
static void requireWord(NXDNCrypto& crypto, uint8_t cipher, const char* expected)
{
    uint8_t word[AMBE_LENGTH_BITS] = {};
    cryptWord(crypto, cipher, word, 0U);
    for (uint8_t i = 0U; i < AMBE_LENGTH_BITS; i++)
        REQUIRE(word[i] == (uint8_t)(expected[i] - '0'));

    // XOR with the same indexed stream is its own inverse.
    cryptWord(crypto, cipher, word, 0U);
    for (uint8_t bit : word)
        REQUIRE(bit == 0U);
}

TEST_CASE("NXDN EHR known-answer vector", "[nxdn][crypto][ehr]")
{
    const uint8_t key[] = { 0x12U, 0x34U };
    NXDNCrypto crypto;
    crypto.setTEKCipherType(CIPHER_TYPE_EHR);
    crypto.setTEKKeyId(1U);
    crypto.setKey(key, sizeof(key));
    crypto.generateKeystream();
    REQUIRE(crypto.hasValidKeystream());
    requireWord(crypto, CIPHER_TYPE_EHR, "0010110001001000111010011011001001110101101011010");
    uint8_t word[AMBE_LENGTH_BITS] = {};
    crypto.cryptEHR_AMBE(word, EHR_WORDS_PER_SESSION);
    for (uint8_t bit : word)
        REQUIRE(bit == 0U);
}

TEST_CASE("NXDN DES-OFB known-answer vector", "[nxdn][crypto][des]")
{
    const uint8_t key[] = { 0x13U, 0x34U, 0x57U, 0x79U, 0x9BU, 0xBCU, 0xDFU, 0xF1U };
    const uint8_t mi[] = { 0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU };
    NXDNCrypto crypto;
    crypto.setTEKCipherType(CIPHER_TYPE_DES);
    crypto.setTEKKeyId(2U);
    crypto.setKey(key, sizeof(key));
    crypto.generateKeystream();
    REQUIRE_FALSE(crypto.hasValidKeystream());
    crypto.setMI(mi);
    crypto.generateKeystream();
    REQUIRE(crypto.hasValidKeystream());
    requireWord(crypto, CIPHER_TYPE_DES, "0110011110101110011110100010100101100001110111111");
}

TEST_CASE("NXDN AES-256-OFB known-answer vector", "[nxdn][crypto][aes]")
{
    uint8_t key[AES_KEY_LENGTH_BYTES];
    for (uint8_t i = 0U; i < sizeof(key); i++)
        key[i] = i;

    const uint8_t mi[] = { 0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xABU, 0xCDU, 0xEFU };
    NXDNCrypto crypto;
    crypto.setTEKCipherType(CIPHER_TYPE_AES);
    crypto.setTEKKeyId(3U);
    crypto.setKey(key, sizeof(key));
    crypto.setMI(mi);
    crypto.generateKeystream();
    REQUIRE(crypto.hasValidKeystream());
    requireWord(crypto, CIPHER_TYPE_AES, "0011110010110110000110001010000001001010001110011");
    crypto.generateNextMI();

    uint8_t next[MI_LENGTH_BYTES];
    crypto.getMI(next);
    const uint8_t expected[] = { 0x20U, 0xB1U, 0x25U, 0xE7U, 0x79U, 0xD0U, 0xF3U, 0x4EU };
    REQUIRE(::memcmp(next, expected, sizeof(next)) == 0);
}

TEST_CASE("NXDN crypto rejects invalid keys", "[nxdn][crypto]")
{
    NXDNCrypto crypto;
    const uint8_t zeroEhr[] = { 0x00U, 0x00U };
    const uint8_t highEhr[] = { 0x80U, 0x01U };
    const uint8_t validEhr[] = { 0x00U, 0x01U };
    const uint8_t weakDes[] = { 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U };
    crypto.setTEKCipherType(CIPHER_TYPE_EHR);
    crypto.setTEKKeyId(1U);
    crypto.setKey(zeroEhr, sizeof(zeroEhr));
    REQUIRE(crypto.getTEKKeyLength() == 0U);
    crypto.setKey(highEhr, sizeof(highEhr));
    REQUIRE(crypto.getTEKKeyLength() == 0U);
    crypto.setTEKKeyId(0U);
    crypto.setKey(validEhr, sizeof(validEhr));
    REQUIRE(crypto.getTEKKeyLength() == 0U);
    crypto.setTEKKeyId(64U);
    crypto.setKey(validEhr, sizeof(validEhr));
    REQUIRE(crypto.getTEKKeyLength() == 0U);
    crypto.setTEKCipherType(CIPHER_TYPE_DES);
    crypto.setTEKKeyId(1U);
    crypto.setKey(weakDes, sizeof(weakDes));
    REQUIRE(crypto.getTEKKeyLength() == 0U);
    crypto.setTEKCipherType(CIPHER_TYPE_EHR);
    crypto.setTEKKeyId(63U);
    crypto.setKey(validEhr, sizeof(validEhr));
    REQUIRE(crypto.getTEKKeyLength() == sizeof(validEhr));
}
