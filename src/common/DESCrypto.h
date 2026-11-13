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
    class DVM_COMMON_API DES {
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

        /**
         * @brief Internal helper to convert payload bytes to a 64-bit long value.
         * @param payload Pointer to the byte array.
         * @returns ulong64_t The 64-bit value.
         */
        static ulong64_t toValue(const uint8_t* payload);
        /**
         * @brief Internal helper to convert a 64-bit long value to payload bytes.
         * @param value The 64-bit value.
         * @returns uint8_t* Pointer to the byte array.
         */
        static uint8_t* fromValue(const ulong64_t value);

        /**
         * @brief Generates the subkeys for the DES algorithm.
         * @param key The encryption key.
         */
        void generateSubkeys(uint64_t key);

        /**
         * @brief Encrypts or decrypts a block of data using the DES algorithm.
         * @param block The input block to encrypt or decrypt.
         * @param decrypt A boolean indicating whether to decrypt (true) or encrypt (false).
         * @returns ulong64_t The encrypted or decrypted block.
         */
        ulong64_t des(ulong64_t block, bool decrypt);

        /**
         * @brief Performs the initial permutation on a block of data.
         * @param block The input block to permute.
         * @returns ulong64_t The permuted block.
         */
        ulong64_t intialPermutation(ulong64_t block);
        /**
         * @brief Performs the final permutation on a block of data.
         * @param block The input block to permute.
         * @returns ulong64_t The permuted block.
         */
        ulong64_t finalPermutation(ulong64_t block);

        /**
         * @brief Performs the Feistel function on a block of data.
         * @param L The left half of the block.
         * @param R The right half of the block.
         * @param F The output of the f function.
         */
        void feistel(uint32_t& L, uint32_t& R, uint32_t F);
        /**
         * @brief The f function used in the Feistel network.
         * @param R The right half of the block.
         * @param k The subkey for the current round.
         * @returns uint32_t The output of the f function.
         */
        uint32_t f(uint32_t R, ulong64_t k);
    };
} // namespace crypto

#endif // __DES_CRYPTO_H__
