// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2009,2014,2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2018-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Utils.h"
#include "Log.h"

#include <cstdio>
#include <cassert>

#if defined(_WIN32)
#include <WS2tcpip.h>
#endif // defined(_WIN32)

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

#if defined(_WIN32)
/* IP address from uint32_t value. */

uint32_t __IP_FROM_STR(const std::string& value)
{
    struct sockaddr_in sa;
    inet_pton(AF_INET, value.c_str(), &(sa.sin_addr));

    uint8_t ip[4U];
    ::memset(ip, 0x00U, 4U);

    ip[3U] = ((uint32_t)sa.sin_addr.s_addr >> 24) & 0xFFU;
    ip[2U] = ((uint32_t)sa.sin_addr.s_addr >> 16) & 0xFFU;
    ip[1U] = ((uint32_t)sa.sin_addr.s_addr >> 8) & 0xFFU;
    ip[0U] = ((uint32_t)sa.sin_addr.s_addr >> 0) & 0xFFU;

    return (ip[0U] << 24) | (ip[1U] << 16) | (ip[2U] << 8) | (ip[3U] << 0);
}
#endif // defined(_WIN32)

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/* Helper to dump the input buffer and display the hexadecimal output in the log. */

void Utils::dump(const std::string& title, const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    dump(2U, title, data, length);
}

/* Helper to dump the input buffer and display the hexadecimal output in the log. */

void Utils::dump(int level, const std::string& title, const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    ::Log(level, "DUMP", "%s (len %u)", title.c_str(), length);

    uint32_t offset = 0U;

    while (length > 0U) {
        std::string output;

        uint32_t bytes = (length > 16U) ? 16U : length;

        for (uint32_t i = 0U; i < bytes; i++) {
            char temp[10U];
            ::sprintf(temp, "%02X ", data[offset + i]);
            output += temp;
        }
#if !defined(CATCH2_TEST_COMPILATION)
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
#endif
        ::Log(level, "DUMP", "%04X:  %s", offset, output.c_str());

        offset += 16U;

        if (length >= 16U)
            length -= 16U;
        else
            length = 0U;
    }
}

/* Helper to dump the input boolean bit buffer and display the hexadecimal output in the log. */

void Utils::dump(const std::string& title, const bool* bits, uint32_t length)
{
    assert(bits != nullptr);

    dump(2U, title, bits, length);
}

/* Helper to dump the input boolean bit buffer and display the hexadecimal output in the log. */

void Utils::dump(int level, const std::string& title, const bool* bits, uint32_t length)
{
    assert(bits != nullptr);

    uint8_t bytes[100U];
    uint32_t nBytes = 0U;
    for (uint32_t n = 0U; n < length; n += 8U, nBytes++)
        bitsToByteBE(bits + n, bytes[nBytes]);

    dump(level, title, bytes, nBytes);
}

/* Helper to dump the input buffer and display the output as a symbolic microslot output. */

void Utils::symbols(const std::string& title, const uint8_t* data, uint32_t length)
{
    assert(data != nullptr);

    ::Log(2U, "SYMBOLS", "%s (len %u)", title.c_str(), length);

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
                if (symOffset + i >= length) {
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

/* Helper to convert the input byte to a boolean array of bits in big-endian. */

void Utils::byteToBitsBE(uint8_t byte, bool* bits)
{
    assert(bits != nullptr);

    bits[0U] = (byte & 0x80U) == 0x80U;
    bits[1U] = (byte & 0x40U) == 0x40U;
    bits[2U] = (byte & 0x20U) == 0x20U;
    bits[3U] = (byte & 0x10U) == 0x10U;
    bits[4U] = (byte & 0x08U) == 0x08U;
    bits[5U] = (byte & 0x04U) == 0x04U;
    bits[6U] = (byte & 0x02U) == 0x02U;
    bits[7U] = (byte & 0x01U) == 0x01U;
}

/* Helper to convert the input byte to a boolean array of bits in little-endian. */

void Utils::byteToBitsLE(uint8_t byte, bool* bits)
{
    assert(bits != nullptr);

    bits[0U] = (byte & 0x01U) == 0x01U;
    bits[1U] = (byte & 0x02U) == 0x02U;
    bits[2U] = (byte & 0x04U) == 0x04U;
    bits[3U] = (byte & 0x08U) == 0x08U;
    bits[4U] = (byte & 0x10U) == 0x10U;
    bits[5U] = (byte & 0x20U) == 0x20U;
    bits[6U] = (byte & 0x40U) == 0x40U;
    bits[7U] = (byte & 0x80U) == 0x80U;
}

/* Helper to convert the input boolean array of bits to a byte in big-endian. */

void Utils::bitsToByteBE(const bool* bits, uint8_t& byte)
{
    assert(bits != nullptr);

    byte = bits[0U] ? 0x80U : 0x00U;
    byte |= bits[1U] ? 0x40U : 0x00U;
    byte |= bits[2U] ? 0x20U : 0x00U;
    byte |= bits[3U] ? 0x10U : 0x00U;
    byte |= bits[4U] ? 0x08U : 0x00U;
    byte |= bits[5U] ? 0x04U : 0x00U;
    byte |= bits[6U] ? 0x02U : 0x00U;
    byte |= bits[7U] ? 0x01U : 0x00U;
}

/* Helper to convert the input boolean array of bits to a byte in little-endian. */

void Utils::bitsToByteLE(const bool* bits, uint8_t& byte)
{
    assert(bits != nullptr);

    byte = bits[0U] ? 0x01U : 0x00U;
    byte |= bits[1U] ? 0x02U : 0x00U;
    byte |= bits[2U] ? 0x04U : 0x00U;
    byte |= bits[3U] ? 0x08U : 0x00U;
    byte |= bits[4U] ? 0x10U : 0x00U;
    byte |= bits[5U] ? 0x20U : 0x00U;
    byte |= bits[6U] ? 0x40U : 0x00U;
    byte |= bits[7U] ? 0x80U : 0x00U;
}

/* Helper to reverse the endianness of the passed value. */

uint16_t Utils::reverseEndian(uint16_t value)
{
    return (value << 8 & 0xff00) | (value >> 8);
}

/* Helper to reverse the endianness of the passed value. */

uint32_t Utils::reverseEndian(uint32_t value)
{
    return (value << 24 | (value & 0xFF00U) << 8 | (value & 0xFF0000U) >> 8 | value >> 24);
}

/* Helper to reverse the endianness of the passed value. */

uint64_t Utils::reverseEndian(uint64_t value)
{
    return (value << 56 | (value & 0xFF00U) << 40 | (value & 0xFF0000U) << 24 | (value & 0xFF000000U) << 8 | 
           (value & 0xFF00000000U) >> 8 | (value & 0xFF0000000000U) >> 24 | (value & 0xFF000000000000U) >> 40 | value >> 56);
}

/* Helper to retreive arbitrary length of bits from an input buffer. */

uint32_t Utils::getBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop)
{
    assert(in != nullptr);
    assert(out != nullptr);

    uint32_t n = 0U;
    for (uint32_t i = start; i < stop; i++, n++) {
        bool b = READ_BIT(in, i);
        WRITE_BIT(out, n, b);
    }

    return n;
}

/* Helper to retreive arbitrary length of bits from an input buffer. */

uint32_t Utils::getBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length)
{
    return getBits(in, out, start, start + length);
}

/* Helper to set an arbitrary length of bits from an input buffer. */

uint32_t Utils::setBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop)
{
    assert(in != nullptr);
    assert(out != nullptr);

    uint32_t n = 0U;
    for (uint32_t i = start; i < stop; i++, n++) {
        bool b = READ_BIT(in, n);
        WRITE_BIT(out, i, b);
    }

    return n;
}

/* Helper to set an arbitrary length of bits from an input buffer. */

uint32_t Utils::setBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length)
{
    return setBits(in, out, start, start + length);
}

/* Helper to convert a binary input buffer into representative 6-bit byte. */

uint8_t Utils::bin2Hex(const uint8_t* input, uint32_t offset)
{
    uint8_t output = 0x00U;

    output |= READ_BIT(input, offset + 0U) ? 0x20U : 0x00U;
    output |= READ_BIT(input, offset + 1U) ? 0x10U : 0x00U;
    output |= READ_BIT(input, offset + 2U) ? 0x08U : 0x00U;
    output |= READ_BIT(input, offset + 3U) ? 0x04U : 0x00U;
    output |= READ_BIT(input, offset + 4U) ? 0x02U : 0x00U;
    output |= READ_BIT(input, offset + 5U) ? 0x01U : 0x00U;

    return output;
}

/* Helper to convert 6-bit input byte into representative binary buffer. */

void Utils::hex2Bin(const uint8_t input, uint8_t* output, uint32_t offset)
{
    WRITE_BIT(output, offset + 0U, input & 0x20U);
    WRITE_BIT(output, offset + 1U, input & 0x10U);
    WRITE_BIT(output, offset + 2U, input & 0x08U);
    WRITE_BIT(output, offset + 3U, input & 0x04U);
    WRITE_BIT(output, offset + 4U, input & 0x02U);
    WRITE_BIT(output, offset + 5U, input & 0x01U);
}

/* Returns the count of bits in the passed 8 byte value. */

uint8_t Utils::countBits8(uint8_t bits)
{
    return BITS_TABLE[bits];
}

/* Returns the count of bits in the passed 32 byte value. */

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

/* Returns the count of bits in the passed 64 byte value. */

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
