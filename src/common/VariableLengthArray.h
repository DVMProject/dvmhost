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
 * @file VariableLengthArray.h
 * @ingroup common
 */
#pragma once
#if !defined(__VARIABLE_LENGTH_ARRAY_H__)
#define __VARIABLE_LENGTH_ARRAY_H__

#if !defined(__COMMON_DEFINES_H__)
#warning "VariableLengthArray.h included before Defines.h, please check include order"
#include "common/Defines.h"
#endif

// ---------------------------------------------------------------------------
//  Types
// ---------------------------------------------------------------------------

/**
 * @addtogroup common
 * @{
 */

/**
 * @brief Unique uint8_t array.
 * @ingroup common
 */
typedef std::unique_ptr<uint8_t[]> UInt8Array;

/**
 * @brief Unique char array.
 * @ingroup common
 */
typedef std::unique_ptr<char[]> CharArray;

/**
 * @brief Unique char array.
 * @ingroup common
 */
typedef std::unique_ptr<short[]> ShortArray;

/** @} */

// ---------------------------------------------------------------------------
//  Macros
// ---------------------------------------------------------------------------

/**
 * @addtogroup common
 * @{
 */

/**
 * @brief Declares a unique uint8_t array/buffer.
 *  This macro creates a unique pointer to a uint8_t array of the specified length and initializes it to zero. 
 *  The resulting pointer is named after the parameter name passed to the macro.
 * @param name Name of array/buffer.
 * @param len Length of array/buffer.
 */
#define DECLARE_UINT8_ARRAY(name, len)                                      \
    UInt8Array __##name##__UInt8Array = std::make_unique<uint8_t[]>(len);   \
    uint8_t* name = __##name##__UInt8Array.get();                           \
    ::memset(name, 0x00U, len);

/**
 * @brief Declares a unique char array/buffer.
 *  This macro creates a unique pointer to a char array of the specified length and initializes it to zero. 
 *  The resulting pointer is named after the parameter name passed to the macro.
 * @ingroup common
 * @param name Name of array/buffer.
 * @param len Length of array/buffer.
 */
#define DECLARE_CHAR_ARRAY(name, len)                                       \
    CharArray __##name##__CharArray = std::make_unique<char[]>(len);        \
    char* name = __##name##__CharArray.get();                               \
    ::memset(name, 0, len);

/**
 * @brief Declares a unique short array/buffer.
 *  This macro creates a unique pointer to a short array of the specified length and initializes it to zero. 
 *  The resulting pointer is named after the parameter name passed to the macro.
 * @ingroup common
 * @param name Name of array/buffer.
 * @param len Length of array/buffer.
 */
#define DECLARE_SHORT_ARRAY(name, len)                                      \
    ShortArray __##name##__ShortArray = std::make_unique<short[]>(len);     \
    short* name = __##name##__ShortArray.get();                             \
    ::memset(name, 0, len);

/** @} */

#endif // __VARIABLE_LENGTH_ARRAY_H__