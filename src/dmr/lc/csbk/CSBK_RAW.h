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
#if !defined(__DMR_LC_CSBK__CSBK_RAW_H__)
#define  __DMR_LC_CSBK__CSBK_RAW_H__

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
            //      Implements a mechanism to generate raw CSBK data from bytes.
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_RAW : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_RAW class.</summary>
                CSBK_RAW();
                /// <summary>Finalizes a instance of the CSBK_RAW class.</summary>
                ~CSBK_RAW();

                /// <summary>Decode a trunking signalling block.</summary>
                virtual bool decode(const uint8_t* data);
                /// <summary>Encode a trunking signalling block.</summary>
                virtual void encode(uint8_t* data);

                /// <summary>Sets the CSBK to encode.</summary>
                void setCSBK(const uint8_t* csbk);

            private:
                uint8_t* m_csbk;
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_RAW_H__
