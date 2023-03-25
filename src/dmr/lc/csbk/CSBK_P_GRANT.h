/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_LC_CSBK__CSBK_P_GRANT_H__)
#define  __DMR_LC_CSBK__CSBK_P_GRANT_H__

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
            //      Implements P_GRANT - Payload Channel Talkgroup Voice Channel Grant
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_P_GRANT : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_P_GRANT class.</summary>
                CSBK_P_GRANT();

                /// <summary>Decode a control signalling block.</summary>
                virtual bool decode(const uint8_t* data);
                /// <summary>Encode a control signalling block.</summary>
                virtual void encode(uint8_t* data);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_P_GRANT_H__
