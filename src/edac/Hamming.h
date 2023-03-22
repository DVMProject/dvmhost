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
#if !defined(__HAMMING_H__)
#define __HAMMING_H__

#include "Defines.h"

namespace edac
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    //      Implements Hamming (15,11,3), (13,9,3), (10,6,3), (16,11,4) and
    //      (17, 12, 3) forward error correction.
    // ---------------------------------------------------------------------------

    class HOST_SW_API Hamming {
    public:
        /// <summary>Decode Hamming (15,11,3).</summary>
        static bool decode15113_1(bool* d);
        /// <summary>Encode Hamming (15,11,3).</summary>
        static void encode15113_1(bool* d);

        /// <summary>Decode Hamming (15,11,3).</summary>
        static bool decode15113_2(bool* d);
        /// <summary>Encode Hamming (15,11,3).</summary>
        static void encode15113_2(bool* d);

        /// <summary>Decode Hamming (13,9,3).</summary>
        static bool decode1393(bool* d);
        /// <summary>Encode Hamming (13,9,3).</summary>
        static void encode1393(bool* d);

        /// <summary>Decode Hamming (10,6,3).</summary>
        static bool decode1063(bool* d);
        /// <summary>Encode Hamming (10,6,3).</summary>
        static void encode1063(bool* d);

        /// <summary>Decode Hamming (16,11,4).</summary>
        static bool decode16114(bool* d);
        /// <summary>Encode Hamming (16,11,4).</summary>
        static void encode16114(bool* d);

        /// <summary>Decode Hamming (17,12,3).</summary>
        static bool decode17123(bool* d);
        /// <summary>Encode Hamming (17,12,3).</summary>
        static void encode17123(bool* d);
    };
} // namespace edac

#endif // __HAMMING_H__
