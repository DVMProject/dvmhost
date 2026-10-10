// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Modem Host Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023-2026 Bryan Biedenkapp, N2PLL
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

    uint32_t chNo = getAvailableChannelForSlot(slot);
    if (chNo == 0U) {
        return false;
    }

    if (chNo == m_tsccChNo && slot == m_tsccSlot) {
        return false;
    }

    bool channelAlreadyManaged = chNo == m_tsccChNo;
    for (auto entry : m_grantChSlotTable) {
        if (std::get<0>(entry.second) == chNo) {
            channelAlreadyManaged = true;
            break;
        }
    }

    if (!channelAlreadyManaged && !m_chLookup->allocRFCh(chNo)) {
        return false;
    }

    __lock();

    m_grantChTable[dstId] = chNo;
    m_grantSrcIdTable[dstId] = srcId;
    m_grantChSlotTable[dstId] = std::make_tuple(chNo, slot);
    m_rfGrantChCnt++;

    m_uuGrantedTable[dstId] = !grp;
    m_netGrantedTable[dstId] = netGranted;

    m_grantTimers[dstId] = Timer(1000U, grantTimeout);
    m_grantTimers[dstId].start();

    if (m_verbose) {
        LogInfoEx(LOG_HOST, "%s, granting channel, chNo = %u, slot = %u, dstId = %u, group = %u",
            m_name.c_str(), chNo, slot, dstId, grp);
    }

    __unlock();

    return true;
}

/* Helper to release the channel grant for the destination ID. */

bool DMRAffiliationLookup::releaseGrant(uint32_t dstId, bool releaseAll)
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
        uint32_t srcId = getGrantedSrcId(dstId);
        std::tuple<uint32_t, uint8_t> slotData = m_grantChSlotTable.at(dstId);
        uint8_t slot = std::get<1>(slotData);

        if (m_verbose) {
            LogInfoEx(LOG_HOST, "%s, releasing channel grant, chNo = %u, slot = %u, dstId = %u",
                m_name.c_str(), chNo, slot, dstId);
        }

        if (m_releaseGrant != nullptr) {
            m_releaseGrant(chNo, srcId, dstId, slot);
        }

        __lock();

        m_grantChTable.erase(dstId);
        m_grantSrcIdTable.erase(dstId);
        m_grantChSlotTable.erase(dstId);
        m_netGrantedTable.erase(dstId);
        m_uuGrantedTable.erase(dstId);

        bool channelStillGranted = false;
        for (auto entry : m_grantChSlotTable) {
            if (std::get<0>(entry.second) == chNo) {
                channelStillGranted = true;
                break;
            }
        }
        if (!channelStillGranted && chNo != m_tsccChNo) {
            m_chLookup->freeRFCh(chNo);
        }

        if (m_rfGrantChCnt > 0U) {
            m_rfGrantChCnt--;
        }
        else {
            m_rfGrantChCnt = 0U;
        }

        m_grantTimers[dstId].stop();
        m_grantTimers.erase(dstId);

        __unlock();

        return true;
    }

    return false;
}

/* Helper to determine if the channel number is busy. */

bool DMRAffiliationLookup::isChBusy(uint32_t chNo) const
{
    if (chNo == 0U) {
        return false;
    }

    __spinlock();

    bool slot1Busy = chNo == m_tsccChNo && m_tsccSlot == 1U;
    bool slot2Busy = chNo == m_tsccChNo && m_tsccSlot == 2U;
    for (auto entry : m_grantChSlotTable) {
        if (std::get<0>(entry.second) != chNo)
            continue;

        if (std::get<1>(entry.second) == 1U)
            slot1Busy = true;
        if (std::get<1>(entry.second) == 2U)
            slot2Busy = true;
    }

    return slot1Busy && slot2Busy;
}

/* Helper to get the slot granted for the given destination ID. */

uint8_t DMRAffiliationLookup::getGrantedSlot(uint32_t dstId) const
{
    if (dstId == 0U) {
        return 0U;
    }

    __spinlock();

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

    // Reserve the physical control channel in the shared allocator. DMR tracks
    // the opposing payload timeslot separately in m_grantChSlotTable.
    m_chLookup->allocRFCh(chNo);
}

/* Helper to determine the an available channel for a slot. */

uint32_t DMRAffiliationLookup::getAvailableChannelForSlot(uint8_t slot) const
{
    if (slot == 0U) {
        return 0U;
    }

    __spinlock();

    // Prefer a physical channel already managed by DMR so its unused timeslot
    // can be filled without changing the shared ChannelLookup allocation model.
    for (auto grant : m_grantChSlotTable) {
        uint32_t chNo = std::get<0>(grant.second);
        bool requestedSlotBusy = false;
        for (auto other : m_grantChSlotTable) {
            if (std::get<0>(other.second) == chNo && std::get<1>(other.second) == slot) {
                requestedSlotBusy = true;
                break;
            }
        }

        if (!requestedSlotBusy && !(chNo == m_tsccChNo && slot == m_tsccSlot))
            return chNo;
    }

    // The timeslot opposing the TSCC is also a valid payload resource.
    if (m_tsccChNo != 0U && slot != m_tsccSlot) {
        bool requestedSlotBusy = false;
        for (auto grant : m_grantChSlotTable) {
            if (std::get<0>(grant.second) == m_tsccChNo && std::get<1>(grant.second) == slot) {
                requestedSlotBusy = true;
                break;
            }
        }
        if (!requestedSlotBusy)
            return m_tsccChNo;
    }

    // For a new physical channel, preserve ChannelLookup's allocation state so
    // P25 and NXDN users of that shared class are unaffected.
    uint32_t chNo = m_chLookup->getFirstRFChannel();
    if (chNo == m_tsccChNo && slot == m_tsccSlot)
        return 0U;
    return chNo;
}

/* Helper to determine the first available slot for given the channel number. */

uint8_t DMRAffiliationLookup::getAvailableSlotForChannel(uint32_t chNo) const
{
    if (chNo == 0U) {
        return 0U;
    }

    __spinlock();

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
