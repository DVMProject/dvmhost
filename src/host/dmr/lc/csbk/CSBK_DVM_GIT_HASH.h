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
#if !defined(__DMR_LC_CSBK__CSBK_DVM_GIT_HASH_H__)
#define  __DMR_LC_CSBK__CSBK_DVM_GIT_HASH_H__

#include "Defines.h"
#include "common/dmr/lc/CSBK.h"

namespace dmr
{
    namespace lc
    {
        namespace csbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements DVM GIT Hash Identification
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_DVM_GIT_HASH : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_DVM_GIT_HASH class.</summary>
                CSBK_DVM_GIT_HASH();

                /// <summary>Decode a control signalling block.</summary>
                bool decode(const uint8_t* data);
                /// <summary>Encode a control signalling block.</summary>
                void encode(uint8_t* data);

                /// <summary>Returns a string that represents the current CSBK.</summary>
                virtual std::string toString() override;
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_DVM_GIT_HASH_H__
