// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_AFFILIATION_LOOKUP_H__)
#define __P25_AFFILIATION_LOOKUP_H__

#include "Defines.h"
#include "common/lookups/AffiliationLookup.h"

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
            ~P25AffiliationLookup() override;

            /// <summary>Helper to release the channel grant for the destination ID.</summary>
            bool releaseGrant(uint32_t dstId, bool releaseAll) override;
            /// <summary>Helper to release group affiliations.</summary>
            std::vector<uint32_t> clearGroupAff(uint32_t dstId, bool releaseAll) override;

        protected:
            Control* m_p25;
        };
    } // namespace lookups
} // namespace p25

#endif // __P25_AFFILIATION_LOOKUP_H__
