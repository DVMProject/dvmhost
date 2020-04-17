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
*   Copyright (C) 2010,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2017 by Bryan Biedenkapp N2PLL
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
#if !defined(__GOLAY24128_H__)
#define __GOLAY24128_H__

#include "Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Golay (23,12,7) and Golay (24,12,8) forward error 
    //      correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Golay24128 {
    public:
        /// <summary>Decode Golay (23,12,7) FEC.</summary>
        static uint32_t decode23127(uint32_t code);
        /// <summary>Decode Golay (24,12,8) FEC.</summary>
        static uint32_t decode24128(uint32_t code);
        /// <summary>Decode Golay (24,12,8) FEC.</summary>
        static uint32_t decode24128(uint8_t* bytes);
        /// <summary>Decode Golay (24,12,8) FEC.</summary>
        static void decode24128(uint8_t* data, const uint8_t* raw, uint32_t msglen);

        /// <summary>Encode Golay (23,12,7) FEC.</summary>
        static uint32_t encode23127(uint32_t data);
        /// <summary>Encode Golay (24,12,8) FEC.</summary>
        static uint32_t encode24128(uint32_t data);
        /// <summary>Encode Golay (24,12,8) FEC.</summary>
        static void encode24128(uint8_t* data, const uint8_t* raw, uint32_t msglen);

    private:
        /// <summary></summary>
        static uint32_t getSyndrome23127(uint32_t pattern);
    };
} // namespace edac

#endif // __GOLAY24128_H__
