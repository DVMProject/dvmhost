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
    class HOST_SW_API RC4 {
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

    private:
        uint32_t m_i1;
        uint32_t m_i2;

        void swap(uint8_t* a, uint8_t i1, uint8_t i2);
        void init(const uint8_t key[], uint8_t keyLen, uint8_t* permutation);
        void transform(const uint8_t* input, uint32_t length, uint8_t* permutation, uint8_t* output);
    };
} // namespace crypto

#endif // __RC4_CRYPTO_H__
