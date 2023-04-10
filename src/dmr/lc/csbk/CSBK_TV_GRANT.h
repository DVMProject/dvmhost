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
#if !defined(__DMR_LC_CSBK__CSBK_TV_GRANT_H__)
#define  __DMR_LC_CSBK__CSBK_TV_GRANT_H__

#include "Defines.h"
#include "dmr/lc/CSBK.h"

namespace dmr
{
    namespace lc
    {
        namespace csbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements TV_GRANT - Talkgroup Voice Channel Grant
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_TV_GRANT : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_TV_GRANT class.</summary>
                CSBK_TV_GRANT();

                /// <summary>Decode a control signalling block.</summary>
                virtual bool decode(const uint8_t* data);
                /// <summary>Encode a control signalling block.</summary>
                virtual void encode(uint8_t* data);

            public:
                /// <summary>Flag indicating whether the grant is a late entry.</summary>
                __PROPERTY(bool, lateEntry, LateEntry);

                __COPY(CSBK_TV_GRANT);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_TV_GRANT_H__
