// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025-2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/Log.h"
#include "lookups/AffiliationLookup.h"

using namespace fne_lookups;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the AffiliationLookup class. */

AffiliationLookup::AffiliationLookup(const std::string name, ::lookups::ChannelLookup* chLookup, bool verbose) : 
    ::lookups::AffiliationLookup(name, chLookup, verbose),
    m_unitRegPeerTable()
{
    m_unitRegPeerTable.clear();
}

/* Finalizes a instance of the AffiliationLookup class. */

AffiliationLookup::~AffiliationLookup() = default;

/* Helper to group affiliate a source ID. */

void AffiliationLookup::unitReg(uint32_t srcId, uint32_t ssrc)
{
    if (isUnitReg(srcId)) {
        return;
    }

    ::lookups::AffiliationLookup::unitReg(srcId);

    m_unitRegPeerTable[srcId] = ssrc;
}

/* Helper to group unaffiliate a source ID. */

bool AffiliationLookup::unitDereg(uint32_t srcId, bool automatic)
{
    bool ret = false;

    if (!isUnitReg(srcId)) {
        return false;
    }

    ret = ::lookups::AffiliationLookup::unitDereg(srcId, automatic);

    if (ret) {
        // lookup dynamic table entry
        if (m_unitRegPeerTable.find(srcId) == m_unitRegPeerTable.end()) {
            return false;
        }

        // remove dynamic table entry
        try {
            uint32_t entry = m_unitRegPeerTable.at(srcId); // this value will get discarded
            (void)entry;                                   // but some variants of C++ mark the unordered_map<>::at as nodiscard
            m_unitRegPeerTable.erase(srcId);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    return false;
}

/* Helper to get the originating network peer ID by the registered source ID. */

uint32_t AffiliationLookup::getSSRCByUnitReg(uint32_t srcId)
{
    // lookup dynamic unit registration table entry
    m_unitRegPeerTable.lock(false);
    for (auto entry : m_unitRegPeerTable) {
        if (entry.first == srcId) {
            m_unitRegPeerTable.unlock();
            return entry.second;
        }
    }
    m_unitRegPeerTable.unlock();

    return 0U;
}

/* Helper to release unit registrations. */

void AffiliationLookup::clearUnitReg()
{
    ::lookups::AffiliationLookup::clearUnitReg();
    m_unitRegPeerTable.clear();
}

/* Helper to grant a channel. */

bool AffiliationLookup::grantCh(uint32_t dstId, uint32_t srcId, uint32_t ssrc, uint32_t grantTimeout, bool grp)
{
    if (dstId == 0U) {
        return false;
    }

    __lock();

    m_grantChTable[dstId] = 1U; // the FNE doesn't contain channels so internally all grants will be tracked with a 
                                // channel number of 1, but this allows us to track the grant by the destination ID
    m_grantSrcIdTable[dstId] = srcId;
    m_grantSSRCTable[dstId] = ssrc;
    m_rfGrantChCnt++;

    m_uuGrantedTable[dstId] = !grp;

    m_grantTimers[dstId] = Timer(1000U, grantTimeout);
    m_grantTimers[dstId].start();

    if (m_verbose) {
        LogInfoEx(LOG_MASTER, "%s, granting talkgroup call, dstId = %u, srcId = %u, ssrc = %u, group = %u",
            m_name.c_str(), dstId, srcId, ssrc, grp);
    }

    __unlock();

    return true;
}

/* Helper to get the originating network peer ID by the registered source ID. */

uint32_t AffiliationLookup::getSSRCByGrant(uint32_t srcId)
{
    // lookup dynamic channel grant table entry
    m_grantSSRCTable.lock(false);
    for (auto entry : m_grantSSRCTable) {
        if (entry.first == srcId) {
            m_grantSSRCTable.unlock();
            return entry.second;
        }
    }
    m_grantSSRCTable.unlock();

    return 0U;
}

/* Helper to release the channel grant for the destination ID. */

bool AffiliationLookup::releaseGrant(uint32_t dstId, bool releaseAll)
{
    if (dstId == 0U && !releaseAll) {
        return false;
    }

    // are we trying to release all grants?
    if (dstId == 0U && releaseAll) {
        LogWarning(LOG_MASTER, "%s, force releasing all talkgroup call grants", m_name.c_str());

        m_grantSSRCTable.lock(false);
        std::vector<uint32_t> gntsToRel = std::vector<uint32_t>();
        for (auto entry : m_grantSSRCTable) {
            uint32_t dstId = entry.first;
            gntsToRel.push_back(dstId);
        }
        m_grantSSRCTable.unlock();

        // release grants
        for (uint32_t dstId : gntsToRel) {
            releaseGrant(dstId, false);
        }

        return true;
    }

    if (isGranted(dstId)) {
        uint32_t ssrc = 0U;
        m_grantSSRCTable.lock(false);
        for (auto entry : m_grantSSRCTable) {
            if (entry.first == dstId) {
                ssrc = entry.second;
                break;
            }
        }
        m_grantSSRCTable.unlock();

        uint32_t srcId = getGrantedSrcId(dstId);

        if (m_verbose) {
            LogInfoEx(LOG_MASTER, "%s, releasing talkgroup call grant, ssrc = %u, dstId = %u",
                m_name.c_str(), ssrc, dstId);
        }

        if (m_releaseGrant != nullptr) {
            m_releaseGrant(0U, srcId, dstId, 0U);
        }

        __lock();

        m_grantChTable.erase(dstId);
        m_grantSSRCTable.erase(dstId);
        m_grantSrcIdTable.erase(dstId);
        m_uuGrantedTable.erase(dstId);
        m_netGrantedTable.erase(dstId);

        if (m_rfGrantChCnt > 0U) {
            m_rfGrantChCnt--;
        }
        else {
            m_rfGrantChCnt = 0U;
        }

        m_grantTimers[dstId].stop();

        __unlock();

        return true;
    }

    return false;
}
