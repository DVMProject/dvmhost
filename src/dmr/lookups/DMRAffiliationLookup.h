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
#if !defined(__DMR_AFFILIATION_LOOKUP_H__)
#define __DMR_AFFILIATION_LOOKUP_H__

#include "Defines.h"
#include "lookups/AffiliationLookup.h"

#include <tuple>

namespace dmr
{
    namespace lookups
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements a lookup table class that contains DMR slot grant
        //      information.
        // ---------------------------------------------------------------------------

        class HOST_SW_API DMRAffiliationLookup : public ::lookups::AffiliationLookup {
        public:
            /// <summary>Initializes a new instance of the DMRAffiliationLookup class.</summary>
            DMRAffiliationLookup(bool verbose);
            /// <summary>Finalizes a instance of the DMRAffiliationLookup class.</summary>
            virtual ~DMRAffiliationLookup();

            /// <summary>Helper to grant a channel.</summary>
            virtual bool grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout);
            /// <summary>Helper to grant a channel and slot.</summary>
            bool grantChSlot(uint32_t dstId, uint32_t srcId, uint8_t slot, uint32_t grantTimeout);
            /// <summary>Helper to release the channel grant for the destination ID.</summary>
            virtual bool releaseGrant(uint32_t dstId, bool releaseAll);
            /// <summary>Helper to determine if the channel number is busy.</summary>
            virtual bool isChBusy(uint32_t chNo) const;
            /// <summary>Helper to get the slot granted for the given destination ID.</summary>
            uint8_t getGrantedSlot(uint32_t dstId) const;

            /// <summary>Helper to set a slot for the given channel as being the TSCC.</summary>
            void setSlotForChannelTSCC(uint32_t chNo, uint8_t slot);

            /// <summary>Helper to determine the first available slot for given the channel number.</summary>
            uint8_t getAvailableSlotForChannel(uint32_t chNo) const;

        protected:
            std::unordered_map<uint32_t, std::tuple<uint32_t, uint8_t>> m_grantChSlotTable;

            uint32_t m_tsccChNo;
            uint8_t m_tsccSlot;
        };
    } // namespace lookups
} // namespace dmr

#endif // __DMR_AFFILIATION_LOOKUP_H__
