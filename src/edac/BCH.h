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
#if !defined(__BCH_H__)
#define __BCH_H__

#include "Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Bose�Chaudhuri�Hocquenghem codes for protecting P25 NID
    //      data.
    // ---------------------------------------------------------------------------

    class HOST_SW_API BCH {
    public:
        /// <summary>Initializes a new instance of the BCH class.</summary>
        BCH();
        /// <summary>Finalizes a instance of the BCH class.</summary>
        ~BCH();

        /// <summary>Encodes input data with BCH.</summary>
        void encode(uint8_t* data);

    private:
        /// <summary></summary>
        void encode(const int* data, int* bb);
    };
} // namespace edac

#endif // __BCH_H__
