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
#if !defined(__DMR_LC_CSBK__CSBK_BROADCAST_H__)
#define  __DMR_LC_CSBK__CSBK_BROADCAST_H__

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
            //      Implements BCAST - Announcement PDUs
            // ---------------------------------------------------------------------------

            class HOST_SW_API CSBK_BROADCAST : public CSBK {
            public:
                /// <summary>Initializes a new instance of the CSBK_BROADCAST class.</summary>
                CSBK_BROADCAST();

                /// <summary>Decode a control signalling block.</summary>
                bool decode(const uint8_t* data);
                /// <summary>Encode a control signalling block.</summary>
                void encode(uint8_t* data);

                /// <summary>Returns a string that represents the current CSBK.</summary>
                virtual std::string toString() override;

            public:
                /// <summary>Broadcast Announcment Type.</summary>
                __PROPERTY(uint8_t, anncType, AnncType);
                /// <summary>Broadcast Hibernation Flag.</summary>
                __PROPERTY(bool, hibernating, Hibernating);

                /// <summary>Broadcast Announce/Withdraw Channel 1 Flag.</summary>
                __PROPERTY(bool, annWdCh1, AnnWdCh1);
                /// <summary>Broadcast Announce/Withdraw Channel 2 Flag.</summary>
                __PROPERTY(bool, annWdCh2, AnnWdCh2);

                /// <summary>Backoff Number.</summary>
                __PROPERTY(uint8_t, backoffNo, BackoffNo);

                __COPY(CSBK_BROADCAST);
            };
        } // namespace csbk
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC_CSBK__CSBK_BROADCAST_H__
