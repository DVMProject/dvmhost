// SPDX-License-Identifier: MIT
/*
 * Digital Voice Modem - Common Library
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2019 SergeyBel
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup crypto Cryptography
 * @brief Defines and implements cryptography routines.
 * @ingroup common
 * 
 * @file AESCrypto.h
 * @ingroup crypto
 * @file AESCrypto.cpp
 * @ingroup crypto
 */
#if !defined(__AES_CRYPTO_H__)
#define __AES_CRYPTO_H__

#include "common/Defines.h"

namespace crypto
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint8_t AES_NB = 4;

    /**
     * @brief Enumeration of AES key lengths.
     * @ingroup crypto
     */
    enum class AESKeyLength { AES_128, AES_192, AES_256 };

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Advanced Encryption Standard Algorithm.
     * @ingroup crypto
     */
    class HOST_SW_API AES {
    public:
        /**
         * @brief Initializes a new instance of the AES class.
         * @param keyLength Encryption key length from the AESKeyLength enumeration.
         */
        explicit AES(const AESKeyLength keyLength = AESKeyLength::AES_256);

        /**
         * @brief Encrypt input buffer with given key in AES-ECB.
         * @param in Input buffer.
         * @param inLen Input buffer length.
         * @param key Encryption key.
         * @returns uint8_t* Encrypted input buffer.
         */
        uint8_t* encryptECB(const uint8_t in[], uint32_t inLen, const uint8_t key[]);
        /**
         * @brief Decrypt input buffer with the given key in AES-ECB.
         * @param in Input buffer.
         * @param inLen Input buffer length.
         * @param key Encryption key.
         * @returns uint8_t* Decrypted input buffer.
         */
        uint8_t* decryptECB(const uint8_t in[], uint32_t inLen, const uint8_t key[]);

        /**
         * @brief Encrypt input buffer with given key and IV in AES-CBC.
         * @param in Input buffer.
         * @param inLen Input buffer length.
         * @param key Encryption key.
         * @param iv Initialization Vector buffer.
         * @return uint8_t* Encrypted input buffer.
         */
        uint8_t* encryptCBC(const uint8_t in[], uint32_t inLen, const uint8_t key[], const uint8_t* iv);
        /**
         * @brief Decrypt input buffer with given key and IV in AES-CBC.
         * @param in Input buffer.
         * @param inLen Input buffer length.
         * @param key Encryption key.
         * @param iv Initialization Vector buffer.
         * @return uint8_t* Decrypted input buffer.
         */
        uint8_t* decryptCBC(const uint8_t in[], uint32_t inLen, const uint8_t key[], const uint8_t* iv);

        /**
         * @brief Encrypt input buffer with given key and IV in AES-CFB.
         * @param in Input buffer.
         * @param inLen Input buffer length.
         * @param key Encryption key.
         * @param iv Initialization Vector buffer.
         * @return uint8_t* Encrypted input buffer.
         */
        uint8_t* encryptCFB(const uint8_t in[], uint32_t inLen, const uint8_t key[], const uint8_t* iv);
        /**
         * @brief Decrypt input buffer with given key and IV in AES-CFB.
         * @param in Input buffer.
         * @param inLen Input buffer length.
         * @param key Encryption key.
         * @param iv Initialization Vector buffer.
         * @return uint8_t* Decrypted input buffer.
         */
        uint8_t* decryptCFB(const uint8_t in[], uint32_t inLen, const uint8_t key[], const uint8_t* iv);

        static constexpr uint32_t BLOCK_BYTES_LEN = 4 * AES_NB * sizeof(uint8_t);

    private:
        uint32_t m_Nk;
        uint32_t m_Nr;

        void subBytes(uint8_t state[4][AES_NB]);
        void invSubBytes(uint8_t state[4][AES_NB]);
        void shiftRow(uint8_t state[4][AES_NB], uint32_t i, uint32_t n);  // shift row i on n positions
        void shiftRows(uint8_t state[4][AES_NB]);
        void invShiftRows(uint8_t state[4][AES_NB]);
    
        uint8_t xtime(uint8_t b) { return (b << 1) ^ (((b >> 7) & 1) * 0x1BU); }

        void mixColumns(uint8_t state[4][AES_NB]);
        void invMixColumns(uint8_t state[4][AES_NB]);
        void addRoundKey(uint8_t state[4][AES_NB], uint8_t* key);

        void subWord(uint8_t* a);
        void rotWord(uint8_t* a);
        void xorWords(uint8_t* a, uint8_t* b, uint8_t* c);

        void rCon(uint8_t* a, uint32_t n);

        void keyExpansion(const uint8_t key[], uint8_t w[]);

        void encryptBlock(const uint8_t in[], uint8_t out[], uint8_t* roundKeys);
        void decryptBlock(const uint8_t in[], uint8_t out[], uint8_t* roundKeys);

        void xorBlocks(const uint8_t* a, const uint8_t* b, uint8_t* c, uint32_t len);
    };
} // namespace crypto

#endif // __AES_CRYPTO_H__
