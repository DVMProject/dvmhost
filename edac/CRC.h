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
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2018 by Bryan Biedenkapp N2PLL
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
#if !defined(__CRC_H__)
#define __CRC_H__

#include "Defines.h"

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

        /// <summary>Check 16-bit CRC-CCITT.</summary>
        static bool checkCCITT162(const uint8_t* in, uint32_t length);
        /// <summary>Encode 16-bit CRC-CCITT.</summary>
        static void addCCITT162(uint8_t* in, uint32_t length);

        /// <summary>Check 16-bit CRC-CCITT.</summary>
        static bool checkCCITT161(const uint8_t* in, uint32_t length);
        /// <summary>Encode 16-bit CRC-CCITT.</summary>
        static void addCCITT161(uint8_t* in, uint32_t length);

        /// <summary>Check 32-bit CRC.</summary>
        static bool checkCRC32(const uint8_t* in, uint32_t length);
        /// <summary>Encode 32-bit CRC.</summary>
        static void addCRC32(uint8_t* in, uint32_t length);

        /// <summary>Generate 8-bit CRC.</summary>
        static uint8_t crc8(const uint8_t* in, uint32_t length);

        /// <summary>Generate 9-bit CRC.</summary>
        static uint16_t crc9(const uint8_t* in, uint32_t bitLength);
    };
} // namespace edac

#endif // __CRC_H__
