// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/Log.h"
#include "dmr/lookups/DMRAffiliationLookup.h"

using namespace dmr::lookups;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the DMRAffiliationLookup class. */

DMRAffiliationLookup::DMRAffiliationLookup(::lookups::ChannelLookup* chLookup, bool verbose) : ::lookups::AffiliationLookup("DMR Affiliation", chLookup, verbose),
    m_grantChSlotTable(),
    m_tsccChNo(0U),
    m_tsccSlot(0U)
{
    /* stub */
}

/* Finalizes a instance of the DMRAffiliationLookup class. */

DMRAffiliationLookup::~DMRAffiliationLookup() = default;

/* Helper to grant a channel. */

bool DMRAffiliationLookup::grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout, bool grp, bool netGranted)
{
    ::LogDebugEx(LOG_HOST, "%s", "DMRAffiliationLookup::grantCh()", "use grantChSlot() BUGBUG");
    return false;
}

/* Helper to grant a channel and slot. */

bool DMRAffiliationLookup::grantChSlot(uint32_t dstId, uint32_t srcId, uint8_t slot, uint32_t grantTimeout, bool grp, bool netGranted)
{
    if (dstId == 0U) {
        return false;
    }

    if (!m_chLookup->isRFChAvailable()) {
        return false;
    }

    uint32_t chNo = getAvailableChannelForSlot(slot);
    if (chNo == 0U) {
        return false;
    }

    if (chNo == m_tsccChNo && slot == m_tsccSlot) {
        return false;
    }

    if (getAvailableSlotForChannel(chNo) == 0U || chNo == m_tsccChNo) {
        m_chLookup->removeRFCh(chNo);
    }

    m_grantChTable[dstId] = chNo;
    m_grantSrcIdTable[dstId] = srcId;
    m_grantChSlotTable[dstId] = std::make_tuple(chNo, slot);
    m_rfGrantChCnt++;

    m_uuGrantedTable[dstId] = !grp;
    m_netGrantedTable[dstId] = netGranted;

    m_grantTimers[dstId] = Timer(1000U, grantTimeout);
    m_grantTimers[dstId].start();

    if (m_verbose) {
        LogMessage(LOG_HOST, "%s, granting channel, chNo = %u, slot = %u, dstId = %u, group = %u",
            m_name.c_str(), chNo, slot, dstId, grp);
    }

    return true;
}

/* Helper to release the channel grant for the destination ID. */

bool DMRAffiliationLookup::releaseGrant(uint32_t dstId, bool releaseAll, bool noLock)
{
    if (dstId == 0U && !releaseAll) {
        return false;
    }

    if (!noLock)
        m_mutex.lock();

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

        if (!noLock)
            m_mutex.unlock();
        return true;
    }

    if (isGranted(dstId)) {
        uint32_t chNo = m_grantChTable.at(dstId);
        std::tuple<uint32_t, uint8_t> slotData = m_grantChSlotTable.at(dstId);
        uint8_t slot = std::get<1>(slotData);

        if (m_verbose) {
            LogMessage(LOG_HOST, "%s, releasing channel grant, chNo = %u, slot = %u, dstId = %u",
                m_name.c_str(), chNo, slot, dstId);
        }

        if (m_releaseGrant != nullptr) {
            m_releaseGrant(chNo, dstId, slot);
        }

        m_grantChTable.erase(dstId);
        m_grantSrcIdTable.erase(dstId);
        m_grantChSlotTable.erase(dstId);
        m_netGrantedTable.erase(dstId);

        m_chLookup->addRFCh(chNo);

        if (m_rfGrantChCnt > 0U) {
            m_rfGrantChCnt--;
        }
        else {
            m_rfGrantChCnt = 0U;
        }

        m_grantTimers[dstId].stop();

        if (!noLock)
            m_mutex.unlock();
        return true;
    }

    if (!noLock)
        m_mutex.unlock();
    return false;
}

/* Helper to determine if the channel number is busy. */

bool DMRAffiliationLookup::isChBusy(uint32_t chNo) const
{
    if (chNo == 0U) {
        return false;
    }

    // lookup dynamic channel grant table entry
    for (auto grantEntry : m_grantChTable) {
        if (grantEntry.second == chNo) {
            uint8_t slotCount = 0U;
            if (chNo == m_tsccChNo) {
                slotCount++; // one slot is *always* used for TSCC
            }

            for (auto slotEntry : m_grantChSlotTable) {
                uint32_t foundChNo = std::get<0>(slotEntry.second);
                if (foundChNo == chNo)
                    slotCount++;
            }

            if (slotCount == 2U) {
                return true;
            } else {
                return false;
            }
        }
    }

    return false;
}

/* Helper to get the slot granted for the given destination ID. */

uint8_t DMRAffiliationLookup::getGrantedSlot(uint32_t dstId) const
{
    if (dstId == 0U) {
        return 0U;
    }

    // lookup dynamic channel grant table entry
    for (auto entry : m_grantChSlotTable) {
        if (entry.first == dstId) {
            uint8_t slot = std::get<1>(entry.second);
            return slot;
        }
    }

    return 0U;
}

/* Helper to set a slot for the given channel as being the TSCC. */

void DMRAffiliationLookup::setSlotForChannelTSCC(uint32_t chNo, uint8_t slot)
{
    assert(chNo != 0U);
    if ((slot == 0U) || (slot > 2U)) {
        return;
    }

    m_tsccChNo = chNo;
    m_tsccSlot = slot;
}

/* Helper to determine the an available channel for a slot. */

uint32_t DMRAffiliationLookup::getAvailableChannelForSlot(uint8_t slot) const
{
    if (slot == 0U) {
        return 0U;
    }

    uint32_t chNo = 0U;
    for (auto entry : m_chLookup->rfChDataTable()) {
        if (entry.second.chNo() == m_tsccChNo && slot == m_tsccSlot) {
            continue;
        } else {
            // lookup dynamic channel slot grant table entry
            bool chAvailSlot = false;
            for (auto gntEntry : m_grantChSlotTable) {
                uint32_t foundChNo = std::get<0>(gntEntry.second);
                if (foundChNo == entry.second.chNo()) {
                    uint8_t foundSlot = std::get<1>(gntEntry.second);
                    if (slot == foundSlot)
                        continue;
                    else {
                        chAvailSlot = true;
                        break;
                    }
                }
            }

            // if we have no granted channels -- return true
            if (m_grantChSlotTable.size() == 0U)
                chAvailSlot = true;

            chNo = entry.second.chNo();
            if (chAvailSlot)
                break;
        }
    }

    return chNo;
}

/* Helper to determine the first available slot for given the channel number. */

uint8_t DMRAffiliationLookup::getAvailableSlotForChannel(uint32_t chNo) const
{
    if (chNo == 0U) {
        return 0U;
    }

    uint8_t slot = 1U;

    // lookup dynamic channel slot grant table entry
    bool grantedSlot = false;
    int slotCount = 0U;
    for (auto entry : m_grantChSlotTable) {
        uint32_t foundChNo = std::get<0>(entry.second);
        if (foundChNo == chNo)
        {
            uint8_t foundSlot = std::get<1>(entry.second);
            if (slot == foundSlot) {
                switch (foundSlot) {
                case 1U:
                    slot = 2U;
                    break;
                case 2U:
                    slot = 1U;
                    break;
                }

                grantedSlot = true;
                slotCount++;
            }
        }
    }

    if (slotCount == 2U) {
        slot = 0U;
        return slot;
    }

    // are we trying to assign the TSCC slot?
    if (chNo == m_tsccChNo && slot == m_tsccSlot) {
        if (!grantedSlot) {
            // since we didn't find a slot being granted out -- utilize the slot opposing the TSCC
            switch (m_tsccSlot) {
            case 1U:
                slot = 2U;
                break;
            case 2U:
                slot = 1U;
                break;
            }
        } else {
            slot = 0U; // TSCC is not assignable
        }
    }

    return slot;
}
