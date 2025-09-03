// SPDX-License-Identifier: MIT
/*
 * Digital Voice Modem - Common Library
 * MIT Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "RC4Crypto.h"
#include "Log.h"
#include "Utils.h"

using namespace crypto;

#include <cassert>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RC4 class. */

RC4::RC4() = default;

/* Encrypt/Decrypt input buffer with given key. */

uint8_t* RC4::crypt(const uint8_t in[], uint32_t inLen, const uint8_t key[], uint32_t keyLen)
{
    uint8_t permutation[RC4_PERMUTATION_CNT];
    ::memset(permutation, 0x00U, RC4_PERMUTATION_CNT);

    init(key, keyLen, permutation);

    uint8_t* out = new uint8_t[inLen];
    ::memset(out, 0x00U, inLen);

    transform(in, inLen, permutation, out, false);
    return out;
}

/* Generates an ARC4 keystream. */

uint8_t* RC4::keystream(uint32_t len, const uint8_t key[], uint32_t keyLen)
{
    uint8_t permutation[RC4_PERMUTATION_CNT];
    ::memset(permutation, 0x00U, RC4_PERMUTATION_CNT);

    init(key, keyLen, permutation);

    uint8_t* out = new uint8_t[len];
    ::memset(out, 0x00U, len);

    transform(out, len, permutation, out, true);
    return out;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* */

void RC4::swap(uint8_t* a, uint8_t i1, uint8_t i2)
{
    uint8_t temp = a[i1];
    a[i1] = a[i2];
    a[i2] = temp;
}

/* */

void RC4::init(const uint8_t key[], uint8_t keyLen, uint8_t* permutation)
{
    assert(permutation != nullptr);

    // init state variable
    for (uint32_t i = 0U; i < RC4_PERMUTATION_CNT; i++)
        permutation[i] = (uint8_t)i;

    m_i1 = 0U;
    m_i2 = 0U;

    // randomize, using key
    for (int j = 0, i = 0; i < 256; i++) {
        j = (j + permutation[i] + key[i % keyLen]) & 0xFFU;

        // swap permutation[i] and permutation[j]
        swap(permutation, i, j);
    }
}

/* */

void RC4::transform(const uint8_t* input, uint32_t length, uint8_t* permutation, uint8_t* output, bool ksOnly)
{
    assert(input != nullptr);
    assert(output != nullptr);

    uint32_t i = 0U, j = 0U;
    for (; i < length; i++, j++) {
        // update indices
        m_i1 = (uint8_t)((m_i1 + 1) & 0xFFU);
        m_i2 = (uint8_t)((m_i2 + permutation[m_i1]) & 0xFFU);

        // swap permutation[m_i1] and permutation[m_i2]
        swap(permutation, m_i1, m_i2);

        // transform byte
        if (ksOnly)
            output[i] = permutation[(permutation[m_i1] + permutation[m_i2]) & 0xFFU];
        else {
            uint8_t b = (uint8_t)((permutation[m_i1] + permutation[m_i2]) & 0xFFU);
            output[j] = (uint8_t)(input[i] ^ permutation[b]);
        }
    }
}
