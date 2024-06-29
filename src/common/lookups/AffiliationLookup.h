// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup lookups_aff Unit Registration, Affiliation and Grant Lookups
 * @brief Implementation for unit registration, affiliation and channel grant lookup tables.
 * @ingroup lookups
 * 
 * @file AffiliationLookup.h
 * @ingroup lookups_aff
 * @file AffiliationLookup.cpp
 * @ingroup lookups_aff
 */
#if !defined(__AFFILIATION_LOOKUP_H__)
#define __AFFILIATION_LOOKUP_H__

#include "common/Defines.h"
#include "common/lookups/ChannelLookup.h"
#include "common/Timer.h"

#include <cstdio>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <functional>

namespace lookups
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements a lookup table class that contains subscriber registration
     *  and group affiliation information.
     * @ingroup lookups_aff
     */
    class HOST_SW_API AffiliationLookup {
    public:
        /**
         * @brief Initializes a new instance of the AffiliationLookup class.
         * @param name Name of lookup table.
         * @param channelLookup Instance of the channel lookup class.
         * @param verbose Flag indicating whether verbose logging is enabled.
         */
        AffiliationLookup(const std::string name, ChannelLookup* chLookup, bool verbose);
        /**
         * @brief Finalizes a instance of the AffiliationLookup class.
         */
        virtual ~AffiliationLookup();

        /** @name Unit Registrations */
        /**
         * @brief Gets the count of unit registrations.
         * @returns uint32_t Total count of unit registrations.
         */
        uint32_t unitRegSize() const { return m_unitRegTable.size(); }
        /**
         * @brief Gets the unit registration table.
         * @returns std::vector<uint32> Unit Registration Table.
         */
        std::vector<uint32_t> unitRegTable() const { return m_unitRegTable; }
        /**
         * @brief Helper to register a source ID.
         * @param srcId Source Radio ID.
         */
        virtual void unitReg(uint32_t srcId);
        /**
         * @brief Helper to deregister a source ID.
         * @param srcId Source Radio ID.
         * @returns bool True, if source ID is deregistered, otherwise false.
         */
        virtual bool unitDereg(uint32_t srcId);
        /**
         * @brief Helper to start the source ID registration timer.
         * @param srcId Source Radio ID.
         */
        virtual void touchUnitReg(uint32_t srcId);
        /**
         * @brief Gets the current timer timeout for this unit registration.
         * @param srcId Source Radio ID.
         * @returns uint32_t Current registration timer timeout for the unit registration.
         */
        virtual uint32_t unitRegTimeout(uint32_t srcId);
        /**
         * @brief Gets the current timer value for this unit registration.
         * @param srcId Source Radio ID.
         * @returns uint32_t Current timer value for the unit registration.
         */
        virtual uint32_t unitRegTimer(uint32_t srcId);
        /**
         * @brief Helper to determine if the source ID has unit registered.
         * @param srcId Source Radio ID.
         * @returns bool True, if source ID has registered, otherwise false.
         */
        virtual bool isUnitReg(uint32_t srcId) const;
        /**
         * @brief Helper to release unit registrations.
         */
        virtual void clearUnitReg();
        /** @} */

        /** @name Group Affiliations */
        /**
         * @brief Gets the count of affiliations.
         * @returns uint32_t Total count of group affiliations.
         */
        uint32_t grpAffSize() const { return m_grpAffTable.size(); }
        /**
         * @brief Gets the group affiliation table.
         * @returns std::unordered_map<uint32_t, uint32_t> Group Affiliation Table.
         */
        std::unordered_map<uint32_t, uint32_t> grpAffTable() const { return m_grpAffTable; }
        /**
         * @brief Helper to group affiliate a source ID.
         * @param srcId Source Radio ID.
         * @param dstId Talkgroup ID.
         */
        virtual void groupAff(uint32_t srcId, uint32_t dstId);
        /**
         * @brief Helper to group unaffiliate a source ID.
         * @param srcId Source Radio ID.
         * @returns bool True, if source ID has deaffiliated, otherwise false.
         */
        virtual bool groupUnaff(uint32_t srcId);
        /**
         * @brief Helper to determine if the group destination ID has any affiations.
         * @param dstId Talkgroup ID.
         * @returns bool True, if talkgroup ID has affiliations, otherwise false.
         */
        virtual bool hasGroupAff(uint32_t dstId) const;
        /**
         * @brief Helper to determine if the source ID has affiliated to the group destination ID.
         * @param srcId Source Radio ID.
         * @param dstId Talkgroup ID.
         * @returns bool True, if the source ID is affiliated to the talkgroup ID, otherwise false.
         */
        virtual bool isGroupAff(uint32_t srcId, uint32_t dstId) const;
        /**
         * @brief Helper to release group affiliations.
         * @param dstId Talkgroup ID.
         * @param releaseAll Flag indicating all affiliations should be released.
         * @returns std::vector<uint32_t> List of source IDs that have been deaffiliated.
         */
        virtual std::vector<uint32_t> clearGroupAff(uint32_t dstId, bool releaseAll);
        /** @} */

        /** @name Channel Grants */
        /**
         * @brief Gets the count of grants.
         * @returns uint32_t Total count of grants.
         */
        uint8_t grantSize() const { return m_grantChTable.size(); }
        /**
         * @brief Gets the grant table.
         * @returns std::unordered_map<uint32_t, uint32_t> Channel Grant Table.
         */
        std::unordered_map<uint32_t, uint32_t> grantTable() const { return m_grantChTable; }
        /**
         * @brief Helper to grant a channel.
         * @param dstId Destination Address.
         * @param srcId Source Radio ID.
         * @param grantTimeout Time before the grant times out from inactivity.
         * @param grp Flag indicating the grant is for a talkgroup.
         * @param netGranted Flag indicating the grant came from network traffic.
         * @returns bool True, if the destination address has been granted to the source ID, otherwise false.
         */
        virtual bool grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted);
        /**
         * @brief Helper to start/reset the destination ID grant timer.
         * @param dstId Destination Address.
         */
        virtual void touchGrant(uint32_t dstId);
        /**
         * @brief Helper to release the channel grant for the destination ID.
         * @param dstId Destination Address.
         * @param releaseAll Flag indicating all channel grants should be released.
         * @returns bool True, if channel grant was released, otherwise false.
         */
        virtual bool releaseGrant(uint32_t dstId, bool releaseAll);
        /**
         * @brief Helper to determine if the channel number is busy.
         * @param chNo Channel Number.
         * @returns bool True, if the specified channel is busy, otherwise false.
         */
        virtual bool isChBusy(uint32_t chNo) const;
        /**
         * @brief Helper to determine if the destination ID is already granted.
         * @param dstId Destination Address.
         * @returns bool True, if the destination ID is granted, otherwise false.
         */
        virtual bool isGranted(uint32_t dstId) const;
        /**
         * @brief Helper to determine if the destination ID granted is a group or not.
         * @param dstId Destination Address.
         * @returns bool True, if the destination ID is granted as a group, otherwise false.
         */
        virtual bool isGroup(uint32_t dstId) const;
        /**
         * @brief Helper to determine if the destination ID is granted by network traffic.
         * @param dstId Destination Address.
         * @returns bool True, if the destination ID is granted by network traffic, otherwise false.
         */
        virtual bool isNetGranted(uint32_t dstId) const;
        /**
         * @brief Helper to get the channel granted for the given destination ID.
         * @param dstId Destination Address.
         * @returns uint32_t Channel Number
         */
        virtual uint32_t getGrantedCh(uint32_t dstId);
        /**
         * @brief Helper to get the destination ID granted to the given source ID.
         * @param srcId Source Radio ID.
         * @returns uint32_t Destination Address.
         */
        virtual uint32_t getGrantedBySrcId(uint32_t srcId);
        /**
         * @brief Helper to get the source ID granted for the given destination ID.
         * @param dstId Destination Address.
         * @returns uint32_t Source Radio ID.
         */
        virtual uint32_t getGrantedSrcId(uint32_t dstId);
        /**
         * @brief Gets the count of granted RF channels.
         * @returns uint8_t Total number of granted RF channels.
         */
        uint8_t getGrantedRFChCnt() const { return m_rfGrantChCnt; }
        /** @} */

        /**
         * @brief Gets the RF channel lookup class.
         * @returns ChannelLookup* Instance of the ChannelLookup class.
         */
        ChannelLookup* rfCh() const { return m_chLookup; }

        /**
         * @brief Updates the processor by the passed number of milliseconds.
         * @param ms Number of milliseconds.
         */
        void clock(uint32_t ms);

        /**
         * @brief Helper to determine if the unit registration timeout is enabled or not.
         * @returns bool True, if idle unit registration timeouts are disabled, otherwise false.
         */
        virtual bool isDisableUnitRegTimeout() const { return m_disableUnitRegTimeout; }
        /**
         * @brief Disables the unit registration timeout.
         * @param disable Flag indicating idle unit registration timeout should be disabled.
         */
        void setDisableUnitRegTimeout(bool disabled) { m_disableUnitRegTimeout = disabled; }

        /**
         * @brief Helper to set the release grant callback.
         * @param callback Relase grant function callback.
         */
        void setReleaseGrantCallback(std::function<void(uint32_t, uint32_t, uint8_t)>&& callback) { m_releaseGrant = callback; }
        /**
         * @brief Helper to set the unit deregistration callback.
         * @param callback Unit deregistration function callback.
         */
        void setUnitDeregCallback(std::function<void(uint32_t)>&& callback) { m_unitDereg = callback; }

    protected:
        uint8_t m_rfGrantChCnt;

        std::vector<uint32_t> m_unitRegTable;
        std::unordered_map<uint32_t, Timer> m_unitRegTimers;
        std::unordered_map<uint32_t, uint32_t> m_grpAffTable;

        std::unordered_map<uint32_t, uint32_t> m_grantChTable;
        std::unordered_map<uint32_t, uint32_t> m_grantSrcIdTable;
        std::unordered_map<uint32_t, bool> m_uuGrantedTable;
        std::unordered_map<uint32_t, bool> m_netGrantedTable;
        std::unordered_map<uint32_t, Timer> m_grantTimers;

        //                 chNo      dstId     slot
        std::function<void(uint32_t, uint32_t, uint8_t)> m_releaseGrant;
        //                 srcId
        std::function<void(uint32_t)> m_unitDereg;

        std::string m_name;
        ChannelLookup* m_chLookup;

        bool m_disableUnitRegTimeout;

        bool m_verbose;
    };
} // namespace lookups

#endif // __AFFILIATION_LOOKUP_H__
