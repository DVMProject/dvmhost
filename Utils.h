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

#include "Defines.h"

#include <string>

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

extern "C" {
    /// <summary>Displays the host version.</summary>
    HOST_SW_API void getHostVersion();
}

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
    static uint32_t getBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
    /// <summary></summary>
    static uint32_t getBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length);
    /// <summary></summary>
    static uint32_t setBits(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
    /// <summary></summary>
    static uint32_t setBitRange(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t length);

    /// <summary>Returns the count of bits in the passed 8 byte value.</summary>
    static uint8_t countBits8(uint8_t bits);
    /// <summary>Returns the count of bits in the passed 32 byte value.</summary>
    static uint8_t countBits32(uint32_t bits);
    /// <summary>Returns the count of bits in the passed 64 byte value.</summary>
    static uint8_t countBits64(ulong64_t bits);
};

#endif // __UTILS_H__
