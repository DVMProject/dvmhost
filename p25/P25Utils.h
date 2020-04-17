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
    //      This class implements various helper functions for interleaving P25
    //      data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API P25Utils {
    public:
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
