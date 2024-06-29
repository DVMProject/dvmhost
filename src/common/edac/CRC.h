// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2018,2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file CRC.h
 * @ingroup edac
 * @file CRC.cpp
 * @ingroup edac
 */
#if !defined(__CRC_H__)
#define __CRC_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements various Cyclic Redundancy Check routines.
     * @ingroup edac
     */
    class HOST_SW_API CRC {
    public:
        /**
         * @brief Check 5-bit CRC.
         * @param in Boolean bit array.
         * @param tcrc Computed CRC to check.
         * @returns bool True, if CRC is valid, otherwise false.
         */
        static bool checkFiveBit(bool* in, uint32_t tcrc);
        /**
         * @brief Encode 5-bit CRC.
         * @param in Boolean bit array.
         * @param tcrc Computed CRC.
         */
        static void encodeFiveBit(const bool* in, uint32_t& tcrc);

        /**
         * @brief Check 16-bit CRC CCITT-162.
         * 
         * This uses polynomial 0x1021.
         * 
         * @param[in] in Input byte array.
         * @param length Length of byte array.
         * @returns bool True, if CRC is valid, otherwise false.
         */
        static bool checkCCITT162(const uint8_t* in, uint32_t length);
        /**
         * @brief Encode 16-bit CRC CCITT-162.
         * 
         * This uses polynomial 0x1021.
         * 
         * @param[out] in Input byte array.
         * @param length Length of byte array.
         */
        static void addCCITT162(uint8_t* in, uint32_t length);

        /**
         * @brief Check 16-bit CRC CCITT-161.
         * 
         * This uses polynomial 0x1189.
         * 
         * @param[in] in Input byte array.
         * @param length Length of byte array.
         * @returns bool True, if CRC is valid, otherwise false.
         */
        static bool checkCCITT161(const uint8_t* in, uint32_t length);
        /**
         * @brief Encode 16-bit CRC CCITT-161.
         * 
         * This uses polynomial 0x1189.
         * 
         * @param[out] in Input byte array.
         * @param length Length of byte array.
         */
        static void addCCITT161(uint8_t* in, uint32_t length);

        /**
         * @brief Check 32-bit CRC.
         * @param[in] in Input byte array.
         * @param length Length of byte array.
         * @returns bool True, if CRC is valid, otherwise false.
         */
        static bool checkCRC32(const uint8_t* in, uint32_t length);
        /**
         * @brief Encode 32-bit CRC.
         * @param[out] in Input byte array.
         * @param length Length of byte array.
         */
        static void addCRC32(uint8_t* in, uint32_t length);

        /**
         * @brief Generate 8-bit CRC.
         * @param[in] in Input byte array.
         * @param length Length of byte array.
         * @returns uint8_t Calculated 8-bit CRC value.
         */
        static uint8_t crc8(const uint8_t* in, uint32_t length);

        /**
         * @brief Check 6-bit CRC.
         * @param[in] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns bool True, if CRC is valid, otherwise false.
         */
        static bool checkCRC6(const uint8_t* in, uint32_t bitLength);
        /**
         * @brief Encode 6-bit CRC.
         * @param[out] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns uint8_t 6-bit CRC.
         */
        static uint8_t addCRC6(uint8_t* in, uint32_t bitLength);

        /**
         * @brief Check 12-bit CRC.
         * @param[out] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns bool True, if CRC is valid, otherwise false.
         */
        static bool checkCRC12(const uint8_t* in, uint32_t bitLength);
        /**
         * @brief Encode 12-bit CRC.
         * @param[out] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns uint16_t 12-bit CRC.
         */
        static uint16_t addCRC12(uint8_t* in, uint32_t bitLength);

        /**
         * @brief Check 15-bit CRC.
         * @param[in] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns bool True, if CRC is valid, otherwise false.
         */
        static bool checkCRC15(const uint8_t* in, uint32_t bitLength);
        /**
         * @brief Encode 15-bit CRC.
         * @param[out] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns uint16_t 15-bit CRC.
         */
        static uint16_t addCRC15(uint8_t* in, uint32_t bitLength);

        /**
         * @brief Check 16-bit CRC CCITT-162 w/ initial generator of 1.
         * @param[in] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns bool True, if CRC is valid, otherwise false.
         */
        static bool checkCRC16(const uint8_t* in, uint32_t bitLength);
        /**
         * @brief Encode 16-bit CRC CCITT-162 w/ initial generator of 1.
         * @param[out] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @param offset Offset in bits to write CRC.
         * @returns uint16_t 16-bit CRC.
         */
        static uint16_t addCRC16(uint8_t* in, uint32_t bitLength);

        /**
         * @brief Generate 9-bit CRC.
         * @param[in] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns uint16_t 9-bit CRC.
         */
        static uint16_t createCRC9(const uint8_t* in, uint32_t bitLength);
        /**
         * @brief Generate 16-bit CRC.
         * @param[in] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns uint16_t 16-bit CRC.
         */
        static uint16_t createCRC16(const uint8_t* in, uint32_t bitLength);

    private:
        /**
         * @brief Generate 6-bit CRC.
         * @param[in] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns uint8_t 6-bit CRC.
         */
        static uint8_t createCRC6(const uint8_t* in, uint32_t bitLength);
        /**
         * @brief Generate 12-bit CRC.
         * @param[in] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns uint16_t 12-bit CRC.
         */
        static uint16_t createCRC12(const uint8_t* in, uint32_t bitLength);
        /**
         * @brief Generate 15-bit CRC.
         * @param[in] in Input byte array.
         * @param bitLength Length of byte array in bits.
         * @returns uint16_t 15-bit CRC.
         */
        static uint16_t createCRC15(const uint8_t* in, uint32_t bitLength);
    };
} // namespace edac

#endif // __CRC_H__
