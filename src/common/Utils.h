// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2009,2014,2015 Jonathan Naylor, G4KLX
 *  Copyright (C) 2018-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup utils Utility Routines
 * @brief Defines and implements utility routines.
 * @ingroup common
 * 
 * @file Utils.h
 * @ingroup utils
 * @file Utils.cpp
 * @ingroup utils
 */
#if !defined(__UTILS_H__)
#define __UTILS_H__

#include "common/Defines.h"

#include <cstring>
#include <string>

#if !defined(_WIN32)
#include <arpa/inet.h>
#endif // !defined(WIN32)

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

/**
 * @brief Bit mask table used for WRITE_BIT and READ_BIT.
 * @ingroup utils
 */
const uint8_t   BIT_MASK_TABLE[] = { 0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U };

// ---------------------------------------------------------------------------
//  Inlines
// ---------------------------------------------------------------------------

/**
 * @brief String from boolean.
 * @ingroup utils
 * @param value Boolean value to convert.
 * @return std::string String representation of the boolean value.
 */
inline std::string __BOOL_STR(const bool& value) {
    std::stringstream ss;
    ss << std::boolalpha << value;
    return ss.str();
}

/**
 * @brief String from integer number.
 * @ingroup utils
 * @param value Integer value to convert.
 * @return std::string String representation of the integer value.
 */
inline std::string __INT_STR(const int& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

/**
 * @brief String from hex integer number.
 * @ingroup utils
 * @param value Integer value to convert.
 * @return std::string String representation of the integer value in hexadecmial.
 */
inline std::string __INT_HEX_STR(const int& value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

/**
 * @brief String from floating point number.
 * @ingroup utils
 * @param value Floating point value to convert.
 * @return std::string String representation of the floating point value.
 */
inline std::string __FLOAT_STR(const float& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

/**
 * @brief IP address from uint32_t value.
 * @ingroup utils
 * @param value Packed IP address.
 * @return std::string String representation of the packed IP address.
 */
inline std::string __IP_FROM_UINT(const uint32_t& value) {
    std::stringstream ss;
    ss << ((value >> 24) & 0xFFU) << "." << ((value >> 16) & 0xFFU) << "." << ((value >> 8) & 0xFFU) << "." << (value & 0xFFU);
    return ss.str();
}

/**
 * @brief IP address from uint32_t value.
 * @ingroup utils
 * @param value String representation of the IP address.
 * @return uint32_t Packed IP address.
 */
#if !defined(_WIN32)
inline uint32_t __IP_FROM_STR(const std::string& value) {
    struct sockaddr_in sa;
    inet_pton(AF_INET, value.c_str(), &(sa.sin_addr));

    uint8_t ip[4U];
    ::memset(ip, 0x00U, 4U);

    ip[3U] = ((uint32_t)sa.sin_addr.s_addr >> 24) & 0xFFU;
    ip[2U] = ((uint32_t)sa.sin_addr.s_addr >> 16) & 0xFFU;
    ip[1U] = ((uint32_t)sa.sin_addr.s_addr >> 8) & 0xFFU;
    ip[0U] = ((uint32_t)sa.sin_addr.s_addr >> 0) & 0xFFU;

    return (ip[0U] << 24) | (ip[1U] << 16) | (ip[2U] << 8)  | (ip[3U] << 0);
}
#else
extern HOST_SW_API uint32_t __IP_FROM_STR(const std::string& value);
#endif // !defined(_WIN32)

/**
 * @brief Helper to lower-case an input string.
 * @ingroup utils
 * @param value String to lower-case.
 * @return std::string Lowercased string.
 */
inline std::string strtolower(const std::string value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    return v;
}

/**
 * @brief Helper to upper-case an input string.
 * @ingroup utils
 * @param value String to upper-case.
 * @return std::string Uppercased string.
 */
inline std::string strtoupper(const std::string value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), ::toupper);
    return v;
}

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

/**
 * @brief Pointer magic to get the memory address of a floating point number.
 * @ingroup utils
 * @param x Floating Point Variable
 */
#define __FLOAT_ADDR(x)  (*(uint32_t*)& x)
/**
 * @brief Pointer magic to get the memory address of a double precision number.
 * @ingroup utils
 * @param x Double Precision Variable
 */
#define __DOUBLE_ADDR(x) (*(uint64_t*)& x)

/**
 * @brief Macro helper to write a specific bit in a byte array.
 * @ingroup utils
 * @param p Byte array.
 * @param i Bit offset.
 * @param b Bit to write.
 */
#define WRITE_BIT(p, i, b) p[(i) >> 3] = (b) ? (p[(i) >> 3] | BIT_MASK_TABLE[(i) & 7]) : (p[(i) >> 3] & ~BIT_MASK_TABLE[(i) & 7])
/**
 * @brief Macro helper to read a specific bit from a byte array.
 * @ingroup utils
 * @param p Byte array.
 * @param i Bit offset.
 * @returns bool Bit.
 */
#define READ_BIT(p, i)     (p[(i) >> 3] & BIT_MASK_TABLE[(i) & 7])

/**
 * @brief Sets a uint32_t into 4 bytes.
 * @ingroup utils
 * @param val uint32_t value to set
 * @param buffer uint8_t buffer to set value on
 * @param offset Offset within uint8_t buffer
 */
#define __SET_UINT32(val, buffer, offset)               \
            buffer[0U + offset] = (val >> 24) & 0xFFU;  \
            buffer[1U + offset] = (val >> 16) & 0xFFU;  \
            buffer[2U + offset] = (val >> 8) & 0xFFU;   \
            buffer[3U + offset] = (val >> 0) & 0xFFU;
/**
 * @brief Gets a uint32_t consisting of 4 bytes.
 * @ingroup utils
 * @param buffer uint8_t buffer to get value from
 * @param offset Offset within uint8_t buffer
 */
#define __GET_UINT32(buffer, offset)                    \
            (buffer[offset + 0U] << 24)     |           \
            (buffer[offset + 1U] << 16)     |           \
            (buffer[offset + 2U] << 8)      |           \
            (buffer[offset + 3U] << 0);
/**
 * @brief Sets a uint32_t into 3 bytes.
 * @ingroup utils
 * @param val uint32_t value to set
 * @param buffer uint8_t buffer to set value on
 * @param offset Offset within uint8_t buffer
 */
#define __SET_UINT16(val, buffer, offset)               \
            buffer[0U + offset] = (val >> 16) & 0xFFU;  \
            buffer[1U + offset] = (val >> 8) & 0xFFU;   \
            buffer[2U + offset] = (val >> 0) & 0xFFU;
/**
 * @brief Gets a uint32_t consisting of 3 bytes. (This is a shortened uint32_t).
 * @ingroup utils
 * @param buffer uint8_t buffer to get value from
 * @param offset Offset within uint8_t buffer
 */
#define __GET_UINT16(buffer, offset)                    \
            (buffer[offset + 0U] << 16)     |           \
            (buffer[offset + 1U] << 8)      |           \
            (buffer[offset + 2U] << 0);
/**
 * @brief Sets a uint16_t into 2 bytes.
 * @ingroup utils
 * @param val uint16_t value to set
 * @param buffer uint8_t buffer to set value on
 * @param offset Offset within uint8_t buffer
 */
#define __SET_UINT16B(val, buffer, offset)              \
            buffer[0U + offset] = (val >> 8) & 0xFFU;   \
            buffer[1U + offset] = (val >> 0) & 0xFFU;
/**
 * @brief Gets a uint16_t consisting of 2 bytes.
 * @ingroup utils
 * @param buffer uint8_t buffer to get value from
 * @param offset Offset within uint8_t buffer
 */
#define __GET_UINT16B(buffer, offset)                   \
            ((buffer[offset + 0U] << 8) & 0xFF00U)  |   \
            ((buffer[offset + 1U] << 0) & 0x00FFU);

/**
 * @brief Unique uint8_t array.
 * @ingroup utils
 */
typedef std::unique_ptr<uint8_t[]> UInt8Array;
/**
 * @brief Unique char array.
 * @ingroup utils
 */
typedef std::unique_ptr<char[]> CharArray;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Various helper utilities.
 * @ingroup utils
 */
class HOST_SW_API Utils {
public:
    /**
     * @brief Helper to dump the input buffer and display the hexadecimal output in the log.
     * @param title Name of buffer.
     * @param data Buffer to dump.
     * @param length Length of buffer.
     */
    static void dump(const std::string& title, const uint8_t* data, uint32_t length);
    /**
     * @brief Helper to dump the input buffer and display the hexadecimal output in the log.
     * @param level Log level.
     * @param title Name of buffer.
     * @param data Buffer to dump.
     * @param length Length of buffer.
     */
    static void dump(int level, const std::string& title, const uint8_t* data, uint32_t length);

    /**
     * @brief Helper to dump the input boolean bit buffer and display the hexadecimal output in the log.
     * @param title Name of buffer.
     * @param data Buffer to dump.
     * @param length Length of buffer.
     */
    static void dump(const std::string& title, const bool* bits, uint32_t length);
    /**
     * @brief Helper to dump the input boolean bit buffer and display the hexadecimal output in the log.
     * @param level Log level.
     * @param title Name of buffer.
     * @param data Buffer to dump.
     * @param length Length of buffer.
     */
    static void dump(int level, const std::string& title, const bool* bits, uint32_t length);

    /**
     * @brief Helper to dump the input buffer and display the output as a symbolic microslot output.
     * @param title Name of buffer.
     * @param data Buffer to dump.
     * @param length Length of buffer.
     */
    static void symbols(const std::string& title, const uint8_t* data, uint32_t length);

    /**
     * @brief Helper to convert the input byte to a boolean array of bits in big-endian.
     * @param byte Input byte.
     * @param bits Output bits array.
     */
    static void byteToBitsBE(uint8_t byte, bool* bits);
    /**
     * @brief Helper to convert the input byte to a boolean array of bits in little-endian.
     * @param byte Input byte.
     * @param bits Output bits array.
     */
    static void byteToBitsLE(uint8_t byte, bool* bits);

    /**
     * @brief Helper to convert the input boolean array of bits to a byte in big-endian.
     * @param byte Input bits array.
     * @param bits Output byte.
     */
    static void bitsToByteBE(const bool* bits, uint8_t& byte);
    /**
     * @brief Helper to convert the input boolean array of bits to a byte in little-endian.
     * @param byte Input bits array.
     * @param bits Output byte.
     */
    static void bitsToByteLE(const bool* bits, uint8_t& byte);

    /**
     * @brief Helper to reverse the endianness of the passed value.
     * @param value Value to reverse.
     * @returns uint16_t Endian reversed output.
     */
    static uint16_t reverseEndian(uint16_t value);
    /**
     * @brief Helper to reverse the endianness of the passed value.
     * @param value Value to reverse.
     * @returns uint32_t Endian reversed output.
     */
    static uint32_t reverseEndian(uint32_t value);
    /**
     * @brief Helper to reverse the endianness of the passed value.
     * @param value Value to reverse.
     * @returns uint64_t Endian reversed output.
     */
    static uint64_t reverseEndian(uint64_t value);

    /**
     * @brief Helper to retreive arbitrary length of bits from an input buffer.
     * @param in Input buffer.
     * @param out Output buffer.
     * @param start Starting bit offset in input buffer to read from.
     * @param stop Ending bit offset in input buffer to stop reading.
     * @returns uint32_t Count of bits read.
     */
    static uint32_t getBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
    /**
     * @brief Helper to retreive arbitrary length of bits from an input buffer.
     * @param in Input buffer.
     * @param out Output buffer.
     * @param start Starting bit offset in input buffer to read from.
     * @param length Number of bits to read.
     * @returns uint32_t Count of bits read.
     */
    static uint32_t getBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length);
    /**
     * @brief Helper to set an arbitrary length of bits from an input buffer.
     * @param in Input buffer.
     * @param out Output buffer.
     * @param start Starting bit offset in input buffer to read from.
     * @param stop Ending bit offset in input buffer to stop reading.
     * @returns uint32_t Count of bits set.
     */
    static uint32_t setBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
    /**
     * @brief Helper to set arbitrary length of bits from an input buffer.
     * @param in Input buffer.
     * @param out Output buffer.
     * @param start Starting bit offset in input buffer to read from.
     * @param length Number of bits to set.
     * @returns uint32_t Count of bits set.
     */
    static uint32_t setBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length);

    /**
     * @brief Helper to convert a binary input buffer into representative 6-bit byte.
     * @param input Input buffer.
     * @param offset Buffer offset.
     * @returns uint8_t Representative 6-bit output.
     */
    static uint8_t bin2Hex(const uint8_t* input, uint32_t offset);
    /**
     * @brief Helper to convert 6-bit input byte into representative binary buffer.
     * @param input Input 6-bit byte.
     * @param output Output buffer.
     * @param offset Buffer offset.
     */
    static void hex2Bin(const uint8_t input, uint8_t* output, uint32_t offset);

    /**
     * @brief Returns the count of bits in the passed 8 byte value.
     * @param bits uint8_t to count bits for.
     * @returns uint8_t Count of bits in passed value.
     */
    static uint8_t countBits8(uint8_t bits);
    /**
     * @brief Returns the count of bits in the passed 32 byte value.
     * @param bits uint32_t to count bits for.
     * @returns uint8_t Count of bits in passed value.
     */
    static uint8_t countBits32(uint32_t bits);
    /**
     * @brief Returns the count of bits in the passed 64 byte value.
     * @param bits ulong64_t to count bits for.
     * @returns uint8_t Count of bits in passed value.
     */
    static uint8_t countBits64(ulong64_t bits);
};

#endif // __UTILS_H__
