// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024,2026 Bryan Biedenkapp, N2PLL
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

#include <unordered_map>

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
             * @brief Grants a channel and records private-call participant identity.
             * @param dstId Destination address.
             * @param srcId Source radio ID.
             * @param grantTimeout Time before the grant expires from inactivity.
             * @param grp Flag indicating the grant is for a talkgroup.
             * @param netGranted Flag indicating the grant came from network traffic.
             * @returns bool True if the grant was created; otherwise, false.
             */
            bool grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted) override;
            /**
             * @brief Helper to release the channel grant for the destination ID.
             * @param dstId Destination Address.
             * @param releaseAll Flag indicating all channel grants should be released.
             * @returns bool True, if channel grant was released, otherwise false.
             */
            bool releaseGrant(uint32_t dstId, bool releaseAll) override;

            /**
             * @brief Resolves a unit-to-unit participant pair to its single grant owner.
             * @param srcId One participant RID.
             * @param dstId The other participant RID.
             * @returns uint32_t Destination-key owner of the grant, or zero when absent.
             */
            uint32_t getU2UGrantOwner(uint32_t srcId, uint32_t dstId) const;
            /**
             * @brief Resolves one private-call participant to its grant owner.
             * @param unitId RID of either private-call participant.
             * @returns uint32_t Destination-key owner of the grant, or zero when absent.
             */
            uint32_t getU2UGrantOwner(uint32_t unitId) const;
            /**
             * @brief Releases the active unit-to-unit grant involving a participant.
             * @param unitId RID of either private-call participant.
             * @returns bool True when an active private grant was released.
             */
            bool releaseU2UGrant(uint32_t unitId);
            /**
             * @brief Updates the directional signaling identities for an active private grant.
             * @param srcId Current first-to-talk/source RID.
             * @param dstId Current target RID.
             * @returns bool True when the active pair was updated.
             */
            bool setU2UGrantDirection(uint32_t srcId, uint32_t dstId);
            /**
             * @brief Gets the current directional source RID for a private grant owner.
             * @param grantOwner Destination-key owner of the underlying grant.
             * @returns uint32_t Current source RID, or zero when unavailable.
             */
            uint32_t getU2UGrantSource(uint32_t grantOwner) const;
            /**
             * @brief Gets the current directional target RID for a private grant owner.
             * @param grantOwner Destination-key owner of the underlying grant.
             * @returns uint32_t Current target RID, or zero when unavailable.
             */
            uint32_t getU2UGrantTarget(uint32_t grantOwner) const;
            /**
             * @brief Tests whether a unit-to-unit participant pair has an active grant.
             * @param srcId One participant RID.
             * @param dstId The other participant RID.
             * @returns bool True if the pair has one active private grant.
             */
            bool isU2UGranted(uint32_t srcId, uint32_t dstId) const;
            /** @} */

        protected:
            Control* m_p25;

            std::unordered_map<uint64_t, uint32_t> m_u2uGrantOwners;  //!< Maps an unordered private-call RID pair to one destination-key grant owner.
            std::unordered_map<uint32_t, uint32_t> m_u2uGrantSources; //!< Maps a private grant owner to the current first-to-talk/source RID.
            std::unordered_map<uint32_t, uint32_t> m_u2uGrantTargets; //!< Maps a private grant owner to the current target RID.

            /**
             * @brief Produces the stable unordered key for a private-call RID pair.
             * @param srcId One participant RID.
             * @param dstId The other participant RID.
             * @returns uint64_t Stable 48-bit pair key, or zero for an invalid pair.
             */
            static uint64_t u2uPairKey(uint32_t srcId, uint32_t dstId);
        };
    } // namespace lookups
} // namespace p25

#endif // __P25_AFFILIATION_LOOKUP_H__
