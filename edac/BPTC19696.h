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
#if !defined(__BPTC19696_H__)
#define __BPTC19696_H__

#include "Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Block Product Turbo Code (196,96) FEC.
    // ---------------------------------------------------------------------------

    class HOST_SW_API BPTC19696 {
    public:
        /// <summary>Initializes a new instance of the BPTC19696 class.</summary>
        BPTC19696();
        /// <summary>Finalizes a instance of the BPTC19696 class.</summary>
        ~BPTC19696();

        /// <summary>Decode BPTC (196,96) FEC.</summary>
        void decode(const uint8_t* in, uint8_t* out);
        /// <summary>Encode BPTC (196,96) FEC.</summary>
        void encode(const uint8_t* in, uint8_t* out);

    private:
        bool* m_rawData;
        bool* m_deInterData;

        /// <summary></summary>
        void decodeExtractBinary(const uint8_t* in);
        /// <summary></summary>
        void decodeErrorCheck();
        /// <summary></summary>
        void decodeDeInterleave();
        /// <summary></summary>
        void decodeExtractData(uint8_t* data) const;

        /// <summary></summary>
        void encodeExtractData(const uint8_t* in) const;
        /// <summary></summary>
        void encodeInterleave();
        /// <summary></summary>
        void encodeErrorCheck();
        /// <summary></summary>
        void encodeExtractBinary(uint8_t* data);
    };
} // namespace edac

#endif // __BPTC19696_H__
