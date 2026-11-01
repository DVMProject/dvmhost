// SPDX-License-Identifier: MIT
/*
 * Digital Voice Modem - Common Library
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup crypto Cryptography
 * @brief Defines and implements cryptography routines.
 * @ingroup common
 * 
 * @file RC4Crypto.h
 * @ingroup crypto
 * @file RC4Crypto.cpp
 * @ingroup crypto
 */
#if !defined(__RC4_CRYPTO_H__)
#define __RC4_CRYPTO_H__

#include "common/Defines.h"

namespace crypto
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t RC4_PERMUTATION_CNT = 256;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Rivest Cipher 4 Algorithm.
     * @ingroup crypto
     */
    class DVM_COMMON_API RC4 {
    public:
        /**
         * @brief Initializes a new instance of the RC4 class.
         */
        explicit RC4();

        /**
         * @brief Encrypt/Decrypt input buffer with given key.
         * @param in Input buffer (if encrypted, will decrypt, if decrypted, will encrypt)
         * @param inLen Input buffer length.
         * @param key Encryption key.
         * @param keyLen Encryption key length.
         * @returns uint8_t* Encrypted input buffer.
         */
        uint8_t* crypt(const uint8_t in[], uint32_t inLen, const uint8_t key[], uint32_t keyLen);
        /**
         * @brief Generates an ARC4 keystream.
         * @param len Keystream length.
         * @param key Encryption key.
         * @param keyLen Encryption key length.
         * @returns uint8_t* ARC4 keystream.
         */
        uint8_t* keystream(uint32_t len, const uint8_t key[], uint32_t keyLen);

    private:
        uint32_t m_i1;
        uint32_t m_i2;

        /**
         * @brief Swaps two values in the permutation array.
         * @param a Pointer to the permutation array.
         * @param i1 Index of the first value to swap.
         * @param i2 Index of the second value to swap.
         */
        void swap(uint8_t* a, uint8_t i1, uint8_t i2);
        /**
         * @brief Initializes the permutation array with the given key.
         * @param key Pointer to the encryption key.
         * @param keyLen Length of the encryption key.
         * @param permutation Pointer to the permutation array to initialize.
         */
        void init(const uint8_t key[], uint8_t keyLen, uint8_t* permutation);
        /**
         * @brief Transforms the input buffer using the given permutation array.
         * @param input Pointer to the input buffer.
         * @param length Length of the input buffer.
         * @param permutation Pointer to the permutation array.
         * @param output Pointer to the output buffer.
         * @param ksOnly If true, only generates the keystream without modifying the input
         */
        void transform(const uint8_t* input, uint32_t length, uint8_t* permutation, uint8_t* output, bool ksOnly);
    };
} // namespace crypto

#endif // __RC4_CRYPTO_H__
