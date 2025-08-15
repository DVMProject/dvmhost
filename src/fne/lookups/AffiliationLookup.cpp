// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
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
    // lookup dynamic channel grant table entry
    m_unitRegPeerTable.lock(false);
    for (auto entry : m_grantChTable) {
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
