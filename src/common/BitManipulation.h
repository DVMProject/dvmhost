// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file BitManipulation.h
 * @ingroup common
 */
#pragma once
#if !defined(__BIT_MANIPULATION_H__)
#define __BIT_MANIPULATION_H__

#if !defined(__COMMON_DEFINES_H__)
#warning "BitManipulation.h included before Defines.h, please check include order"
#include "common/Defines.h"
#endif

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

/**
 * @brief Bit mask table used for WRITE_BIT and READ_BIT.
 * @ingroup utils
 */
const uint8_t   BIT_MASK_TABLE[] = { 0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U };

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

/**
 * @brief Macro helper to write a specific bit in a byte array.
 * @ingroup common
 * @param p Byte array.
 * @param i Bit offset.
 * @param b Bit to write.
 */
#define WRITE_BIT(p, i, b) p[(i) >> 3] = (b) ? (p[(i) >> 3] | BIT_MASK_TABLE[(i) & 7]) : (p[(i) >> 3] & ~BIT_MASK_TABLE[(i) & 7])
/**
 * @brief Macro helper to read a specific bit from a byte array.
 * @ingroup common
 * @param p Byte array.
 * @param i Bit offset.
 * @returns bool Bit.
 */
#define READ_BIT(p, i)     (p[(i) >> 3] & BIT_MASK_TABLE[(i) & 7])

/**
 * @brief Sets a uint32_t into 4 bytes of a buffer/array. (32-bit value).
 * @ingroup common
 * @param val uint32_t value to set
 * @param buffer uint8_t buffer to set value on
 * @param offset Offset within uint8_t buffer
 */
#define SET_UINT32(val, buffer, offset)                 \
            buffer[0U + offset] = (val >> 24) & 0xFFU;  \
            buffer[1U + offset] = (val >> 16) & 0xFFU;  \
            buffer[2U + offset] = (val >> 8) & 0xFFU;   \
            buffer[3U + offset] = (val >> 0) & 0xFFU;
/**
 * @brief Gets a uint32_t consisting of 4 bytes from a buffer/array. (32-bit value).
 * @ingroup common
 * @param buffer uint8_t buffer to get value from
 * @param offset Offset within uint8_t buffer
 */
#define GET_UINT32(buffer, offset)                      \
            (buffer[offset + 0U] << 24)     |           \
            (buffer[offset + 1U] << 16)     |           \
            (buffer[offset + 2U] << 8)      |           \
            (buffer[offset + 3U] << 0);
/**
 * @brief Sets a uint32_t into 3 bytes of a buffer/array. (24-bit value).
 * @ingroup common
 * @param val uint32_t value to set
 * @param buffer uint8_t buffer to set value on
 * @param offset Offset within uint8_t buffer
 */
#define SET_UINT24(val, buffer, offset)                 \
            buffer[0U + offset] = (val >> 16) & 0xFFU;  \
            buffer[1U + offset] = (val >> 8) & 0xFFU;   \
            buffer[2U + offset] = (val >> 0) & 0xFFU;
/**
 * @brief Gets a uint32_t consisting of 3 bytes from a buffer/array. (24-bit value).
 * @ingroup common
 * @param buffer uint8_t buffer to get value from
 * @param offset Offset within uint8_t buffer
 */
#define GET_UINT24(buffer, offset)                      \
            (buffer[offset + 0U] << 16)     |           \
            (buffer[offset + 1U] << 8)      |           \
            (buffer[offset + 2U] << 0);
/**
 * @brief Sets a uint16_t into 2 bytes of a buffer/array. (16-bit value).
 * @ingroup common
 * @param val uint16_t value to set
 * @param buffer uint8_t buffer to set value on
 * @param offset Offset within uint8_t buffer
 */
#define SET_UINT16(val, buffer, offset)                 \
            buffer[0U + offset] = (val >> 8) & 0xFFU;   \
            buffer[1U + offset] = (val >> 0) & 0xFFU;
/**
 * @brief Gets a uint16_t consisting of 2 bytes from a buffer/array. (16-bit value).
 * @ingroup common
 * @param buffer uint8_t buffer to get value from
 * @param offset Offset within uint8_t buffer
 */
#define GET_UINT16(buffer, offset)                      \
            ((buffer[offset + 0U] << 8) & 0xFF00U)  |   \
            ((buffer[offset + 1U] << 0) & 0x00FFU);

#endif // __BIT_MANIPULATION_H__