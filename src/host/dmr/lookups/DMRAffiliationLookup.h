// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__DMR_AFFILIATION_LOOKUP_H__)
#define __DMR_AFFILIATION_LOOKUP_H__

#include "Defines.h"
#include "common/lookups/AffiliationLookup.h"

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
            ~DMRAffiliationLookup() override;

            /// <summary>Helper to grant a channel.</summary>
            bool grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted) override;
            /// <summary>Helper to grant a channel and slot.</summary>
            bool grantChSlot(uint32_t dstId, uint32_t srcId, uint8_t slot, uint32_t grantTimeout, bool grp, bool netGranted);
            /// <summary>Helper to release the channel grant for the destination ID.</summary>
            bool releaseGrant(uint32_t dstId, bool releaseAll) override;
            /// <summary>Helper to determine if the channel number is busy.</summary>
            bool isChBusy(uint32_t chNo) const override;
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
