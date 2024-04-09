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
#include "lookups/AffiliationLookup.h"
#include "Log.h"

using namespace lookups;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the AffiliationLookup class.
/// </summary>
/// <param name="name">Name of lookup table.</param>
/// <param name="channelLookup">Instance of the channel lookup class.</param>
/// <param name="verbose">Flag indicating whether verbose logging is enabled.</param>
AffiliationLookup::AffiliationLookup(const std::string name, ChannelLookup* channelLookup, bool verbose) :
    m_rfGrantChCnt(0U),
    m_unitRegTable(),
    m_grpAffTable(),
    m_grantChTable(),
    m_grantSrcIdTable(),
    m_uuGrantedTable(),
    m_netGrantedTable(),
    m_grantTimers(),
    m_releaseGrant(nullptr),
    m_name(),
    m_chLookup(channelLookup),
    m_verbose(verbose)
{
    assert(channelLookup != nullptr);

    m_name = name;

    m_unitRegTable.clear();
    m_grpAffTable.clear();

    m_grantChTable.clear();
    m_grantSrcIdTable.clear();
    m_grantTimers.clear();
}

/// <summary>
/// Finalizes a instance of the AffiliationLookup class.
/// </summary>
AffiliationLookup::~AffiliationLookup() = default;

/// <summary>
/// Helper to group affiliate a source ID.
/// </summary>
/// <param name="srcId"></param>
void AffiliationLookup::unitReg(uint32_t srcId)
{
    if (isUnitReg(srcId)) {
        return;
    }

    m_unitRegTable.push_back(srcId);

    if (m_verbose) {
        LogMessage(LOG_HOST, "%s, unit registration, srcId = %u",
            m_name.c_str(), srcId);
    }
}

/// <summary>
/// Helper to group unaffiliate a source ID.
/// </summary>
/// <param name="srcId"></param>
bool AffiliationLookup::unitDereg(uint32_t srcId)
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

    // remove dynamic unit registration table entry
    if (std::find(m_unitRegTable.begin(), m_unitRegTable.end(), srcId) != m_unitRegTable.end()) {
        auto it = std::find(m_unitRegTable.begin(), m_unitRegTable.end(), srcId);
        m_unitRegTable.erase(it);
        ret = true;
    }

    return ret;
}

/// <summary>
/// Helper to determine if the source ID has unit registered.
/// </summary>
/// <param name="srcId"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to release unit registrations.
/// </summary>
void AffiliationLookup::clearUnitReg()
{
    std::vector<uint32_t> srcToRel = std::vector<uint32_t>();
    LogWarning(LOG_HOST, "%s, releasing all unit registrations", m_name.c_str());
    m_unitRegTable.clear();
}

/// <summary>
/// Helper to group affiliate a source ID.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
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

/// <summary>
/// Helper to group unaffiliate a source ID.
/// </summary>
/// <param name="srcId"></param>
bool AffiliationLookup::groupUnaff(uint32_t srcId)
{
    // lookup dynamic affiliation table entry
    auto entry = m_grpAffTable.find(srcId);    
    if (entry != m_grpAffTable.end()) {
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
        m_grpAffTable.at(srcId);
        m_grpAffTable.erase(srcId);
        return true;
    }
    catch (...) {
        return false;
    }
}

/// <summary>
/// Helper to determine if the group destination ID has any affiations.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <returns></returns>
bool AffiliationLookup::hasGroupAff(uint32_t dstId) const
{
    for (auto entry : m_grpAffTable) {
        if (entry.second == dstId) {
            return true;
        }
    }

    return false;
}

/// <summary>
/// Helper to determine if the source ID has affiliated to the group destination ID.
/// </summary>
/// <param name="srcId"></param>
/// <param name="dstId"></param>
/// <returns></returns>
bool AffiliationLookup::isGroupAff(uint32_t srcId, uint32_t dstId) const
{
    // lookup dynamic affiliation table entry
    auto entry = m_grpAffTable.find(srcId);    
    if (entry != m_grpAffTable.end()) {
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

/// <summary>
/// Helper to release group affiliations.
/// </summary>
/// <param name="dstId"></param>
/// <param name="releaseAll"></param>
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

/// <summary>
/// Helper to grant a channel.
/// </summary>
/// <param name="dstId"></param>
/// <param name="srcId"></param>
/// <param name="grantTimeout"></param>
/// <param name="grp"></param>
/// <param name="netGranted"></param>
/// <returns></returns>
bool AffiliationLookup::grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted)
{
    if (dstId == 0U) {
        return false;
    }

    if (!m_chLookup->isRFChAvailable()) {
        return false;
    }

    uint32_t chNo = m_chLookup->getFirstRFChannel();
    m_chLookup->removeRFCh(chNo);

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

/// <summary>
/// Helper to start the destination ID grant timer.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
void AffiliationLookup::touchGrant(uint32_t dstId)
{
    if (dstId == 0U) {
        return;
    }

    if (isGranted(dstId)) {
        m_grantTimers[dstId].start();
    }
}

/// <summary>
/// Helper to release the channel grant for the destination ID.
/// </summary>
/// <param name="dstId"></param>
/// <param name="releaseAll"></param>
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

/// <summary>
/// Helper to determine if the channel number is busy.
/// </summary>
/// <param name="chNo"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to determine if the destination ID is already granted.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to determine if the destination ID is network granted.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to determine if the destination ID is network granted.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to get the channel granted for the given destination ID.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to get the destination ID granted to the given source ID.
/// </summary>
/// <param name="srcId"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to get the source ID granted for the given destination ID.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
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

/// <summary>
/// Updates the processor by the passed number of milliseconds.
/// </summary>
/// <param name="ms"></param>
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
}
