// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "lookups/AffiliationLookup.h"
#include "Log.h"

using namespace lookups;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint32_t UNIT_REG_TIMEOUT = 43200U; // 12 hours

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the AffiliationLookup class. */

AffiliationLookup::AffiliationLookup(const std::string name, ChannelLookup* channelLookup, bool verbose) :
    m_rfGrantChCnt(0U),
    m_unitRegTable(),
    m_unitRegTimers(),
    m_grpAffTable(),
    m_grantChTable(),
    m_grantSrcIdTable(),
    m_uuGrantedTable(),
    m_netGrantedTable(),
    m_grantTimers(),
    m_releaseGrant(nullptr),
    m_name(),
    m_chLookup(channelLookup),
    m_disableUnitRegTimeout(false),
    m_verbose(verbose)
{
    assert(channelLookup != nullptr);

    m_name = name;

    m_unitRegTable.clear();
    m_unitRegTimers.clear();
    m_grpAffTable.clear();

    m_grantChTable.clear();
    m_grantSrcIdTable.clear();
    m_grantTimers.clear();
}

/* Finalizes a instance of the AffiliationLookup class. */

AffiliationLookup::~AffiliationLookup() = default;

/* Helper to group affiliate a source ID. */

void AffiliationLookup::unitReg(uint32_t srcId)
{
    if (isUnitReg(srcId)) {
        return;
    }

    m_unitRegTable.push_back(srcId);

    m_unitRegTimers[srcId] = Timer(1000U, UNIT_REG_TIMEOUT);
    m_unitRegTimers[srcId].start();

    if (m_verbose) {
        LogMessage(LOG_HOST, "%s, unit registration, srcId = %u",
            m_name.c_str(), srcId);
    }
}

/* Helper to group unaffiliate a source ID. */

bool AffiliationLookup::unitDereg(uint32_t srcId, bool automatic)
{
    bool ret = false;

    if (!isUnitReg(srcId)) {
        return false;
    }

    if (m_verbose) {
        LogMessage(LOG_HOST, "%s, unit deregistration, srcId = %u",
            m_name.c_str(), srcId);
    }

    groupUnaff(srcId);

    m_unitRegTimers[srcId].stop();

    // remove dynamic unit registration table entry
    if (std::find(m_unitRegTable.begin(), m_unitRegTable.end(), srcId) != m_unitRegTable.end()) {
        auto it = std::find(m_unitRegTable.begin(), m_unitRegTable.end(), srcId);
        m_unitRegTable.erase(it);
        ret = true;
    }

    if (ret) {
        if (m_unitDereg != nullptr) {
            m_unitDereg(srcId, automatic);
        }
    }

    return ret;
}

/* Helper to start the source ID registration timer. */

void AffiliationLookup::touchUnitReg(uint32_t srcId)
{
    if (srcId == 0U) {
        return;
    }

    if (isUnitReg(srcId)) {
        m_unitRegTimers[srcId].start();
    }
}

/* Gets the current timer timeout for this unit registration. */

uint32_t AffiliationLookup::unitRegTimeout(uint32_t srcId)
{
    if (srcId == 0U) {
        return 0U;
    }

    if (isUnitReg(srcId)) {
        return m_unitRegTimers[srcId].getTimeout();
    }

    return 0U;
}

/* Gets the current timer value for this unit registration. */

uint32_t AffiliationLookup::unitRegTimer(uint32_t srcId)
{
    if (srcId == 0U) {
        return 0U;
    }

    if (isUnitReg(srcId)) {
        return m_unitRegTimers[srcId].getTimer();
    }

    return 0U;
}

/* Helper to determine if the source ID has unit registered. */

bool AffiliationLookup::isUnitReg(uint32_t srcId) const
{
    // lookup dynamic unit registration table entry
    if (std::find(m_unitRegTable.begin(), m_unitRegTable.end(), srcId) != m_unitRegTable.end()) {
        return true;
    }
    else {
        return false;
    }
}

/* Helper to release unit registrations. */

void AffiliationLookup::clearUnitReg()
{
    std::vector<uint32_t> srcToRel = std::vector<uint32_t>();
    LogWarning(LOG_HOST, "%s, releasing all unit registrations", m_name.c_str());
    m_unitRegTable.clear();
}

/* Helper to group affiliate a source ID. */

void AffiliationLookup::groupAff(uint32_t srcId, uint32_t dstId)
{
    if (!isGroupAff(srcId, dstId)) {
        // update dynamic affiliation table
        m_grpAffTable[srcId] = dstId;

        if (m_verbose) {
            LogMessage(LOG_HOST, "%s, group affiliation, srcId = %u, dstId = %u",
                m_name.c_str(), srcId, dstId);
        }
    }
}

/* Helper to group unaffiliate a source ID. */

bool AffiliationLookup::groupUnaff(uint32_t srcId)
{
    // lookup dynamic affiliation table entry
    if (m_grpAffTable.find(srcId) != m_grpAffTable.end()) {
        uint32_t tblDstId = m_grpAffTable.at(srcId);
        if (m_verbose) {
            LogMessage(LOG_HOST, "%s, group unaffiliation, srcId = %u, dstId = %u",
                m_name.c_str(), srcId, tblDstId);
        }
    } else {
        return false;
    }

    // remove dynamic affiliation table entry
    try {
        uint32_t entry = m_grpAffTable.at(srcId); // this value will get discarded
        (void)entry;                              // but some variants of C++ mark the unordered_map<>::at as nodiscard
        m_grpAffTable.erase(srcId);
        return true;
    }
    catch (...) {
        return false;
    }
}

/* Helper to determine if the group destination ID has any affiations. */

bool AffiliationLookup::hasGroupAff(uint32_t dstId) const
{
    for (auto entry : m_grpAffTable) {
        if (entry.second == dstId) {
            return true;
        }
    }

    return false;
}

/* Helper to determine if the source ID has affiliated to the group destination ID. */

bool AffiliationLookup::isGroupAff(uint32_t srcId, uint32_t dstId) const
{
    // lookup dynamic affiliation table entry
    if (m_grpAffTable.find(srcId) != m_grpAffTable.end()) {
        uint32_t tblDstId = m_grpAffTable.at(srcId);
        if (tblDstId == dstId) {
            return true;
        }
        else {
            return false;
        }
    }

    return false;
}

/* Helper to release group affiliations. */

std::vector<uint32_t> AffiliationLookup::clearGroupAff(uint32_t dstId, bool releaseAll)
{
    std::vector<uint32_t> srcToRel = std::vector<uint32_t>();
    if (dstId == 0U && !releaseAll) {
        return srcToRel;
    }

    if (dstId == 0U && releaseAll) {
        LogWarning(LOG_HOST, "%s, releasing all group affiliations", m_name.c_str());
        for (auto entry : m_grpAffTable) {
            uint32_t srcId = entry.first;
            srcToRel.push_back(srcId);
        }
    }
    else {
        LogWarning(LOG_HOST, "%s, releasing group affiliations, dstId = %u", m_name.c_str(), dstId);
        for (auto entry : m_grpAffTable) {
            uint32_t srcId = entry.first;
            uint32_t grpId = entry.second;
            if (grpId == dstId) {
                srcToRel.push_back(srcId);
            }
        }
    }

    for (auto srcId : srcToRel) {
        m_grpAffTable.erase(srcId);
    }

    return srcToRel;
}

/* Helper to grant a channel. */

bool AffiliationLookup::grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted)
{
    if (dstId == 0U) {
        return false;
    }

    if (!m_chLookup->isRFChAvailable()) {
        return false;
    }

    uint32_t chNo = m_chLookup->getFirstRFChannel();
    if (!m_chLookup->removeRFCh(chNo)) {
        return false;
    }

    m_grantChTable[dstId] = chNo;
    m_grantSrcIdTable[dstId] = srcId;
    m_rfGrantChCnt++;

    m_uuGrantedTable[dstId] = !grp;
    m_netGrantedTable[dstId] = netGranted;

    m_grantTimers[dstId] = Timer(1000U, grantTimeout);
    m_grantTimers[dstId].start();

    if (m_verbose) {
        LogMessage(LOG_HOST, "%s, granting channel, chNo = %u, dstId = %u, srcId = %u, group = %u",
            m_name.c_str(), chNo, dstId, srcId, grp);
    }

    return true;
}

/* Helper to start the destination ID grant timer. */

void AffiliationLookup::touchGrant(uint32_t dstId)
{
    if (dstId == 0U) {
        return;
    }

    if (isGranted(dstId)) {
        m_grantTimers[dstId].start();
    }
}

/* Helper to release the channel grant for the destination ID. */

bool AffiliationLookup::releaseGrant(uint32_t dstId, bool releaseAll)
{
    if (dstId == 0U && !releaseAll) {
        return false;
    }

    // are we trying to release all grants?
    if (dstId == 0U && releaseAll) {
        LogWarning(LOG_HOST, "%s, force releasing all channel grants", m_name.c_str());

        std::vector<uint32_t> gntsToRel = std::vector<uint32_t>();
        for (auto entry : m_grantChTable) {
            uint32_t dstId = entry.first;
            gntsToRel.push_back(dstId);
        }

        // release grants
        for (uint32_t dstId : gntsToRel) {
            releaseGrant(dstId, false);
        }

        return true;
    }

    if (isGranted(dstId)) {
        uint32_t chNo = m_grantChTable.at(dstId);

        if (m_verbose) {
            LogMessage(LOG_HOST, "%s, releasing channel grant, chNo = %u, dstId = %u",
                m_name.c_str(), chNo, dstId);
        }

        if (m_releaseGrant != nullptr) {
            m_releaseGrant(chNo, dstId, 0U);
        }

        m_grantChTable.erase(dstId);
        m_grantSrcIdTable.erase(dstId);
        m_uuGrantedTable.erase(dstId);
        m_netGrantedTable.erase(dstId);
        m_chLookup->addRFCh(chNo, true);

        if (m_rfGrantChCnt > 0U) {
            m_rfGrantChCnt--;
        }
        else {
            m_rfGrantChCnt = 0U;
        }

        m_grantTimers[dstId].stop();

        return true;
    }

    return false;
}

/* Helper to determine if the channel number is busy. */

bool AffiliationLookup::isChBusy(uint32_t chNo) const
{
    if (chNo == 0U) {
        return false;
    }

    // lookup dynamic channel grant table entry
    for (auto entry : m_grantChTable) {
        if (entry.second == chNo) {
            return true;
        }
    }

    return false;
}

/* Helper to determine if the destination ID is already granted. */

bool AffiliationLookup::isGranted(uint32_t dstId) const
{
    if (dstId == 0U) {
        return false;
    }

    // lookup dynamic channel grant table entry
    try {
        uint32_t chNo = m_grantChTable.at(dstId);
        if (chNo != 0U) {
            return true;
        }
        else {
            return false;
        }
    } catch (...) {
        return false;
    }
}

/* Helper to determine if the destination ID is network granted. */

bool AffiliationLookup::isGroup(uint32_t dstId) const
{
    if (dstId == 0U) {
        return true;
    }

    // lookup dynamic channel grant table entry
    try {
        bool uu = m_uuGrantedTable.at(dstId);
        return !uu;
    } catch (...) {
        return true;
    }
}

/* Helper to determine if the destination ID is network granted. */

bool AffiliationLookup::isNetGranted(uint32_t dstId) const
{
    if (dstId == 0U) {
        return false;
    }

    // lookup dynamic channel grant table entry
    try {
        bool net = m_netGrantedTable.at(dstId);
        return net;
    } catch (...) {
        return false;
    }
}

/* Helper to get the channel granted for the given destination ID. */

uint32_t AffiliationLookup::getGrantedCh(uint32_t dstId)
{
    if (dstId == 0U) {
        return 0U;
    }

    if (isGranted(dstId)) {
        return m_grantChTable[dstId];
    }

    return 0U;
}

/* Helper to get the destination ID for the given channel. */

uint32_t AffiliationLookup::getGrantedDstByCh(uint32_t chNo)
{
    for (auto entry : m_grantChTable) {
        if (entry.second == chNo)
            return entry.first;
    }

    return 0U;
}

/* Helper to get the destination ID granted to the given source ID. */

uint32_t AffiliationLookup::getGrantedBySrcId(uint32_t srcId)
{
    if (srcId == 0U) {
        return 0U;
    }

    // lookup dynamic channel grant table entry
    for (auto entry : m_grantSrcIdTable) {
        if (entry.second == srcId) {
            return entry.first;
        }
    }

    return 0U;
}

/* Helper to get the source ID granted for the given destination ID. */

uint32_t AffiliationLookup::getGrantedSrcId(uint32_t dstId)
{
    if (dstId == 0U) {
        return 0U;
    }

    if (isGranted(dstId)) {
        return m_grantSrcIdTable[dstId];
    }

    return 0U;
}

/* Updates the processor by the passed number of milliseconds. */

void AffiliationLookup::clock(uint32_t ms)
{
    // clock all the grant timers
    std::vector<uint32_t> gntsToRel = std::vector<uint32_t>();
    for (auto entry : m_grantChTable) {
        uint32_t dstId = entry.first;

        m_grantTimers[dstId].clock(ms);
        if (m_grantTimers[dstId].isRunning() && m_grantTimers[dstId].hasExpired()) {
            gntsToRel.push_back(dstId);
        }
    }

    // release grants that have timed out
    for (uint32_t dstId : gntsToRel) {
        releaseGrant(dstId, false);
    }

    if (!m_disableUnitRegTimeout) {
        // clock all the unit registration timers
        std::vector<uint32_t> unitsToDereg = std::vector<uint32_t>();
        for (uint32_t srcId : m_unitRegTable) {
            m_unitRegTimers[srcId].clock(ms);
            if (m_unitRegTimers[srcId].isRunning() && m_unitRegTimers[srcId].hasExpired()) {
                unitsToDereg.push_back(srcId);
            }
        }

        // release units registrations that have timed out
        for (uint32_t srcId : unitsToDereg) {
            unitDereg(srcId, true);
        }
    }
}
