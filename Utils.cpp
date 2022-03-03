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
*   Copyright (C) 2009,2014,2015,2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2018-2020 Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; version 2 of the License.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*/
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Constants/Macros
// ---------------------------------------------------------------------------

const uint8_t BITS_TABLE[] = {
#   define B2(n) n,     n+1,     n+1,     n+2
#   define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#   define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
};

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
/// <summary>
/// Displays the host version.
/// </summary>
void getHostVersion()
{
    LogInfo(__PROG_NAME__ " %s (built %s)", __VER__, __BUILD__);
}

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------
/// <summary>
///
/// </summary>
/// <param name="title"></param>
/// <param name="data"></param>
/// <param name="length"></param>
void Utils::dump(const std::string& title, const uint8_t* data, uint32_t length)
{
    assert(data != NULL);

    dump(2U, title, data, length);
}

/// <summary>
///
/// </summary>
/// <param name="level"></param>
/// <param name="title"></param>
/// <param name="data"></param>
/// <param name="length"></param>
void Utils::dump(int level, const std::string& title, const uint8_t* data, uint32_t length)
{
    assert(data != NULL);

    ::Log(level, "DUMP", "%s", title.c_str());

    uint32_t offset = 0U;

    while (length > 0U) {
        std::string output;

        uint32_t bytes = (length > 16U) ? 16U : length;

        for (uint32_t i = 0U; i < bytes; i++) {
            char temp[10U];
            ::sprintf(temp, "%02X ", data[offset + i]);
            output += temp;
        }

        for (uint32_t i = bytes; i < 16U; i++)
            output += "   ";

        output += "   *";

        for (uint32_t i = 0U; i < bytes; i++) {
            uint8_t c = data[offset + i];

            if (::isprint(c))
                output += c;
            else
                output += '.';
        }

        output += '*';

        ::Log(level, "DUMP", "%04X:  %s", offset, output.c_str());

        offset += 16U;

        if (length >= 16U)
            length -= 16U;
        else
            length = 0U;
    }
}

/// <summary>
///
/// </summary>
/// <param name="title"></param>
/// <param name="bits"></param>
/// <param name="length"></param>
void Utils::dump(const std::string& title, const bool* bits, uint32_t length)
{
    assert(bits != NULL);

    dump(2U, title, bits, length);
}

/// <summary>
///
/// </summary>
/// <param name="level"></param>
/// <param name="title"></param>
/// <param name="bits"></param>
/// <param name="length"></param>
void Utils::dump(int level, const std::string& title, const bool* bits, uint32_t length)
{
    assert(bits != NULL);

    uint8_t bytes[100U];
    uint32_t nBytes = 0U;
    for (uint32_t n = 0U; n < length; n += 8U, nBytes++)
        bitsToByteBE(bits + n, bytes[nBytes]);

    dump(level, title, bytes, nBytes);
}

/// <summary>
///
/// </summary>
/// <param name="title"></param>
/// <param name="data"></param>
/// <param name="length"></param>
void Utils::symbols(const std::string& title, const uint8_t* data, uint32_t length)
{
    assert(data != NULL);

    ::Log(2U, "SYMBOLS", "%s", title.c_str());

    uint32_t offset = 0U;
    uint32_t count = 0U;

    std::string microslotHeader;
    for (unsigned j = 0; j < 2; j++) {
        char temp[27U];
        ::sprintf(temp, "_____________%u____________", j);
        microslotHeader += temp;

        ::sprintf(temp, "    ");
        microslotHeader += temp;
    }

    ::Log(2U, "SYMBOLS", "MCR:  %s", microslotHeader.c_str());

    uint32_t bufLen = length;
    while (bufLen > 0U) {
        std::string output;

        uint32_t symOffset = offset;

        // iterate through bytes in groups of 2
        for (unsigned j = 0; j < 2U; j++) {
            if (symOffset + 1 > length) {
                break;
            }

            for (unsigned i = 0U; i < 9U; i++) {
                if (symOffset + i > length) {
                    break;
                }

                char temp[10U];
                ::sprintf(temp, "%02X ", data[symOffset + i]);
                output += temp;
            }

            char temp[10U];
            ::sprintf(temp, "   ");
            output += temp;
            symOffset += 9;
        }

        ::Log(2U, "SYMBOLS", "%03u:  %s", count, output.c_str());

        offset += 18U;
        count += 2U;

        if (bufLen >= 18U)
            bufLen -= 18U;
        else
            bufLen = 0U;
    }
}

/// <summary>
///
/// </summary>
/// <param name="byte"></param>
/// <param name="bits"></param>
void Utils::byteToBitsBE(uint8_t byte, bool* bits)
{
    assert(bits != NULL);

    bits[0U] = (byte & 0x80U) == 0x80U;
    bits[1U] = (byte & 0x40U) == 0x40U;
    bits[2U] = (byte & 0x20U) == 0x20U;
    bits[3U] = (byte & 0x10U) == 0x10U;
    bits[4U] = (byte & 0x08U) == 0x08U;
    bits[5U] = (byte & 0x04U) == 0x04U;
    bits[6U] = (byte & 0x02U) == 0x02U;
    bits[7U] = (byte & 0x01U) == 0x01U;
}

/// <summary>
///
/// </summary>
/// <param name="byte"></param>
/// <param name="bits"></param>
void Utils::byteToBitsLE(uint8_t byte, bool* bits)
{
    assert(bits != NULL);

    bits[0U] = (byte & 0x01U) == 0x01U;
    bits[1U] = (byte & 0x02U) == 0x02U;
    bits[2U] = (byte & 0x04U) == 0x04U;
    bits[3U] = (byte & 0x08U) == 0x08U;
    bits[4U] = (byte & 0x10U) == 0x10U;
    bits[5U] = (byte & 0x20U) == 0x20U;
    bits[6U] = (byte & 0x40U) == 0x40U;
    bits[7U] = (byte & 0x80U) == 0x80U;
}

/// <summary>
///
/// </summary>
/// <param name="bits"></param>
/// <param name="byte"></param>
void Utils::bitsToByteBE(const bool* bits, uint8_t& byte)
{
    assert(bits != NULL);

    byte = bits[0U] ? 0x80U : 0x00U;
    byte |= bits[1U] ? 0x40U : 0x00U;
    byte |= bits[2U] ? 0x20U : 0x00U;
    byte |= bits[3U] ? 0x10U : 0x00U;
    byte |= bits[4U] ? 0x08U : 0x00U;
    byte |= bits[5U] ? 0x04U : 0x00U;
    byte |= bits[6U] ? 0x02U : 0x00U;
    byte |= bits[7U] ? 0x01U : 0x00U;
}

/// <summary>
///
/// </summary>
/// <param name="bits"></param>
/// <param name="byte"></param>
void Utils::bitsToByteLE(const bool* bits, uint8_t& byte)
{
    assert(bits != NULL);

    byte = bits[0U] ? 0x01U : 0x00U;
    byte |= bits[1U] ? 0x02U : 0x00U;
    byte |= bits[2U] ? 0x04U : 0x00U;
    byte |= bits[3U] ? 0x08U : 0x00U;
    byte |= bits[4U] ? 0x10U : 0x00U;
    byte |= bits[5U] ? 0x20U : 0x00U;
    byte |= bits[6U] ? 0x40U : 0x00U;
    byte |= bits[7U] ? 0x80U : 0x00U;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="start"></param>
/// <param name="stop"></param>
uint32_t Utils::getBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop)
{
    assert(in != NULL);
    assert(out != NULL);

    uint32_t n = 0U;
    for (uint32_t i = start; i < stop; i++, n++) {
        bool b = READ_BIT(in, i);
        WRITE_BIT(out, n, b);
    }

    return n;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="start"></param>
/// <param name="length"></param>
uint32_t Utils::getBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length)
{
    return getBits(in, out, start, start + length);
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="start"></param>
/// <param name="stop"></param>
uint32_t Utils::setBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop)
{
    assert(in != NULL);
    assert(out != NULL);

    uint32_t n = 0U;
    for (uint32_t i = start; i < stop; i++, n++) {
        bool b = READ_BIT(in, n);
        WRITE_BIT(out, i, b);
    }

    return n;
}

/// <summary>
///
/// </summary>
/// <param name="in"></param>
/// <param name="out"></param>
/// <param name="start"></param>
/// <param name="length"></param>
uint32_t Utils::setBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length)
{
    return setBits(in, out, start, start + length);
}

/// <summary>
/// Returns the count of bits in the passed 8 byte value.
/// </summary>
/// <param name="bits"></param>
/// <returns></returns>
uint8_t Utils::countBits8(uint8_t bits)
{
    return BITS_TABLE[bits];
}

/// <summary>
/// Returns the count of bits in the passed 32 byte value.
/// </summary>
/// <param name="bits"></param>
/// <returns></returns>
uint8_t Utils::countBits32(uint32_t bits)
{
    uint8_t* p = (uint8_t*)&bits;
    uint8_t n = 0U;
    n += BITS_TABLE[p[0U]];
    n += BITS_TABLE[p[1U]];
    n += BITS_TABLE[p[2U]];
    n += BITS_TABLE[p[3U]];
    return n;
}

/// <summary>
/// Returns the count of bits in the passed 64 byte value.
/// </summary>
/// <param name="bits"></param>
/// <returns></returns>
uint8_t Utils::countBits64(ulong64_t bits)
{
    uint8_t* p = (uint8_t*)&bits;
    uint8_t n = 0U;
    n += BITS_TABLE[p[0U]];
    n += BITS_TABLE[p[1U]];
    n += BITS_TABLE[p[2U]];
    n += BITS_TABLE[p[3U]];
    n += BITS_TABLE[p[4U]];
    n += BITS_TABLE[p[5U]];
    n += BITS_TABLE[p[6U]];
    n += BITS_TABLE[p[7U]];
    return n;
}
