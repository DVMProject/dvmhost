// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup host_lookups Lookup Tables
 * @brief Implementation for various data lookup tables.
 * @ingroup host
 * 
 * @file P25AffiliationLookup.h
 * @ingroup host_lookups
 * @file P25AffiliationLookup.cpp
 * @ingroup host_lookups
 */
#if !defined(__P25_AFFILIATION_LOOKUP_H__)
#define __P25_AFFILIATION_LOOKUP_H__

#include "Defines.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/lookups/ChannelLookup.h"

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
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a lookup table class that contains subscriber registration
         *  and group affiliation information.
         * @ingroup host_lookups
         */
        class HOST_SW_API P25AffiliationLookup : public ::lookups::AffiliationLookup {
        public:
            /**
             * @brief Initializes a new instance of the P25AffiliationLookup class.
             * @param p25 Instance of the p25::Control class.
             * @param channelLookup Instance of the channel lookup class.
             * @param verbose Flag indicating whether verbose logging is enabled.
             */
            P25AffiliationLookup(Control* p25, ::lookups::ChannelLookup* chLookup, bool verbose);
            /**
             * @brief Finalizes a instance of the P25AffiliationLookup class.
             */
            ~P25AffiliationLookup() override;
    
            /** @name Group Affiliations */
            /**
             * @brief Helper to release group affiliations.
             * @param dstId Talkgroup ID.
             * @param releaseAll Flag indicating all affiliations should be released.
             * @returns std::vector<uint32_t> List of source IDs that have been deaffiliated.
             */
            std::vector<uint32_t> clearGroupAff(uint32_t dstId, bool releaseAll) override;
            /** @} */

            /** @name Channel Grants */
            /**
             * @brief Helper to release the channel grant for the destination ID.
             * @param dstId Destination Address.
             * @param releaseAll Flag indicating all channel grants should be released.
             * @param noLock Flag indicating no mutex lock operation should be performed while releasing.
             * @returns bool True, if channel grant was released, otherwise false.
             */
            bool releaseGrant(uint32_t dstId, bool releaseAll, bool noLock = false) override;
            /** @} */

        protected:
            Control* m_p25;
        };
    } // namespace lookups
} // namespace p25

#endif // __P25_AFFILIATION_LOOKUP_H__
