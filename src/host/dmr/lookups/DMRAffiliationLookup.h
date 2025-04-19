// SPDX-License-Identifier: GPL-2.0-only
/**
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DMRAffiliationLookup.h
 * @ingroup host_lookups
 * @file DMRAffiliationLookup.cpp
 * @ingroup host_lookups
 */
#if !defined(__DMR_AFFILIATION_LOOKUP_H__)
#define __DMR_AFFILIATION_LOOKUP_H__

#include "Defines.h"
#include "common/lookups/AffiliationLookup.h"
#include "common/lookups/ChannelLookup.h"

#include <tuple>

namespace dmr
{
    namespace lookups
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a lookup table class that contains DMR slot grant
         *  information.
         * @ingroup host_lookups
         */
        class HOST_SW_API DMRAffiliationLookup : public ::lookups::AffiliationLookup {
        public:
            /**
             * @brief Initializes a new instance of the DMRAffiliationLookup class.
             * @param channelLookup Instance of the channel lookup class.
             * @param verbose Flag indicating whether verbose logging is enabled.
             */
            DMRAffiliationLookup(::lookups::ChannelLookup* chLookup, bool verbose);
            /**
             * @brief Finalizes a instance of the DMRAffiliationLookup class.
             */
            ~DMRAffiliationLookup() override;

            /** @name Channel Grants */
            /**
             * @brief Helper to grant a channel.
             * @param dstId Destination Address.
             * @param srcId Source Radio ID.
             * @param grantTimeout Time before the grant times out from inactivity.
             * @param grp Flag indicating the grant is for a talkgroup.
             * @param netGranted Flag indicating the grant came from network traffic.
             * @returns bool True, if the destination address has been granted to the source ID, otherwise false.
             */
           bool grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted) override;
            /**
             * @brief Helper to grant a channel and slot.
             * @param dstId Destination Address.
             * @param srcId Source Radio ID.
             * @param slot DMR slot.
             * @param grp Flag indicating the grant is for a talkgroup.
             * @param netGranted Flag indicating the grant came from network traffic.
             * @returns bool True, if the destination address has been granted to the source ID, otherwise false.
             */
            bool grantChSlot(uint32_t dstId, uint32_t srcId, uint8_t slot, uint32_t grantTimeout, bool grp, bool netGranted);
            /**
             * @brief Helper to release the channel grant for the destination ID.
             * @param dstId Destination Address.
             * @param releaseAll Flag indicating all channel grants should be released.
             * @returns bool True, if channel grant was released, otherwise false.
             */
            bool releaseGrant(uint32_t dstId, bool releaseAll) override;
            /**
             * @brief Helper to determine if the channel number is busy.
             * @param chNo Channel Number.
             * @returns bool True, if the specified channel is busy, otherwise false.
             */
            bool isChBusy(uint32_t chNo) const override;
            /**
             * @brief Helper to get the slot granted for the given destination ID.
             * @param dstId Destination Address.
             * @returns uint8_t DMR slot number granted for the destination ID.
             */
            uint8_t getGrantedSlot(uint32_t dstId) const;
            /** @} */

            /**
             * @brief Helper to set a slot for the given channel as being the TSCC.
             * @param chNo Channel Number.
             * @param slot DMR slot.
             */
            void setSlotForChannelTSCC(uint32_t chNo, uint8_t slot);

            /**
             * @brief Helper to determine the an available channel for a slot.
             * @param chNo Channel Number.
             */
            uint32_t getAvailableChannelForSlot(uint8_t slot) const;

            /**
             * @brief Helper to determine the first available slot for given the channel number.
             * @param chNo Channel Number.
             */
            uint8_t getAvailableSlotForChannel(uint32_t chNo) const;

        protected:
            concurrent::unordered_map<uint32_t, std::tuple<uint32_t, uint8_t>> m_grantChSlotTable;

            uint32_t m_tsccChNo;
            uint8_t m_tsccSlot;
        };
    } // namespace lookups
} // namespace dmr

#endif // __DMR_AFFILIATION_LOOKUP_H__
