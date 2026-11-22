// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 * 
 */
#include "Defines.h"
#include "edac/BCH.h"
#include "Utils.h"

using namespace edac;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const int length = 63;
const int k = 16;
const int t = 11;
const int fieldOrder = 6;
const int fieldSize = 1 << fieldOrder;
const int primitivePolynomial = 0x43; // x^6 + x + 1

const int g[] = {
    1, 1, 0,  0, 1, 1,  0, 1, 1,  0, 0, 1,  0, 0, 1,  1, 0, 0,  0, 0, 1,  0, 1, 1,
    1, 1, 0,  1, 1, 1,  0, 1, 0,  0, 1, 1,  1, 0, 1,  1, 0, 0,  1, 0, 1,  0, 1, 1
};

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the BCH class. */

BCH::BCH() = default;

/* Finalizes a instance of the BCH class. */

BCH::~BCH() = default;

/* Encodes input data with BCH. */

void BCH::encode(uint8_t* nid)
{
    assert(nid != nullptr);

    int data[16];
    for (int i = 0; i < 16; i++)
        data[i] = READ_BIT(nid, i) ? 1 : 0;

    int bb[63];
    encode(data, bb);

    for (int i = 0; i < (length - k); i++) {
        bool b = bb[i] == 1;
        WRITE_BIT(nid, i + 16U, b);
    }
}

/* Decodes and corrects a BCH(63,16,23) P25 NID codeword. */

bool BCH::decode(uint8_t* nid, uint32_t& errors)
{
    assert(nid != nullptr);

    errors = 0U;
    uint8_t corrected[8U];
    ::memcpy(corrected, nid, sizeof(corrected));

    uint8_t syndromes[2 * t + 1] = { 0U };
    bool valid = true;
    for (int syndrome = 1; syndrome <= 2 * t; syndrome++) {
        uint8_t value = 0U;
        for (int bit = 0; bit < length; bit++) {
            if (READ_BIT(corrected, bit) != 0U)
                value ^= alphaPower(syndrome * (length - 1 - bit));
        }
        syndromes[syndrome] = value;
        valid = valid && (value == 0U);
    }

    if (valid)
        return true;

    uint8_t locator[2 * t + 1] = { 0U };
    uint8_t previous[2 * t + 1] = { 0U };
    locator[0U] = 1U;
    previous[0U] = 1U;

    int degree = 0;
    int shift = 1;
    uint8_t previousDiscrepancy = 1U;

    for (int step = 0; step < 2 * t; step++) {
        uint8_t discrepancy = syndromes[step + 1];
        for (int i = 1; i <= degree; i++)
            discrepancy ^= multiply(locator[i], syndromes[step + 1 - i]);

        if (discrepancy == 0U) {
            shift++;
            continue;
        }

        uint8_t saved[2 * t + 1];
        for (int i = 0; i <= 2 * t; i++)
            saved[i] = locator[i];

        const uint8_t scale = multiply(discrepancy, inverse(previousDiscrepancy));
        for (int i = 0; i + shift <= 2 * t; i++)
            locator[i + shift] ^= multiply(scale, previous[i]);

        if ((2 * degree) <= step) {
            degree = step + 1 - degree;
            for (int i = 0; i <= 2 * t; i++)
                previous[i] = saved[i];
            previousDiscrepancy = discrepancy;
            shift = 1;
        }
        else {
            shift++;
        }
    }

    if ((degree <= 0) || (degree > t))
        return false;

    for (int polynomialPosition = 0; polynomialPosition < length; polynomialPosition++) {
        const uint8_t root = alphaPower(-polynomialPosition);
        uint8_t evaluation = locator[degree];
        for (int i = degree - 1; i >= 0; i--)
            evaluation = multiply(evaluation, root) ^ locator[i];

        if (evaluation == 0U) {
            const int bit = length - 1 - polynomialPosition;
            WRITE_BIT(corrected, bit, READ_BIT(corrected, bit) == 0U);
            errors++;
        }
    }

    if (errors != static_cast<uint32_t>(degree))
        return false;

    for (int syndrome = 1; syndrome <= 2 * t; syndrome++) {
        uint8_t value = 0U;
        for (int bit = 0; bit < length; bit++) {
            if (READ_BIT(corrected, bit) != 0U)
                value ^= alphaPower(syndrome * (length - 1 - bit));
        }
        if (value != 0U)
            return false;
    }

    ::memcpy(nid, corrected, sizeof(corrected));
    return true;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Multiplies two elements of GF(2^6). */

uint8_t BCH::multiply(uint8_t a, uint8_t b)
{
    uint8_t product = 0U;

    while (b != 0U) {
        if ((b & 0x01U) != 0U)
            product ^= a;

        b >>= 1;
        a <<= 1;
        if ((a & fieldSize) != 0U)
            a ^= primitivePolynomial;
    }

    return product;
}

/* Returns alpha raised to an exponent in GF(2^6). */

uint8_t BCH::alphaPower(int exponent)
{
    while (exponent < 0)
        exponent += length;
    exponent %= length;

    uint8_t value = 1U;
    for (int i = 0; i < exponent; i++)
        value = multiply(value, 0x02U);

    return value;
}

/* Returns the multiplicative inverse of a nonzero GF(2^6) element. */

uint8_t BCH::inverse(uint8_t value)
{
    assert(value != 0U);

    uint8_t result = 1U;
    for (int i = 0; i < fieldSize - 2; i++)
        result = multiply(result, value);

    return result;
}

/* Compute redundancy bb[], the coefficients of b(x). The redundancy polynomial b(x) is the 
   remainder after dividing x^(length-k)*data(x) by the generator polynomial g(x). */

void BCH::encode(const int* data, int* bb)
{
    for (int i = 0; i < length - k; i++)
        bb[i] = 0;

    for (int i = k - 1; i >= 0; i--) {
        int feedback = data[i] ^ bb[length - k - 1];
        if (feedback != 0) {
            for (int j = length - k - 1; j > 0; j--)
                if (g[j] != 0)
                    bb[j] = bb[j - 1] ^ feedback;
                else
                    bb[j] = bb[j - 1];
            bb[0] = g[0] && feedback;
        }
        else {
            for (int j = length - k - 1; j > 0; j--)
                bb[j] = bb[j - 1];
            bb[0] = 0;
        }
    }
}
