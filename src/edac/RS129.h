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
*   Copyright (C) 2015 by Jonathan Naylor G4KLX
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
#if !defined(__RS129_H__)
#define __RS129_H__

#include "Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Reed-Solomon (12,9) forward error correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RS129 {
    public:
        /// <summary>Check RS (12,9) FEC.</summary>
        static bool check(const uint8_t* in);

        /// <summary>Encode RS (12,9) FEC.</summary>
        static void encode(const uint8_t* msg, uint32_t nbytes, uint8_t* parity);

    private:
        /// <summary></summary>
        static uint8_t gmult(uint8_t a, uint8_t b);
    };
} // namespace edac

#endif // __RS129_H__
