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
#if !defined(__P25_AFFILIATION_LOOKUP_H__)
#define __P25_AFFILIATION_LOOKUP_H__

#include "Defines.h"
#include "lookups/AffiliationLookup.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Prototypes
    // ---------------------------------------------------------------------------

    class HOST_SW_API Control;

    namespace lookups
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements a lookup table class that contains subscriber registration
        //      and group affiliation information.
        // ---------------------------------------------------------------------------

        class HOST_SW_API P25AffiliationLookup : public ::lookups::AffiliationLookup {
        public:
            /// <summary>Initializes a new instance of the P25AffiliationLookup class.</summary>
            P25AffiliationLookup(Control* p25, bool verbose);
            /// <summary>Finalizes a instance of the P25AffiliationLookup class.</summary>
            virtual ~P25AffiliationLookup();

            /// <summary>Helper to release the channel grant for the destination ID.</summary>
            virtual bool releaseGrant(uint32_t dstId, bool releaseAll);
            /// <summary>Helper to release group affiliations.</summary>
            virtual std::vector<uint32_t> clearGroupAff(uint32_t dstId, bool releaseAll);

        protected:
            Control* m_p25;
        };
    } // namespace lookups
} // namespace p25

#endif // __P25_AFFILIATION_LOOKUP_H__
