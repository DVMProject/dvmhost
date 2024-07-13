// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/P25Utils.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Helper to set the status bits on P25 frame data. */

void P25Utils::setStatusBits(uint8_t* data, uint32_t ssOffset, bool b1, bool b2)
{
    assert(data != nullptr);

    WRITE_BIT(data, ssOffset, b1);
    WRITE_BIT(data, ssOffset + 1U, b2);
}

/* Helper to add the status bits on P25 frame data. */

void P25Utils::addStatusBits(uint8_t* data, uint32_t length, bool inbound, bool control)
{
    assert(data != nullptr);

    // insert the "10" (Unknown, use for inbound or outbound) status bits
    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += P25_SS_INCREMENT) {
        uint32_t ss1Pos = ss0Pos + 1U;
        WRITE_BIT(data, ss0Pos, true);              // 1
        WRITE_BIT(data, ss1Pos, false);             // 0
    }

    // interleave the requested status bits (every other)
    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += (P25_SS_INCREMENT * 2U)) {
        uint32_t ss1Pos = ss0Pos + 1U;
        if (inbound) {
            WRITE_BIT(data, ss0Pos, false);         // 0
            WRITE_BIT(data, ss1Pos, true);          // 1
        } else {
            if (control) {
                WRITE_BIT(data, ss0Pos, true);      // 1
                WRITE_BIT(data, ss1Pos, false);     // 0
            } else {
                WRITE_BIT(data, ss0Pos, true);      // 1
                WRITE_BIT(data, ss1Pos, true);      // 1
            }
        }
    }
}

/* Helper to add the idle status bits on P25 frame data. */

void P25Utils::addIdleStatusBits(uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += (P25_SS_INCREMENT * 5U)) {
        uint32_t ss1Pos = ss0Pos + 1U;
        WRITE_BIT(data, ss0Pos, true);              // 1
        WRITE_BIT(data, ss1Pos, false);             // 0
    }
}

/* Helper to add the trunk start slot status bits on P25 frame data. */

void P25Utils::addTrunkSlotStatusBits(uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += (P25_SS_INCREMENT * 5U)) {
        uint32_t ss1Pos = ss0Pos + 1U;
        WRITE_BIT(data, ss0Pos, true);              // 1
        WRITE_BIT(data, ss1Pos, true);              // 1
    }
}

/* Decode bit interleaving. */

uint32_t P25Utils::decode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop)
{
    assert(in != nullptr);
    assert(out != nullptr);

    // Move the SSx positions to the range needed
    uint32_t ss0Pos = P25_SS0_START;
    uint32_t ss1Pos = P25_SS1_START;

    while (ss0Pos < start) {
        ss0Pos += P25_SS_INCREMENT;
        ss1Pos += P25_SS_INCREMENT;
    }

    uint32_t n = 0U;
    for (uint32_t i = start; i < stop; i++) {
        if (i == ss0Pos) {
            ss0Pos += P25_SS_INCREMENT;
        }
        else if (i == ss1Pos) {
            ss1Pos += P25_SS_INCREMENT;
        }
        else {
            bool b = READ_BIT(in, i);
            WRITE_BIT(out, n, b);
            n++;
        }
    }

    return n;
}

/* Encode bit interleaving. */

uint32_t P25Utils::encode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop)
{
    assert(in != nullptr);
    assert(out != nullptr);

    // Move the SSx positions to the range needed
    uint32_t ss0Pos = P25_SS0_START;
    uint32_t ss1Pos = P25_SS1_START;

    while (ss0Pos < start) {
        ss0Pos += P25_SS_INCREMENT;
        ss1Pos += P25_SS_INCREMENT;
    }

    uint32_t n = 0U;
    for (uint32_t i = start; i < stop; i++) {
        if (i == ss0Pos) {
            ss0Pos += P25_SS_INCREMENT;
        }
        else if (i == ss1Pos) {
            ss1Pos += P25_SS_INCREMENT;
        }
        else {
            bool b = READ_BIT(in, n);
            WRITE_BIT(out, i, b);
            n++;
        }
    }

    return n;
}

/* Encode bit interleaving. */

uint32_t P25Utils::encode(const uint8_t* in, uint8_t* out, uint32_t length)
{
    assert(in != nullptr);
    assert(out != nullptr);

    // Move the SSx positions to the range needed
    uint32_t ss0Pos = P25_SS0_START;
    uint32_t ss1Pos = P25_SS1_START;

    uint32_t n = 0U;
    uint32_t pos = 0U;
    while (n < length) {
        if (pos == ss0Pos) {
            ss0Pos += P25_SS_INCREMENT;

        }
        else if (pos == ss1Pos) {
            ss1Pos += P25_SS_INCREMENT;

        }
        else {
            bool b = READ_BIT(in, n);
            WRITE_BIT(out, pos, b);
            n++;

        }
        pos++;

    }

    return pos;
}

/* Compare two datasets for the given length. */

uint32_t P25Utils::compare(const uint8_t* data1, const uint8_t* data2, uint32_t length)
{
    assert(data1 != nullptr);
    assert(data2 != nullptr);

    uint32_t errs = 0U;
    for (uint32_t i = 0U; i < length; i++) {
        uint8_t v = data1[i] ^ data2[i];
        while (v != 0U) {
            v &= v - 1U;
            errs++;
        }
    }

    return errs;
}
