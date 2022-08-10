/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__AFFILIATION_LOOKUP_H__)
#define __AFFILIATION_LOOKUP_H__

#include "Defines.h"
#include "Timer.h"

#include <cstdio>
#include <unordered_map>
#include <algorithm>
#include <vector>

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
        AffiliationLookup(const char* name, bool verbose);
        /// <summary>Finalizes a instance of the AffiliationLookup class.</summary>
        virtual ~AffiliationLookup();

        /// <summary>Gets the count of unit registrations.</summary>
        uint8_t unitRegSize() const { return m_unitRegTable.size(); }
        /// <summary>Gets the unit registration table.</summary>
        std::vector<uint32_t> unitRegTable() const { return m_unitRegTable; }
        /// <summary>Helper to register a source ID.</summary>
        virtual void unitReg(uint32_t srcId);
        /// <summary>Helper to deregister a source ID.</summary>
        virtual bool unitDereg(uint32_t srcId);
        /// <summary>Helper to determine if the source ID has unit registered.</summary>
        virtual bool isUnitReg(uint32_t srcId) const;

        /// <summary>Gets the count of affiliations.</summary>
        uint8_t grpAffSize() const { return m_grpAffTable.size(); }
        /// <summary>Gets the group affiliation table.</summary>
        std::unordered_map<uint32_t, uint32_t> grpAffTable() const { return m_grpAffTable; }
        /// <summary>Helper to group affiliate a source ID.</summary>
        virtual void groupAff(uint32_t srcId, uint32_t dstId);
        /// <summary>Helper to group unaffiliate a source ID.</summary>
        virtual bool groupUnaff(uint32_t srcId);
        /// <summary>Helper to determine if the source ID has affiliated to the group destination ID.</summary>
        virtual bool isGroupAff(uint32_t srcId, uint32_t dstId) const;
        /// <summary>Helper to release group affiliations.</summary>
        virtual std::vector<uint32_t> clearGroupAff(uint32_t dstId, bool releaseAll);

        /// <summary>Gets the count of grants.</summary>
        uint8_t grantSize() const { return m_grantChTable.size(); }
        /// <summary>Gets the grant table.</summary>
        std::unordered_map<uint32_t, uint32_t> grantTable() const { return m_grantChTable; }
        /// <summary>Helper to grant a channel.</summary>
        virtual bool grantCh(uint32_t dstId, uint32_t grantTimeout);
        /// <summary>Helper to start the destination ID grant timer.</summary>
        virtual void touchGrant(uint32_t dstId);
        /// <summary>Helper to release the channel grant for the destination ID.</summary>
        virtual bool releaseGrant(uint32_t dstId, bool releaseAll);
        /// <summary>Helper to determine if the channel number is busy.</summary>
        virtual bool isChBusy(uint32_t chNo) const;
        /// <summary>Helper to determine if the destination ID is already granted.</summary>
        virtual bool isGranted(uint32_t dstId) const;
        /// <summary>Helper to get the channel granted for the given destination ID.</summary>
        virtual uint32_t getGrantedCh(uint32_t dstId);

        /// <summary>Helper to add a RF channel.</summary>
        void addRFCh(uint32_t chNo) { m_rfChTable.push_back(chNo); }
        /// <summary>Helper to remove a RF channel.</summary>
        void removeRFCh(uint32_t chNo) { m_rfChTable.push_back(chNo); }
        /// <summary>Gets the count of RF channels.</summary>
        uint8_t getRFChCnt() const { return m_rfChTable.size(); }
        /// <summary>Helper to determine if there are any RF channels available..</summary>
        bool isRFChAvailable() const { return !m_rfChTable.empty(); }
        /// <summary>Gets the count of granted RF channels.</summary>
        uint8_t getGrantedRFChCnt() const { return m_rfGrantChCnt; }

        /// <summary>Updates the processor by the passed number of milliseconds.</summary>
        void clock(uint32_t ms);

    protected:
        std::vector<uint32_t> m_rfChTable;
        uint8_t m_rfGrantChCnt;

        std::vector<uint32_t> m_unitRegTable;
        std::unordered_map<uint32_t, uint32_t> m_grpAffTable;

        std::unordered_map<uint32_t, uint32_t> m_grantChTable;
        std::unordered_map<uint32_t, Timer> m_grantTimers;

        const char *m_name;

        bool m_verbose;
    };
} // namespace lookups

#endif // __AFFILIATION_LOOKUP_H__
