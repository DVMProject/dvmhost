// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
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
    //      Implements a lookup table class that contains subscriber registration
    //      and group affiliation information.
    // ---------------------------------------------------------------------------

    class HOST_SW_API AffiliationLookup {
    public:
        /// <summary>Initializes a new instance of the AffiliationLookup class.</summary>
        AffiliationLookup(const std::string name, ChannelLookup* chLookup, bool verbose);
        /// <summary>Finalizes a instance of the AffiliationLookup class.</summary>
        virtual ~AffiliationLookup();

        /** Unit Registrations */
        /// <summary>Gets the count of unit registrations.</summary>
        uint8_t unitRegSize() const { return m_unitRegTable.size(); }
        /// <summary>Gets the unit registration table.</summary>
        std::vector<uint32_t> unitRegTable() const { return m_unitRegTable; }
        /// <summary>Helper to register a source ID.</summary>
        virtual void unitReg(uint32_t srcId);
        /// <summary>Helper to deregister a source ID.</summary>
        virtual bool unitDereg(uint32_t srcId);
        /// <summary>Helper to start the source ID registration timer.</summary>
        virtual void touchUnitReg(uint32_t srcId);
        /// <summary>Gets the current timer timeout for this unit registration.</summary>
        virtual uint32_t unitRegTimeout(uint32_t srcId);
        /// <summary>Gets the current timer value for this unit registration.</summary>
        virtual uint32_t unitRegTimer(uint32_t srcId);
        /// <summary>Helper to determine if the source ID has unit registered.</summary>
        virtual bool isUnitReg(uint32_t srcId) const;
        /// <summary>Helper to release unit registrations.</summary>
        virtual void clearUnitReg();

        /** Group Affiliations */
        /// <summary>Gets the count of affiliations.</summary>
        uint8_t grpAffSize() const { return m_grpAffTable.size(); }
        /// <summary>Gets the group affiliation table.</summary>
        std::unordered_map<uint32_t, uint32_t> grpAffTable() const { return m_grpAffTable; }
        /// <summary>Helper to group affiliate a source ID.</summary>
        virtual void groupAff(uint32_t srcId, uint32_t dstId);
        /// <summary>Helper to group unaffiliate a source ID.</summary>
        virtual bool groupUnaff(uint32_t srcId);
        /// <summary>Helper to determine if the group destination ID has any affiations.</summary>
        virtual bool hasGroupAff(uint32_t dstId) const;
        /// <summary>Helper to determine if the source ID has affiliated to the group destination ID.</summary>
        virtual bool isGroupAff(uint32_t srcId, uint32_t dstId) const;
        /// <summary>Helper to release group affiliations.</summary>
        virtual std::vector<uint32_t> clearGroupAff(uint32_t dstId, bool releaseAll);

        /** Channel Grants */
        /// <summary>Gets the count of grants.</summary>
        uint8_t grantSize() const { return m_grantChTable.size(); }
        /// <summary>Gets the grant table.</summary>
        std::unordered_map<uint32_t, uint32_t> grantTable() const { return m_grantChTable; }
        /// <summary>Helper to grant a channel.</summary>
        virtual bool grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted);
        /// <summary>Helper to start the destination ID grant timer.</summary>
        virtual void touchGrant(uint32_t dstId);
        /// <summary>Helper to release the channel grant for the destination ID.</summary>
        virtual bool releaseGrant(uint32_t dstId, bool releaseAll);
        /// <summary>Helper to determine if the channel number is busy.</summary>
        virtual bool isChBusy(uint32_t chNo) const;
        /// <summary>Helper to determine if the destination ID is already granted.</summary>
        virtual bool isGranted(uint32_t dstId) const;
        /// <summary>Helper to determine if the destination ID granted is a group or not.</summary>
        virtual bool isGroup(uint32_t dstId) const;
        /// <summary>Helper to determine if the destination ID is granted by network traffic.</summary>
        virtual bool isNetGranted(uint32_t dstId) const;
        /// <summary>Helper to get the channel granted for the given destination ID.</summary>
        virtual uint32_t getGrantedCh(uint32_t dstId);
        /// <summary>Helper to get the destination ID granted to the given source ID.</summary>
        virtual uint32_t getGrantedBySrcId(uint32_t srcId);
        /// <summary>Helper to get the source ID granted for the given destination ID.</summary>
        virtual uint32_t getGrantedSrcId(uint32_t dstId);
        /// <summary>Gets the count of granted RF channels.</summary>
        uint8_t getGrantedRFChCnt() const { return m_rfGrantChCnt; }

        /// <summary>Gets the RF channel lookup class.</summary>
        ChannelLookup* rfCh() const { return m_chLookup; }

        /// <summary>Updates the processor by the passed number of milliseconds.</summary>
        void clock(uint32_t ms);

        /// <summary>Helper to determine if the unit registration timeout is enabled or not.</summary>
        virtual bool isDisableUnitRegTimeout() const { return m_disableUnitRegTimeout; }
        /// <summary>Disables the unit registration timeout.</summary>
        void setDisableUnitRegTimeout(bool disabled) { m_disableUnitRegTimeout = disabled; }

        /// <summary>Helper to set the release grant callback.</summary>
        void setReleaseGrantCallback(std::function<void(uint32_t, uint32_t, uint8_t)>&& callback) { m_releaseGrant = callback; }
        /// <summary>Helper to set the unit deregistration callback.</summary>
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
