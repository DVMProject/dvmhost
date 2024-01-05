/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_LC_TSBK__LC_TEL_INT_VCH_USER_H__)
#define  __P25_LC_TSBK__LC_TEL_INT_VCH_USER_H__

#include "common/Defines.h"
#include "common/p25/lc/TDULC.h"

namespace p25
{
    namespace lc
    {
        namespace tdulc
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements TEL INT VCH USER - Telephone Interconnect Voice Channel User
            // ---------------------------------------------------------------------------

            class HOST_SW_API LC_TEL_INT_VCH_USER : public TDULC {
            public:
                /// <summary>Initializes a new instance of the LC_TEL_INT_VCH_USER class.</summary>
                LC_TEL_INT_VCH_USER();

                /// <summary>Decode a terminator data unit w/ link control.</summary>
                bool decode(const uint8_t* data);
                /// <summary>Encode a terminator data unit w/ link control.</summary>
                void encode(uint8_t* data);
            };
        } // namespace tdulc
    } // namespace lc
} // namespace p25

#endif // __P25_LC_TSBK__LC_TEL_INT_VCH_USER_H__
