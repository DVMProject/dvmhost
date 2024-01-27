/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2009,2014,2015 by Jonathan Naylor, G4KLX
*   Copyright (C) 2018-2019 Bryan Biedenkapp N2PLL
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
#if !defined(__UTILS_H__)
#define __UTILS_H__

#include "common/Defines.h"

#include <string>

// ---------------------------------------------------------------------------
//  Inlines
// ---------------------------------------------------------------------------

/// <summary>
/// String from boolean.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
inline std::string __BOOL_STR(const bool& value) {
    std::stringstream ss;
    ss << std::boolalpha << value;
    return ss.str();
}

/// <summary>
/// String from integer number.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
inline std::string __INT_STR(const int& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

/// <summary>
/// String from hex integer number.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
inline std::string __INT_HEX_STR(const int& value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

/// <summary>
/// String from floating point number.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
inline std::string __FLOAT_STR(const float& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

/// <summary>
/// IP address from ulong64_t value.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
inline std::string __IP_FROM_ULONG(const ulong64_t& value) {
    std::stringstream ss;
    ss << ((value >> 24) & 0xFFU) << "." << ((value >> 16) & 0xFFU) << "." << ((value >> 8) & 0xFFU) << "." << (value & 0xFFU);
    return ss.str();
}

/// <summary>
/// Helper to lower-case an input string.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
inline std::string strtolower(const std::string value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    return v;
}

/// <summary>
/// Helper to upper-case an input string.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
inline std::string strtoupper(const std::string value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), ::toupper);
    return v;
}

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

/// <summary>Pointer magic to get the memory address of a floating point number.</summary>
/// <param name="x">Floating Point Variable</param>
#define __FLOAT_ADDR(x)  (*(uint32_t*)& x)
/// <summary>Pointer magic to get the memory address of a double precision number.</summary>
/// <param name="x">Double Precision Variable</param>
#define __DOUBLE_ADDR(x) (*(uint64_t*)& x)

#define WRITE_BIT(p, i, b) p[(i) >> 3] = (b) ? (p[(i) >> 3] | BIT_MASK_TABLE[(i) & 7]) : (p[(i) >> 3] & ~BIT_MASK_TABLE[(i) & 7])
#define READ_BIT(p, i)     (p[(i) >> 3] & BIT_MASK_TABLE[(i) & 7])

/// <summary>Sets a uint32_t into 4 bytes.</summary>
/// <param name="val">uint32_t value to set</param>
/// <param name="buffer">uint8_t buffer to set value on</param>
/// <param name="offset">Offset within uint8_t buffer</param>
#define __SET_UINT32(val, buffer, offset)               \
            buffer[0U + offset] = (val >> 24) & 0xFFU;  \
            buffer[1U + offset] = (val >> 16) & 0xFFU;  \
            buffer[2U + offset] = (val >> 8) & 0xFFU;   \
            buffer[3U + offset] = (val >> 0) & 0xFFU;
/// <summary>Gets a uint32_t consisting of 4 bytes.</summary>
/// <param name="buffer">uint8_t buffer to get value from</param>
/// <param name="offset">Offset within uint8_t buffer</param>
#define __GET_UINT32(buffer, offset)                    \
            (buffer[offset + 0U] << 24)     |           \
            (buffer[offset + 1U] << 16)     |           \
            (buffer[offset + 2U] << 8)      |           \
            (buffer[offset + 3U] << 0);
/// <summary>Sets a uint32_t into 3 bytes.</summary>
/// <param name="val">uint32_t value to set</param>
/// <param name="buffer">uint8_t buffer to set value on</param>
/// <param name="offset">Offset within uint8_t buffer</param>
#define __SET_UINT16(val, buffer, offset)               \
            buffer[0U + offset] = (val >> 16) & 0xFFU;  \
            buffer[1U + offset] = (val >> 8) & 0xFFU;   \
            buffer[2U + offset] = (val >> 0) & 0xFFU;
/// <summary>Gets a uint32_t consisting of 3 bytes. (This is a shortened uint32_t).</summary>
/// <param name="buffer">uint8_t buffer to get value from</param>
/// <param name="offset">Offset within uint8_t buffer</param>
#define __GET_UINT16(buffer, offset)                    \
            (buffer[offset + 0U] << 16)     |           \
            (buffer[offset + 1U] << 8)      |           \
            (buffer[offset + 2U] << 0);
/// <summary>Sets a uint16_t into 2 bytes.</summary>
/// <param name="val">uint16_t value to set</param>
/// <param name="buffer">uint8_t buffer to set value on</param>
/// <param name="offset">Offset within uint8_t buffer</param>
#define __SET_UINT16B(val, buffer, offset)              \
            buffer[0U + offset] = (val >> 8) & 0xFFU;   \
            buffer[1U + offset] = (val >> 0) & 0xFFU;
/// <summary>Gets a uint16_t consisting of 2 bytes.</summary>
/// <param name="buffer">uint8_t buffer to get value from</param>
/// <param name="offset">Offset within uint8_t buffer</param>
#define __GET_UINT16B(buffer, offset)                   \
            ((buffer[offset + 0U] << 8) & 0xFF00U)  |   \
            ((buffer[offset + 1U] << 0) & 0x00FFU);

/// <summary>Unique uint8_t array.</summary>
typedef std::unique_ptr<uint8_t[]> UInt8Array;

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements various helper utilities.
// ---------------------------------------------------------------------------

class HOST_SW_API Utils {
public:
    /// <summary></summary>
    static void dump(const std::string& title, const uint8_t* data, uint32_t length);
    /// <summary></summary>
    static void dump(int level, const std::string& title, const uint8_t* data, uint32_t length);

    /// <summary></summary>
    static void dump(const std::string& title, const bool* bits, uint32_t length);
    /// <summary></summary>
    static void dump(int level, const std::string& title, const bool* bits, uint32_t length);

    /// <summary></summary>
    static void symbols(const std::string& title, const uint8_t* data, uint32_t length);

    /// <summary></summary>
    static void byteToBitsBE(uint8_t byte, bool* bits);
    /// <summary></summary>
    static void byteToBitsLE(uint8_t byte, bool* bits);

    /// <summary></summary>
    static void bitsToByteBE(const bool* bits, uint8_t& byte);
    /// <summary></summary>
    static void bitsToByteLE(const bool* bits, uint8_t& byte);

    /// <summary></summary>
    static uint16_t reverseEndian(uint16_t value);
    /// <summary></summary>
    static uint32_t reverseEndian(uint32_t value);
    /// <summary></summary>
    static uint64_t reverseEndian(uint64_t value);

    /// <summary></summary>
    static uint32_t getBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
    /// <summary></summary>
    static uint32_t getBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length);
    /// <summary></summary>
    static uint32_t setBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
    /// <summary></summary>
    static uint32_t setBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length);

    /// <summary></summary>
    static uint8_t bin2Hex(const uint8_t* input, uint32_t offset);
    /// <summary></summary>
    static void hex2Bin(const uint8_t input, uint8_t* output, uint32_t offset);

    /// <summary>Returns the count of bits in the passed 8 byte value.</summary>
    static uint8_t countBits8(uint8_t bits);
    /// <summary>Returns the count of bits in the passed 32 byte value.</summary>
    static uint8_t countBits32(uint32_t bits);
    /// <summary>Returns the count of bits in the passed 64 byte value.</summary>
    static uint8_t countBits64(ulong64_t bits);
};

#endif // __UTILS_H__
