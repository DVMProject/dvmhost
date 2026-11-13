// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024,2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/Log.h"
#include "p25/lookups/P25AffiliationLookup.h"
#include "p25/Control.h"

#include <algorithm>

using namespace p25::lookups;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the P25AffiliationLookup class. */

P25AffiliationLookup::P25AffiliationLookup(Control* p25, ::lookups::ChannelLookup* chLookup, bool verbose) : ::lookups::AffiliationLookup("P25 Affiliation", chLookup, verbose),
    m_p25(p25)
{
    /* stub */
}

/* Finalizes a instance of the P25AffiliationLookup class. */

P25AffiliationLookup::~P25AffiliationLookup() = default;

/* Produces the stable unordered key for a private-call RID pair. */

uint64_t P25AffiliationLookup::u2uPairKey(uint32_t srcId, uint32_t dstId)
{
    if (srcId == 0U || dstId == 0U || srcId == dstId) {
        return 0U;
    }

    const uint32_t low = std::min(srcId, dstId) & 0xFFFFFFU;
    const uint32_t high = std::max(srcId, dstId) & 0xFFFFFFU;
    return (static_cast<uint64_t>(low) << 24) | high;
}

/* Grants a channel and records private-call participant identity. */

bool P25AffiliationLookup::grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted)
{
    if (!grp && isU2UGranted(srcId, dstId)) {
        return false;
    }

    if (!grp) {
        for (const auto& entry : m_u2uGrantOwners) {
            const uint32_t low = static_cast<uint32_t>((entry.first >> 24) & 0xFFFFFFU);
            const uint32_t high = static_cast<uint32_t>(entry.first & 0xFFFFFFU);
            if (low == srcId || low == dstId || high == srcId || high == dstId) {
                return false;
            }
        }
    }

    const bool granted = ::lookups::AffiliationLookup::grantCh(dstId, srcId, grantTimeout, grp, netGranted);
    if (granted && !grp) {
        const uint64_t key = u2uPairKey(srcId, dstId);
        if (key != 0U) {
            m_u2uGrantOwners[key] = dstId;
            m_u2uGrantSources[dstId] = srcId;
            m_u2uGrantTargets[dstId] = dstId;
        }
    }

    return granted;
}

/* Helper to release group affiliations. */

std::vector<uint32_t> P25AffiliationLookup::clearGroupAff(uint32_t dstId, bool releaseAll)
{
    std::vector<uint32_t> srcToRel = ::lookups::AffiliationLookup::clearGroupAff(dstId, releaseAll);
    if (srcToRel.size() > 0U) {
        // release affiliations
        for (uint32_t srcId : srcToRel) {
            m_p25->m_control->writeRF_TSDU_U_Dereg_Ack(srcId);
        }
    }

    return srcToRel;
}

/* Helper to release the channel grant for the destination ID. */

bool P25AffiliationLookup::releaseGrant(uint32_t dstId, bool releaseAll)
{
    uint32_t grantOwner = dstId;
    if (!releaseAll && !::lookups::AffiliationLookup::isGranted(grantOwner)) {
        for (const auto& entry : m_u2uGrantOwners) {
            const uint32_t low = static_cast<uint32_t>((entry.first >> 24) & 0xFFFFFFU);
            const uint32_t high = static_cast<uint32_t>(entry.first & 0xFFFFFFU);
            if (low == dstId || high == dstId) {
                grantOwner = entry.second;
                break;
            }
        }
    }

    bool ret = ::lookups::AffiliationLookup::releaseGrant(grantOwner, releaseAll);
    if (ret) {
        if (releaseAll) {
            m_u2uGrantOwners.clear();
            m_u2uGrantSources.clear();
            m_u2uGrantTargets.clear();
        }
        else {
            for (auto it = m_u2uGrantOwners.begin(); it != m_u2uGrantOwners.end(); ) {
                if (it->second == grantOwner) {
                    it = m_u2uGrantOwners.erase(it);
                }
                else {
                    ++it;
                }
            }

            m_u2uGrantSources.erase(grantOwner);
            m_u2uGrantTargets.erase(grantOwner);
        }

        if (m_rfGrantChCnt > 0U) {
            m_p25->m_siteData.setChCnt(m_chLookup->rfChSize() + m_rfGrantChCnt);
        }
        else {
            m_p25->m_siteData.setChCnt(m_chLookup->rfChSize());
        }
    }

    return ret;
}

/* Resolves a unit-to-unit participant pair to its single grant owner. */

uint32_t P25AffiliationLookup::getU2UGrantOwner(uint32_t srcId, uint32_t dstId) const
{
    const uint64_t key = u2uPairKey(srcId, dstId);
    auto it = m_u2uGrantOwners.find(key);
    if (it == m_u2uGrantOwners.end()) {
        return 0U;
    }

    return ::lookups::AffiliationLookup::isGranted(it->second) ? it->second : 0U;
}

/* Resolves one private-call participant to its grant owner. */

uint32_t P25AffiliationLookup::getU2UGrantOwner(uint32_t unitId) const
{
    for (const auto& entry : m_u2uGrantOwners) {
        const uint32_t low = static_cast<uint32_t>((entry.first >> 24) & 0xFFFFFFU);
        const uint32_t high = static_cast<uint32_t>(entry.first & 0xFFFFFFU);
        if (low == unitId || high == unitId) {
            return ::lookups::AffiliationLookup::isGranted(entry.second) ? entry.second : 0U;
        }
    }

    return 0U;
}

/* Releases the active unit-to-unit grant involving a participant. */

bool P25AffiliationLookup::releaseU2UGrant(uint32_t unitId)
{
    const uint32_t grantOwner = getU2UGrantOwner(unitId);
    return grantOwner != 0U && releaseGrant(grantOwner, false);
}

/* Updates the directional signaling identities for an active private grant. */

bool P25AffiliationLookup::setU2UGrantDirection(uint32_t srcId, uint32_t dstId)
{
    const uint32_t grantOwner = getU2UGrantOwner(srcId, dstId);
    if (grantOwner == 0U) {
        return false;
    }

    m_u2uGrantSources[grantOwner] = srcId;
    m_u2uGrantTargets[grantOwner] = dstId;
    return true;
}

/* Gets the current directional source RID for a private grant owner. */

uint32_t P25AffiliationLookup::getU2UGrantSource(uint32_t grantOwner) const
{
    auto it = m_u2uGrantSources.find(grantOwner);
    return (it != m_u2uGrantSources.end()) ? it->second : 0U;
}

/* Gets the current directional target RID for a private grant owner. */

uint32_t P25AffiliationLookup::getU2UGrantTarget(uint32_t grantOwner) const
{
    auto it = m_u2uGrantTargets.find(grantOwner);
    return (it != m_u2uGrantTargets.end()) ? it->second : 0U;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Tests whether a unit-to-unit participant pair has an active grant. */

bool P25AffiliationLookup::isU2UGranted(uint32_t srcId, uint32_t dstId) const
{
    return getU2UGrantOwner(srcId, dstId) != 0U;
}
