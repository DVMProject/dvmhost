/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/P25Utils.h"

using namespace p25;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to set the busy status bits on P25 frame data.
/// </summary>
/// <param name="data"></param>
/// <param name="ssOffset"></param>
/// <param name="b1"></param>
/// <param name="b2"></param>
void P25Utils::setBusyBits(uint8_t* data, uint32_t ssOffset, bool b1, bool b2)
{
    assert(data != nullptr);

    WRITE_BIT(data, ssOffset, b1);
    WRITE_BIT(data, ssOffset + 1U, b2);
}

/// <summary>
/// Helper to add the busy status bits on P25 frame data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="b1"></param>
/// <param name="b2"></param>
void P25Utils::addBusyBits(uint8_t* data, uint32_t length, bool b1, bool b2)
{
    assert(data != nullptr);

    // insert the "10" (Unknown, use for inbound or outbound) status bits
    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += P25_SS_INCREMENT) {
        uint32_t ss1Pos = ss0Pos + 1U;
        WRITE_BIT(data, ss0Pos, true);  // 1
        WRITE_BIT(data, ss1Pos, false); // 0
    }

    // interleave the requested status bits (every other)
    for (uint32_t ss0Pos = P25_SS0_START; ss0Pos < length; ss0Pos += (P25_SS_INCREMENT * 2)) {
        uint32_t ss1Pos = ss0Pos + 1U;
        WRITE_BIT(data, ss0Pos, b1);
        WRITE_BIT(data, ss1Pos, b2);
    }
}

/// <summary>
/// Decode bit interleaving.
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="start"></param>
/// <param name="stop"></param>
/// <returns></returns>
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

/// <summary>
/// Encode bit interleaving.
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="start"></param>
/// <param name="stop"></param>
/// <returns></returns>
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

/// <summary>
/// Encode bit interleaving.
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="length"></param>
/// <returns></returns>
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

/// <summary>
/// Compare two datasets for the given length.
/// </summary>
/// <param name="data1"></param>
/// <param name="data2"></param>
/// <param name="length"></param>
/// <returns></returns>
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
