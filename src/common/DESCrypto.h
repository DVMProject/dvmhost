// SPDX-License-Identifier: MIT
/*
 * Digital Voice Modem - Common Library
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DESCrypto.h
 * @ingroup crypto
 * @file DESCrypto.cpp
 * @ingroup crypto
 */
#if !defined(__DES_CRYPTO_H__)
#define __DES_CRYPTO_H__

#include "common/Defines.h"

namespace crypto
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Data Encryption Standard Algorithm.
     * @ingroup crypto
     */
    class HOST_SW_API DES {
    public:
        /**
         * @brief Initializes a new instance of the DES class.
         */
        explicit DES();

        /**
         * @brief Encrypt input block with given key.
         * @param in Input buffer with block to encrypt.
         * @param key Encryption key.
         * @returns uint8_t* Encrypted input buffer.
         */
        uint8_t* encryptBlock(const uint8_t block[], const uint8_t key[]);
        /**
         * @brief Decrypt input block with given key.
         * @param block Input buffer with block to encrypt.
         * @param key Encryption key.
         * @returns uint8_t* Encrypted input buffer.
         */
        uint8_t* decryptBlock(const uint8_t block[], const uint8_t key[]);

    private:
        uint64_t sub_key[16]; // 48 bits each

        static ulong64_t toValue(const uint8_t* payload);
        static uint8_t* fromValue(const ulong64_t value);

        void generateSubkeys(uint64_t key);

        ulong64_t des(ulong64_t block, bool decrypt);

        ulong64_t intialPermutation(ulong64_t block);
        ulong64_t finalPermutation(ulong64_t block);

        void feistel(uint32_t& L, uint32_t& R, uint32_t F);
        uint32_t f(uint32_t R, ulong64_t k);
    };
} // namespace crypto

#endif // __DES_CRYPTO_H__
