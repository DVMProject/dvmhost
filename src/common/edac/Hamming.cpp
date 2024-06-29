// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
#include "edac/Hamming.h"

using namespace edac;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Decode Hamming (15,11,3). */
bool Hamming::decode15113_1(bool* d)
{
    assert(d != nullptr);

    // Calculate the parity it should have
    bool c0 = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[6];
    bool c1 = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[7] ^ d[8] ^ d[9];
    bool c2 = d[0] ^ d[1] ^ d[4] ^ d[5] ^ d[7] ^ d[8] ^ d[10];
    bool c3 = d[0] ^ d[2] ^ d[4] ^ d[6] ^ d[7] ^ d[9] ^ d[10];

    unsigned char n = 0U;
    n |= (c0 != d[11]) ? 0x01U : 0x00U;
    n |= (c1 != d[12]) ? 0x02U : 0x00U;
    n |= (c2 != d[13]) ? 0x04U : 0x00U;
    n |= (c3 != d[14]) ? 0x08U : 0x00U;

    switch (n) {
        // Parity bit errors
        case 0x01U: d[11] = !d[11]; return true;
        case 0x02U: d[12] = !d[12]; return true;
        case 0x04U: d[13] = !d[13]; return true;
        case 0x08U: d[14] = !d[14]; return true;

        // Data bit errors
        case 0x0FU: d[0] = !d[0];  return true;
        case 0x07U: d[1] = !d[1];  return true;
        case 0x0BU: d[2] = !d[2];  return true;
        case 0x03U: d[3] = !d[3];  return true;
        case 0x0DU: d[4] = !d[4];  return true;
        case 0x05U: d[5] = !d[5];  return true;
        case 0x09U: d[6] = !d[6];  return true;
        case 0x0EU: d[7] = !d[7];  return true;
        case 0x06U: d[8] = !d[8];  return true;
        case 0x0AU: d[9] = !d[9];  return true;
        case 0x0CU: d[10] = !d[10]; return true;

        // No bit errors
        default: return false;
    }
}

/* Encode Hamming (15,11,3). */
void Hamming::encode15113_1(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this row should have
    d[11] = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[6];
    d[12] = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[7] ^ d[8] ^ d[9];
    d[13] = d[0] ^ d[1] ^ d[4] ^ d[5] ^ d[7] ^ d[8] ^ d[10];
    d[14] = d[0] ^ d[2] ^ d[4] ^ d[6] ^ d[7] ^ d[9] ^ d[10];
}

/* Decode Hamming (15,11,3). */
bool Hamming::decode15113_2(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this row should have
    bool c0 = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[5] ^ d[7] ^ d[8];
    bool c1 = d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[6] ^ d[8] ^ d[9];
    bool c2 = d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[7] ^ d[9] ^ d[10];
    bool c3 = d[0] ^ d[1] ^ d[2] ^ d[4] ^ d[6] ^ d[7] ^ d[10];

    unsigned char n = 0x00U;
    n |= (c0 != d[11]) ? 0x01U : 0x00U;
    n |= (c1 != d[12]) ? 0x02U : 0x00U;
    n |= (c2 != d[13]) ? 0x04U : 0x00U;
    n |= (c3 != d[14]) ? 0x08U : 0x00U;

    switch (n) {
        // Parity bit errors
        case 0x01U: d[11] = !d[11]; return true;
        case 0x02U: d[12] = !d[12]; return true;
        case 0x04U: d[13] = !d[13]; return true;
        case 0x08U: d[14] = !d[14]; return true;

        // Data bit errors
        case 0x09U: d[0] = !d[0];  return true;
        case 0x0BU: d[1] = !d[1];  return true;
        case 0x0FU: d[2] = !d[2];  return true;
        case 0x07U: d[3] = !d[3];  return true;
        case 0x0EU: d[4] = !d[4];  return true;
        case 0x05U: d[5] = !d[5];  return true;
        case 0x0AU: d[6] = !d[6];  return true;
        case 0x0DU: d[7] = !d[7];  return true;
        case 0x03U: d[8] = !d[8];  return true;
        case 0x06U: d[9] = !d[9];  return true;
        case 0x0CU: d[10] = !d[10]; return true;

        // No bit errors
        default: return false;
    }
}

/* Encode Hamming (15,11,3). */
void Hamming::encode15113_2(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this row should have
    d[11] = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[5] ^ d[7] ^ d[8];
    d[12] = d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[6] ^ d[8] ^ d[9];
    d[13] = d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[7] ^ d[9] ^ d[10];
    d[14] = d[0] ^ d[1] ^ d[2] ^ d[4] ^ d[6] ^ d[7] ^ d[10];
}

/* Decode Hamming (13,9,3). */
bool Hamming::decode1393(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this column should have
    bool c0 = d[0] ^ d[1] ^ d[3] ^ d[5] ^ d[6];
    bool c1 = d[0] ^ d[1] ^ d[2] ^ d[4] ^ d[6] ^ d[7];
    bool c2 = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[5] ^ d[7] ^ d[8];
    bool c3 = d[0] ^ d[2] ^ d[4] ^ d[5] ^ d[8];

    unsigned char n = 0x00U;
    n |= (c0 != d[9]) ? 0x01U : 0x00U;
    n |= (c1 != d[10]) ? 0x02U : 0x00U;
    n |= (c2 != d[11]) ? 0x04U : 0x00U;
    n |= (c3 != d[12]) ? 0x08U : 0x00U;

    switch (n) {
        // Parity bit errors
        case 0x01U: d[9] = !d[9];  return true;
        case 0x02U: d[10] = !d[10]; return true;
        case 0x04U: d[11] = !d[11]; return true;
        case 0x08U: d[12] = !d[12]; return true;

        // Data bit erros
        case 0x0FU: d[0] = !d[0]; return true;
        case 0x07U: d[1] = !d[1]; return true;
        case 0x0EU: d[2] = !d[2]; return true;
        case 0x05U: d[3] = !d[3]; return true;
        case 0x0AU: d[4] = !d[4]; return true;
        case 0x0DU: d[5] = !d[5]; return true;
        case 0x03U: d[6] = !d[6]; return true;
        case 0x06U: d[7] = !d[7]; return true;
        case 0x0CU: d[8] = !d[8]; return true;

        // No bit errors
        default: return false;
    }
}

/* Encode Hamming (13,9,3). */
void Hamming::encode1393(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this column should have
    d[9] = d[0] ^ d[1] ^ d[3] ^ d[5] ^ d[6];
    d[10] = d[0] ^ d[1] ^ d[2] ^ d[4] ^ d[6] ^ d[7];
    d[11] = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[5] ^ d[7] ^ d[8];
    d[12] = d[0] ^ d[2] ^ d[4] ^ d[5] ^ d[8];
}

/* Decode Hamming (10,6,3). */
bool Hamming::decode1063(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this column should have
    bool c0 = d[0] ^ d[1] ^ d[2] ^ d[5];
    bool c1 = d[0] ^ d[1] ^ d[3] ^ d[5];
    bool c2 = d[0] ^ d[2] ^ d[3] ^ d[4];
    bool c3 = d[1] ^ d[2] ^ d[3] ^ d[4];

    unsigned char n = 0x00U;
    n |= (c0 != d[6]) ? 0x01U : 0x00U;
    n |= (c1 != d[7]) ? 0x02U : 0x00U;
    n |= (c2 != d[8]) ? 0x04U : 0x00U;
    n |= (c3 != d[9]) ? 0x08U : 0x00U;

    switch (n) {
        // Parity bit errors
        case 0x01U: d[6] = !d[6]; return true;
        case 0x02U: d[7] = !d[7]; return true;
        case 0x04U: d[8] = !d[8]; return true;
        case 0x08U: d[9] = !d[9]; return true;

        // Data bit erros
        case 0x07U: d[0] = !d[0]; return true;
        case 0x0BU: d[1] = !d[1]; return true;
        case 0x0DU: d[2] = !d[2]; return true;
        case 0x0EU: d[3] = !d[3]; return true;
        case 0x0CU: d[4] = !d[4]; return true;
        case 0x03U: d[5] = !d[5]; return true;

        // No bit errors
        default: return false;
    }
}

/* Encode Hamming (10,6,3). */
void Hamming::encode1063(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this column should have
    d[6] = d[0] ^ d[1] ^ d[2] ^ d[5];
    d[7] = d[0] ^ d[1] ^ d[3] ^ d[5];
    d[8] = d[0] ^ d[2] ^ d[3] ^ d[4];
    d[9] = d[1] ^ d[2] ^ d[3] ^ d[4];
}

/* Decode Hamming (16,11,4). */
bool Hamming::decode16114(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this column should have
    bool c0 = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[5] ^ d[7] ^ d[8];
    bool c1 = d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[6] ^ d[8] ^ d[9];
    bool c2 = d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[7] ^ d[9] ^ d[10];
    bool c3 = d[0] ^ d[1] ^ d[2] ^ d[4] ^ d[6] ^ d[7] ^ d[10];
    bool c4 = d[0] ^ d[2] ^ d[5] ^ d[6] ^ d[8] ^ d[9] ^ d[10];

    // Compare these with the actual bits
    unsigned char n = 0x00U;
    n |= (c0 != d[11]) ? 0x01U : 0x00U;
    n |= (c1 != d[12]) ? 0x02U : 0x00U;
    n |= (c2 != d[13]) ? 0x04U : 0x00U;
    n |= (c3 != d[14]) ? 0x08U : 0x00U;
    n |= (c4 != d[15]) ? 0x10U : 0x00U;

    switch (n) {
        // Parity bit errors
        case 0x01U: d[11] = !d[11]; return true;
        case 0x02U: d[12] = !d[12]; return true;
        case 0x04U: d[13] = !d[13]; return true;
        case 0x08U: d[14] = !d[14]; return true;
        case 0x10U: d[15] = !d[15]; return true;

        // Data bit errors
        case 0x19U: d[0] = !d[0];  return true;
        case 0x0BU: d[1] = !d[1];  return true;
        case 0x1FU: d[2] = !d[2];  return true;
        case 0x07U: d[3] = !d[3];  return true;
        case 0x0EU: d[4] = !d[4];  return true;
        case 0x15U: d[5] = !d[5];  return true;
        case 0x1AU: d[6] = !d[6];  return true;
        case 0x0DU: d[7] = !d[7];  return true;
        case 0x13U: d[8] = !d[8];  return true;
        case 0x16U: d[9] = !d[9];  return true;
        case 0x1CU: d[10] = !d[10]; return true;

        // No bit errors
        case 0x00U: return true;

        // Unrecoverable errors
        default: return false;
    }
}

/* Encode Hamming (10,6,3). */
void Hamming::encode16114(bool* d)
{
    assert(d != nullptr);

    d[11] = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[5] ^ d[7] ^ d[8];
    d[12] = d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[6] ^ d[8] ^ d[9];
    d[13] = d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[7] ^ d[9] ^ d[10];
    d[14] = d[0] ^ d[1] ^ d[2] ^ d[4] ^ d[6] ^ d[7] ^ d[10];
    d[15] = d[0] ^ d[2] ^ d[5] ^ d[6] ^ d[8] ^ d[9] ^ d[10];
}

/* Decode Hamming (17,12,3). */
bool Hamming::decode17123(bool* d)
{
    assert(d != nullptr);

    // Calculate the checksum this column should have
    bool c0 = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[6] ^ d[7] ^ d[9];
    bool c1 = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[7] ^ d[8] ^ d[10];
    bool c2 = d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[8] ^ d[9] ^ d[11];
    bool c3 = d[0] ^ d[1] ^ d[4] ^ d[5] ^ d[7] ^ d[10];
    bool c4 = d[0] ^ d[1] ^ d[2] ^ d[5] ^ d[6] ^ d[8] ^ d[11];

    // Compare these with the actual bits
    unsigned char n = 0x00U;
    n |= (c0 != d[12]) ? 0x01U : 0x00U;
    n |= (c1 != d[13]) ? 0x02U : 0x00U;
    n |= (c2 != d[14]) ? 0x04U : 0x00U;
    n |= (c3 != d[15]) ? 0x08U : 0x00U;
    n |= (c4 != d[16]) ? 0x10U : 0x00U;

    switch (n) {
        // Parity bit errors
        case 0x01U: d[12] = !d[12]; return true;
        case 0x02U: d[13] = !d[13]; return true;
        case 0x04U: d[14] = !d[14]; return true;
        case 0x08U: d[15] = !d[15]; return true;
        case 0x10U: d[16] = !d[16]; return true;

        // Data bit errors
        case 0x1BU: d[0] = !d[0];  return true;
        case 0x1FU: d[1] = !d[1];  return true;
        case 0x17U: d[2] = !d[2];  return true;
        case 0x07U: d[3] = !d[3];  return true;
        case 0x0EU: d[4] = !d[4];  return true;
        case 0x1CU: d[5] = !d[5];  return true;
        case 0x11U: d[6] = !d[6];  return true;
        case 0x0BU: d[7] = !d[7];  return true;
        case 0x16U: d[8] = !d[8];  return true;
        case 0x05U: d[9] = !d[9];  return true;
        case 0x0AU: d[10] = !d[10]; return true;
        case 0x14U: d[11] = !d[11]; return true;

        // No bit errors
        case 0x00U: return true;

        // Unrecoverable errors
        default: return false;
    }
}

/* Encode Hamming (17,12,3). */
void Hamming::encode17123(bool* d)
{
    assert(d != nullptr);

    d[12] = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[6] ^ d[7] ^ d[9];
    d[13] = d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[7] ^ d[8] ^ d[10];
    d[14] = d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[8] ^ d[9] ^ d[11];
    d[15] = d[0] ^ d[1] ^ d[4] ^ d[5] ^ d[7] ^ d[10];
    d[16] = d[0] ^ d[1] ^ d[2] ^ d[5] ^ d[6] ^ d[8] ^ d[11];
}
