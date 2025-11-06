// SPDX-License-Identifier: MIT
/*
 * Digital Voice Modem - Common Library
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "DESCrypto.h"
#include "Log.h"
#include "Utils.h"

using namespace crypto;

#include <cassert>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define LB32_MASK 0x00000001
#define LB64_MASK 0x0000000000000001
#define L64_MASK 0x00000000ffffffff
#define H64_MASK 0xffffffff00000000

// Permuted Choice 1 Table [7*8]
static const char PC1_TABLE[] = {
    57, 49, 41, 33, 25, 17,  9,
     1, 58, 50, 42, 34, 26, 18,
    10,  2, 59, 51, 43, 35, 27,
    19, 11,  3, 60, 52, 44, 36,

    63, 55, 47, 39, 31, 23, 15,
     7, 62, 54, 46, 38, 30, 22,
    14,  6, 61, 53, 45, 37, 29,
    21, 13,  5, 28, 20, 12,  4
};

// Permuted Choice 2 Table [6*8]
static const char PC2_TABLE[] = {
    14, 17, 11, 24,  1,  5,
     3, 28, 15,  6, 21, 10,
    23, 19, 12,  4, 26,  8,
    16,  7, 27, 20, 13,  2,
    41, 52, 31, 37, 47, 55,
    30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,
    46, 42, 50, 36, 29, 32
};

// Iteration Shift Array
static const char ITERATION_SHIFT[] = {
//  1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
    1,  1,  2,  2,  2,  2,  2,  2,  1,  2,  2,  2,  2,  2,  2,  1
};

// Initial Permutation Table [8*8]
static const char IP[] = {
    58, 50, 42, 34, 26, 18, 10, 2,
    60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6,
    64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17,  9, 1,
    59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5,
    63, 55, 47, 39, 31, 23, 15, 7
};

// Inverse Initial Permutation Table [8*8]
static const char FP[] = {
    40, 8, 48, 16, 56, 24, 64, 32,
    39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30,
    37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28,
    35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26,
    33, 1, 41,  9, 49, 17, 57, 25
};

// Expansion Table [6*8]
static const char EXPANSION[] = {
    32,  1,  2,  3,  4,  5,
     4,  5,  6,  7,  8,  9,
     8,  9, 10, 11, 12, 13,
    12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21,
    20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29,
    28, 29, 30, 31, 32,  1
};

// Post S-Box Permutation [4*8]
static const char PBOX[] = {
    16,  7, 20, 21,
    29, 12, 28, 17,
     1, 15, 23, 26,
     5, 18, 31, 10,
     2,  8, 24, 14,
    32, 27,  3,  9,
    19, 13, 30,  6,
    22, 11,  4, 25
};

// The S-Box Tables [8*16*4]
static const char SBOX[8][64] = {
    { // S1
        14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
        0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
        4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
        15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13 },
    { // S2
        15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
        3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
        0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
        13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9 },
    { // S3
        10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
        13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
        13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
        1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12 },
    { // S4
        7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
        13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
        10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
        3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14 },
    { // S5
        2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
        14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
        4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
        11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3 },
    { // S6
        12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
        10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
        9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
        4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13 },
    { // S7
        4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
        13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
        1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
        6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12 },
    { // S8
        13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
        1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
        7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
        2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11 }
};

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DES class. */

DES::DES() = default;

/* Encrypt input block with given key. */

uint8_t* DES::encryptBlock(const uint8_t block[], const uint8_t key[])
{
    ulong64_t keyValue = toValue(key);
    ulong64_t blockValue = toValue(block);

    generateSubkeys(keyValue);
    ulong64_t out = des(blockValue, false);

    return fromValue(out);
}

/* Decrypt input block with given key. */

uint8_t* DES::decryptBlock(const uint8_t block[], const uint8_t key[])
{
    ulong64_t keyValue = toValue(key);
    ulong64_t blockValue = toValue(block);

    generateSubkeys(keyValue);
    ulong64_t out = des(blockValue, true);

    return fromValue(out);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to convert payload bytes to a 64-bit long value. */

ulong64_t DES::toValue(const uint8_t* payload)
{
    assert(payload != nullptr);

    ulong64_t value = 0U;

    // combine bytes into ulong64_t (8 byte) value
    value = payload[0U];
    value = (value << 8) + payload[1U];
    value = (value << 8) + payload[2U];
    value = (value << 8) + payload[3U];
    value = (value << 8) + payload[4U];
    value = (value << 8) + payload[5U];
    value = (value << 8) + payload[6U];
    value = (value << 8) + payload[7U];

    return value;
}

/* Internal helper to convert a 64-bit long value to payload bytes. */

uint8_t* DES::fromValue(const ulong64_t value)
{
    uint8_t* payload = new uint8_t[8U];
    ::memset(payload, 0x00U, 8U);

    // split ulong64_t (8 byte) value into bytes
    payload[0U] = (uint8_t)((value >> 56) & 0xFFU);
    payload[1U] = (uint8_t)((value >> 48) & 0xFFU);
    payload[2U] = (uint8_t)((value >> 40) & 0xFFU);
    payload[3U] = (uint8_t)((value >> 32) & 0xFFU);
    payload[4U] = (uint8_t)((value >> 24) & 0xFFU);
    payload[5U] = (uint8_t)((value >> 16) & 0xFFU);
    payload[6U] = (uint8_t)((value >> 8) & 0xFFU);
    payload[7U] = (uint8_t)((value >> 0) & 0xFFU);

    return payload;
}

/* */

void DES::generateSubkeys(uint64_t key)
{
    // initial key schedule calculation
    uint64_t PC1 = 0; // 56 bits
    for (uint8_t i = 0; i < 56; i++) {
        PC1 <<= 1;
        PC1 |= (key >> (64 - PC1_TABLE[i])) & LB64_MASK;
    }

    // 28 bits
    uint32_t C = (uint32_t)((PC1 >> 28) & 0x000000000fffffff);
    uint32_t D = (uint32_t)(PC1 & 0x000000000fffffff);

    // calculation of the 16 keys
    for (uint8_t i = 0; i < 16; i++) {
        // key schedule, shifting Ci and Di
        for (uint8_t j = 0; j < ITERATION_SHIFT[i]; j++) {
            C = (0x0fffffff & (C << 1)) | (0x00000001 & (C >> 27));
            D = (0x0fffffff & (D << 1)) | (0x00000001 & (D >> 27));
        }

        uint64_t PC2 = (((uint64_t)C) << 28) | (uint64_t)D;

        sub_key[i] = 0; // 48 bits (2*24)
        for (uint8_t j = 0; j < 48; j++) {
            sub_key[i] <<= 1;
            sub_key[i] |= (PC2 >> (56 - PC2_TABLE[j])) & LB64_MASK;
        }
    }
}

/* */

ulong64_t DES::des(ulong64_t block, bool decrypt)
{
    // applying initial permutation
    block = intialPermutation(block);

    // dividing T' into two 32-bit parts
    uint32_t L = (uint32_t)(block >> 32) & L64_MASK;
    uint32_t R = (uint32_t)(block & L64_MASK);

    // 16 rounds
    for (uint8_t i = 0; i < 16; i++) {
        uint32_t F = decrypt ? f(R, sub_key[15 - i]) : f(R, sub_key[i]);
        feistel(L, R, F);
    }

    // swapping the two parts
    block = (((uint64_t)R) << 32) | (uint64_t)L;

    // applying final permutation
    return finalPermutation(block);
}

/* */

ulong64_t DES::intialPermutation(ulong64_t block)
{
    // initial permutation
    uint64_t result = 0;
    for (uint8_t i = 0; i < 64; i++) {
        result <<= 1;
        result |= (block >> (64 - IP[i])) & LB64_MASK;
    }

    return result;
}

/* */

ulong64_t DES::finalPermutation(ulong64_t block)
{
    // inverse initial permutation
    uint64_t result = 0;
    for (uint8_t i = 0; i < 64; i++) {
        result <<= 1;
        result |= (block >> (64 - FP[i])) & LB64_MASK;
    }

    return result;
}

/* */

void DES::feistel(uint32_t& L, uint32_t& R, uint32_t F)
{
    uint32_t temp = R;
    R = L ^ F;
    L = temp;
}

/* */

uint32_t DES::f(uint32_t R, ulong64_t k)
{
    // applying expansion permutation and returning 48-bit data
    ulong64_t input = 0;
    for (uint8_t i = 0; i < 48; i++) {
        input <<= 1;
        input |= (ulong64_t)((R >> (32 - EXPANSION[i])) & LB32_MASK);
    }

    // XORing expanded Ri with Ki, the round key
    input = input ^ k;

    // applying S-Boxes function and returning 32-bit data
    uint32_t output = 0;
    for (uint8_t i = 0; i < 8; i++) {
        // Outer bits
        char row = (char)((input & (0x0000840000000000 >> 6 * i)) >> (42 - 6 * i));
        row = (row >> 4) | (row & 0x01);

        // Middle 4 bits of input
        char column = (char)((input & (0x0000780000000000 >> 6 * i)) >> (43 - 6 * i));

        output <<= 4;
        output |= (uint32_t)(SBOX[i][16 * row + column] & 0x0f);
    }

    // applying the round permutation
    uint32_t result = 0;
    for (uint8_t i = 0; i < 32; i++) {
        result <<= 1;
        result |= (output >> (32 - PBOX[i])) & LB32_MASK;
    }

    return result;
}
