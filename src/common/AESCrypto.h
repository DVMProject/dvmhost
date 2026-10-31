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
    class DVM_COMMON_API AES {
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

        /**
         * @brief Substitutes bytes in the state using the S-Box.
         * @param state The state array to substitute bytes in.
         */
        void subBytes(uint8_t state[4][AES_NB]);
        /**
         * @brief Inverse substitutes bytes in the state using the inverse S-Box.
         * @param state The state array to inverse substitute bytes in.
         */
        void invSubBytes(uint8_t state[4][AES_NB]);
        /**
         * @brief Shifts the rows of the state to the left by a certain number of positions.
         * @param state The state array to shift rows in.
         * @param i The index of the row to shift.
         * @param n The number of positions to shift the row.
         */
        void shiftRow(uint8_t state[4][AES_NB], uint32_t i, uint32_t n);  // shift row i on n positions
        /**
         * @brief Inverse shifts the rows of the state to the right by a certain number of positions.
         * @param state The state array to inverse shift rows in.
         * @param i The index of the row to inverse shift.
         * @param n The number of positions to inverse shift the row.
         */
        void shiftRows(uint8_t state[4][AES_NB]);
        /**
         * @brief Inverse shifts the rows of the state to the right by a certain number of positions.
         * @param state The state array to inverse shift rows in.
         */
        void invShiftRows(uint8_t state[4][AES_NB]);
    
        /**
         * @brief Performs the xtime operation on a byte.
         * @param b The byte to perform the xtime operation on.
         * @returns uint8_t The result of the xtime operation.
         */
        uint8_t xtime(uint8_t b) { return (b << 1) ^ (((b >> 7) & 1) * 0x1BU); }

        /**
         * @brief Multiplies two bytes in the Galois field GF(2^8).
         * @param a The first byte.
         * @param b The second byte.
         * @returns uint8_t The result of the multiplication in GF(2^8).
         */
        void mixColumns(uint8_t state[4][AES_NB]);
        /**
         * @brief Inverse multiplies the columns of the state in the Galois field GF(2^8).
         * @param state The state array to inverse multiply columns in.
         */
        void invMixColumns(uint8_t state[4][AES_NB]);
        /**
         * @brief Adds the round key to the state.
         * @param state The state array to add the round key to.
         * @param key The round key to add to the state.
         */
        void addRoundKey(uint8_t state[4][AES_NB], uint8_t* key);

        /**
         * @brief Substitutes a word using the S-Box.
         * @param a The word to substitute.
         */
        void subWord(uint8_t* a);
        /**
         * @brief Rotates a word by shifting its bytes to the left.
         * @param a The word to rotate.
         */
        void rotWord(uint8_t* a);
        /**
         * @brief Performs the XOR operation on two words and stores the result in a third word.
         * @param a The first word.
         * @param b The second word.
         * @param c The word to store the result of the XOR operation.
         */
        void xorWords(uint8_t* a, uint8_t* b, uint8_t* c);

        /**
         * @brief Performs the round constants operation on a word.
         * @param a The word to perform the round constants operation on.
         * @param n The round number for the round constants operation.
         */
        void rCon(uint8_t* a, uint32_t n);

        /**
         * @brief Expands the encryption key into round keys for the AES algorithm.
         * @param key The encryption key.
         * @param w The array to store the expanded round keys.
         */
        void keyExpansion(const uint8_t key[], uint8_t w[]);
        
        /**
         * @brief Encrypts a single block of data using the AES algorithm.
         * @param in The input block to encrypt.
         * @param out The output block to store the encrypted data.
         * @param roundKeys The expanded round keys for the AES algorithm.
         */
        void encryptBlock(const uint8_t in[], uint8_t out[], uint8_t* roundKeys);
        /**
         * @brief Decrypts a single block of data using the AES algorithm.
         * @param in The input block to decrypt.
         * @param out The output block to store the decrypted data.
         * @param roundKeys The expanded round keys for the AES algorithm.
         */
        void decryptBlock(const uint8_t in[], uint8_t out[], uint8_t* roundKeys);

        /**
         * @brief Performs the XOR operation on two blocks of data.
         * @param a The first block of data.
         * @param b The second block of data.
         * @param c The block to store the result of the XOR operation.
         * @param len The length of the blocks in bytes.
         */
        void xorBlocks(const uint8_t* a, const uint8_t* b, uint8_t* c, uint32_t len);
    };
} // namespace crypto

#endif // __AES_CRYPTO_H__
