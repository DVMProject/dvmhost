// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025-2026 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup fne_lookups Lookup Tables
 * @brief Implementation for various data lookup tables.
 * @ingroup fne
 * 
 * @file P25AffiliationLookup.h
 * @ingroup fne_lookups
 * @file P25AffiliationLookup.cpp
 * @ingroup fne_lookups
 */
#if !defined(__FNE_AFFILIATION_LOOKUP_H__)
#define __FNE_AFFILIATION_LOOKUP_H__

#include "Defines.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/lookups/ChannelLookup.h"

namespace fne_lookups
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t GRANT_TIMER_TIMEOUT = 15U;

    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements a lookup table class that contains subscriber registration
     *  and group affiliation information.
     * @ingroup fne_lookups
     */
    class HOST_SW_API AffiliationLookup : public ::lookups::AffiliationLookup {
    public:
        /**
         * @brief Initializes a new instance of the AffiliationLookup class.
         * @param name Name of lookup table.
         * @param channelLookup Instance of the channel lookup class.
         * @param verbose Flag indicating whether verbose logging is enabled.
         */
        AffiliationLookup(const std::string name, ::lookups::ChannelLookup* chLookup, bool verbose);
        /**
         * @brief Finalizes a instance of the AffiliationLookup class.
         */
        ~AffiliationLookup() override;

        /** @name Unit Registrations */
        /**
         * @brief Helper to register a source ID.
         * @param srcId Source Radio ID.
         * @param ssrc Originating Peer ID.
         */
        void unitReg(uint32_t srcId, uint32_t ssrc);
        /**
         * @brief Helper to deregister a source ID.
         * @param srcId Source Radio ID.
         * @param automatic Flag indicating the deregistration is a result of an automated timer.
         * @returns bool True, if source ID is deregistered, otherwise false.
         */
        bool unitDereg(uint32_t srcId, bool automatic = false) override;
        /**
         * @brief Helper to get the originating network peer ID by the registered source ID.
         * @param srcId Source Radio ID.
         * @returns uint32_t Originating Peer ID.
         */
        uint32_t getSSRCByUnitReg(uint32_t srcId);
        /**
         * @brief Helper to release unit registrations.
         */
        void clearUnitReg() override;
        /** @} */

        /** @name Channel Grants */
        /**
         * @brief Gets the grant table.
         * @returns std::unordered_map<uint32_t, uint32_t> Channel Grant Table.
         */
        std::unordered_map<uint32_t, uint32_t> grantSrcIdTable() const { return m_grantSrcIdTable.get(); }
        /**
         * @brief Gets the grant SSRC table.
         * @returns std::unordered_map<uint32_t, uint32_t> Source Peer Grant Table.
         */
        std::unordered_map<uint32_t, uint32_t> grantSSRCTable() const { return m_grantSSRCTable.get(); }
        /**
         * @brief Helper to grant a channel.
         * @param dstId Destination Address.
         * @param srcId Source Radio ID.
         * @param ssrc Originating Peer ID.
         * @param grantTimeout Time before the grant times out from inactivity.
         * @param grp Flag indicating the grant is for a talkgroup.
         * @returns bool True, if the destination address has been granted to the source ID, otherwise false.
         */
        bool grantCh(uint32_t dstId, uint32_t srcId, uint32_t ssrc, uint32_t grantTimeout, bool grp);
        /**
         * @brief Helper to get the originating network peer ID by the granted destination ID.
         * @param dstId Destination Address.
         * @returns uint32_t Originating Peer ID.
         */
        uint32_t getSSRCByGrant(uint32_t dstId);
        /**
         * @brief Helper to release the channel grant for the destination ID.
         * @param dstId Destination Address.
         * @param releaseAll Flag indicating all channel grants should be released.
         * @returns bool True, if channel grant was released, otherwise false.
         */
        bool releaseGrant(uint32_t dstId, bool releaseAll = false) override;
        /** @} */

    protected:
        concurrent::unordered_map<uint32_t, uint32_t> m_unitRegPeerTable;
        concurrent::unordered_map<uint32_t, uint32_t> m_grantSSRCTable;
    };
} // namespace fne_lookups

#endif // __FNE_AFFILIATION_LOOKUP_H__
