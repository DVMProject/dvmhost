// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2018,2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__CRC_H__)
#define __CRC_H__

#include "common/Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements various Cyclic Redundancy Check routines.
    // ---------------------------------------------------------------------------

    class HOST_SW_API CRC {
    public:
        /// <summary>Check 5-bit CRC.</summary>
        static bool checkFiveBit(bool* in, uint32_t tcrc);
        /// <summary>Encode 5-bit CRC.</summary>
        static void encodeFiveBit(const bool* in, uint32_t& tcrc);

        /// <summary>Check 16-bit CRC CCITT-162.</summary>
        static bool checkCCITT162(const uint8_t* in, uint32_t length);
        /// <summary>Encode 16-bit CRC CCITT-162.</summary>
        static void addCCITT162(uint8_t* in, uint32_t length);

        /// <summary>Check 16-bit CRC CCITT-161.</summary>
        static bool checkCCITT161(const uint8_t* in, uint32_t length);
        /// <summary>Encode 16-bit CRC CCITT-161.</summary>
        static void addCCITT161(uint8_t* in, uint32_t length);

        /// <summary>Check 32-bit CRC.</summary>
        static bool checkCRC32(const uint8_t* in, uint32_t length);
        /// <summary>Encode 32-bit CRC.</summary>
        static void addCRC32(uint8_t* in, uint32_t length);

        /// <summary>Generate 8-bit CRC.</summary>
        static uint8_t crc8(const uint8_t* in, uint32_t length);

        /// <summary>Check 6-bit CRC.</summary>
        static bool checkCRC6(const uint8_t* in, uint32_t bitLength);
        /// <summary>Encode 6-bit CRC.</summary>
        static uint8_t addCRC6(uint8_t* in, uint32_t bitLength);

        /// <summary>Check 12-bit CRC.</summary>
        static bool checkCRC12(const uint8_t* in, uint32_t bitLength);
        /// <summary>Encode 12-bit CRC.</summary>
        static uint16_t addCRC12(uint8_t* in, uint32_t bitLength);

        /// <summary>Check 15-bit CRC.</summary>
        static bool checkCRC15(const uint8_t* in, uint32_t bitLength);
        /// <summary>Encode 15-bit CRC.</summary>
        static uint16_t addCRC15(uint8_t* in, uint32_t bitLength);

        /// <summary>Check 16-bit CRC CCITT-162 w/ initial generator of 1.</summary>
        static bool checkCRC16(const uint8_t* in, uint32_t bitLength);
        /// <summary>Encode 16-bit CRC CCITT-162 w/ initial generator of 1.</summary>
        static uint16_t addCRC16(uint8_t* in, uint32_t bitLength);

        /// <summary>Generate 9-bit CRC.</summary>
        static uint16_t createCRC9(const uint8_t* in, uint32_t bitLength);
        /// <summary>Generate 16-bit CRC.</summary>
        static uint16_t createCRC16(const uint8_t* in, uint32_t bitLength);

    private:
        /// <summary></summary>
        static uint8_t createCRC6(const uint8_t* in, uint32_t bitLength);
        /// <summary></summary>
        static uint16_t createCRC12(const uint8_t* in, uint32_t bitLength);
        /// <summary></summary>
        static uint16_t createCRC15(const uint8_t* in, uint32_t bitLength);
    };
} // namespace edac

#endif // __CRC_H__
