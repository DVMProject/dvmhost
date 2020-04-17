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
#if !defined(__RS634717_H__)
#define __RS634717_H__

#include "Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Reed-Solomon (63,47,17). Which is also used to implement
    //      Reed-Solomon (24,12,13), (24,16,9) and (36,20,17) forward
    //      error correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API RS634717 {
    public:
        /// <summary>Initializes a new instance of the RS634717 class.</summary>
        RS634717();
        /// <summary>Finalizes a instance of the RS634717 class.</summary>
        ~RS634717();

        /// <summary>Decode RS (24,12,13) FEC.</summary>
        bool decode241213(uint8_t* data);
        /// <summary>Encode RS (24,12,13) FEC.</summary>
        void encode241213(uint8_t* data);

        /// <summary>Decode RS (24,16,9) FEC.</summary>
        bool decode24169(uint8_t* data);
        /// <summary>Encode RS (24,16,9) FEC.</summary>
        void encode24169(uint8_t* data);

        /// <summary>Decode RS (36,20,17) FEC.</summary>
        bool decode362017(uint8_t* data);
        /// <summary>Encode RS (36,20,17) FEC.</summary>
        void encode362017(uint8_t* data);

    private:
        /// <summary></summary>
        static uint8_t bin2Hex(const uint8_t* input, uint32_t offset);
        /// <summary></summary>
        static void hex2Bin(uint8_t input, uint8_t* output, uint32_t offset);

        /// <summary></summary>
        uint8_t gf6Mult(uint8_t a, uint8_t b) const;
        /// <summary>Decode variable length Reed-Solomon FEC.</summary>
        bool decode(uint8_t* data, const uint32_t bitLength, const int firstData, const int roots);
    };
} // namespace edac

#endif // __RS634717_H__
