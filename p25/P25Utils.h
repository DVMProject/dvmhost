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
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2021 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_UTILS_H__)
#define __P25_UTILS_H__

#include "Defines.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      This class implements various helper functions for validating and
    //      for interleaving P25 data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API P25Utils {
    public:
        /// <summary>Helper to test and clamp a P25 NAC.</summary>
        static uint32_t nac(uint32_t nac)
        {
            if (nac < 0U) { // clamp to $000
                nac = 0U;
            }
            if (nac > 0xF7DU) { // clamp to $F7D
                nac = 0xF7DU;
            }

            return nac;
        }

        /// <summary>Helper to test and clamp a P25 site ID.</summary>
        static uint8_t siteId(uint8_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFEU) { // clamp to $FE
                id = 0xFEU;
            }

            return id;
        }

        /// <summary>Helper to test and clamp a P25 network ID.</summary>
        static uint32_t netId(uint32_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFFFFEU) { // clamp to $FFFFE
                id = 0xFFFFEU;
            }

            return id;
        }

        /// <summary>Helper to test and clamp a P25 system ID.</summary>
        static uint32_t sysId(uint32_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFFEU) { // clamp to $FFE
                id = 0xFFEU;
            }

            return id;
        }

        /// <summary>Helper to test and clamp a P25 RFSS ID.</summary>
        static uint8_t rfssId(uint8_t id)
        {
            if (id == 0U) { // clamp to 1
                id = 1U;
            }
            if (id > 0xFEU) { // clamp to $FE
                id = 0xFEU;
            }

            return id;
        }

        /// <summary>Decode bit interleaving.</summary>
        static uint32_t decode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
        /// <summary>Encode bit interleaving.</summary>
        static uint32_t encode(const uint8_t* in, uint8_t* out, uint32_t start, uint32_t stop);
        /// <summary>Encode bit interleaving for a given length.</summary>
        static uint32_t encode(const uint8_t* in, uint8_t* out, uint32_t length);

        /// <summary>Compare two datasets for the given length.</summary>
        static uint32_t compare(const uint8_t* data1, const uint8_t* data2, uint32_t length);
    };
} // namespace p25

#endif // __P25_UTILS_H__
