/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
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
#if !defined(__DMR_LC_CSBK__CSBK_ALOHA_H__)
#define  __DMR_LC_CSBK__CSBK_ALOHA_H__

#include "common/Defines.h"
#include "common/dmr/lc/CSBK.h"

namespace dmr
{
    namespace lc
    {
        namespace csbk
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //      Implements ALOHA - Aloha PDUs for the random access protocol
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_ALOHA : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_ALOHA class.</summary>
                CSBK_ALOHA();

                /// <summary>Decode a control signalling block.</summary>
                bool decode(const uint8_t* data) override;
                /// <summary>Encode a control signalling block.</summary>
                void encode(uint8_t* data) override;

                /// <summary>Returns a string that represents the current CSBK.</summary>
                virtual std::string toString() override;

            public:
                /// <summary>Aloha Site Time Slot Synchronization.</summary>
                __PROPERTY(bool, siteTSSync, SiteTSSync);
                /// <summary>Aloha MS mask.</summary>
                __PROPERTY(uint8_t, alohaMask, AlohaMask);

                /// <summary>Backoff Number.</summary>
                __PROPERTY(uint8_t, backoffNo, BackoffNo);
                /// <summary>Random Access Wait Delay.</summary>
                __PROPERTY(uint8_t, nRandWait, NRandWait);

                __COPY(CSBK_ALOHA);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_ALOHA_H__
