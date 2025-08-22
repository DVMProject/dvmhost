// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
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

    protected:
        concurrent::unordered_map<uint32_t, uint32_t> m_unitRegPeerTable;
    };
} // namespace fne_lookups

#endif // __FNE_AFFILIATION_LOOKUP_H__
