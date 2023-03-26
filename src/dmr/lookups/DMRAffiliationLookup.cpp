/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
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
#include "dmr/lookups/DMRAffiliationLookup.h"
#include "Log.h"

using namespace dmr::lookups;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the DMRAffiliationLookup class.
/// </summary>
/// <param name="verbose">Flag indicating whether verbose logging is enabled.</param>
DMRAffiliationLookup::DMRAffiliationLookup(bool verbose) : ::lookups::AffiliationLookup("DMR Affiliation", verbose),
    m_grantChSlotTable(),
    m_tsccChNo(0U),
    m_tsccSlot(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the DMRAffiliationLookup class.
/// </summary>
DMRAffiliationLookup::~DMRAffiliationLookup()
{
    /* stub */
}

/// <summary>
/// Helper to grant a channel.
/// </summary>
/// <param name="dstId"></param>
/// <param name="srcId"></param>
/// <param name="grantTimeout"></param>
/// <returns></returns>
bool DMRAffiliationLookup::grantCh(uint32_t dstId, uint32_t srcId, uint32_t grantTimeout)
{
    uint32_t chNo = m_rfChTable.at(0);
    uint8_t slot = getAvailableSlotForChannel(chNo);

    if (slot == 0U) {
        return false;
    }

    return grantChSlot(dstId, srcId, slot, grantTimeout);
}

/// <summary>
/// Helper to grant a channel and slot.
/// </summary>
/// <param name="dstId"></param>
/// <param name="srcId"></param>
/// <param name="slot"></param>
/// <param name="grantTimeout"></param>
/// <returns></returns>
bool DMRAffiliationLookup::grantChSlot(uint32_t dstId, uint32_t srcId, uint8_t slot, uint32_t grantTimeout)
{
    if (dstId == 0U) {
        return false;
    }

    if (!isRFChAvailable()) {
        return false;
    }

    uint32_t chNo = m_rfChTable.at(0);
    if (chNo == m_tsccChNo && slot == m_tsccSlot) {
        return false;
    }

    if (getAvailableSlotForChannel(chNo) == 0U || chNo == m_tsccChNo) {
        auto it = std::find(m_rfChTable.begin(), m_rfChTable.end(), chNo);
        m_rfChTable.erase(it);
    }

    m_grantChTable[dstId] = chNo;
    m_grantSrcIdTable[dstId] = srcId;
    m_grantChSlotTable[dstId] = std::make_tuple(chNo, slot);
    m_rfGrantChCnt++;

    m_grantTimers[dstId] = Timer(1000U, grantTimeout);
    m_grantTimers[dstId].start();

    if (m_verbose) {
        LogMessage(LOG_HOST, "%s, granting channel, chNo = %u, slot = %u, dstId = %u",
            m_name, chNo, slot, dstId);
    }

    return true;
}

/// <summary>
/// Helper to release the channel grant for the destination ID.
/// </summary>
/// <param name="dstId"></param>
/// <param name="releaseAll"></param>
bool DMRAffiliationLookup::releaseGrant(uint32_t dstId, bool releaseAll)
{
    if (dstId == 0U && !releaseAll) {
        return false;
    }

    // are we trying to release all grants?
    if (dstId == 0U && releaseAll) {
        LogWarning(LOG_HOST, "%s, force releasing all channel grants", m_name);

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
        std::tuple<uint32_t, uint8_t> slotData = m_grantChSlotTable.at(dstId);
        uint8_t slot = std::get<1>(slotData);

        if (m_verbose) {
            LogMessage(LOG_HOST, "%s, releasing channel grant, chNo = %u, slot = %u, dstId = %u",
                m_name, chNo, slot, dstId);
        }

        if (m_releaseGrant != nullptr) {
            m_releaseGrant(chNo, dstId, slot);
        }

        m_grantChTable.erase(dstId);
        m_grantSrcIdTable.erase(dstId);
        m_grantChSlotTable.erase(dstId);

        auto it = std::find(m_rfChTable.begin(), m_rfChTable.end(), chNo);
        if (it == m_rfChTable.end()) {
            m_rfChTable.push_back(chNo);
        }

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

/// <summary>
/// Helper to get the slot granted for the given destination ID.
/// </summary>
/// <param name="dstId"></param>
/// <returns></returns>
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

/// <summary>
/// Helper to set a slot for the given channel as being the TSCC.
/// </summary>
/// <param name="chNo"></param>
/// <param name="slot"></param>
/// <returns></returns>
void DMRAffiliationLookup::setSlotForChannelTSCC(uint32_t chNo, uint8_t slot)
{
    assert(chNo != 0U);
    if ((slot == 0U) || (slot > 2U)) {
        return;
    }

    m_tsccChNo = chNo;
    m_tsccSlot = slot;
}

/// <summary>
/// Helper to determine the first available slot for given the channel number.
/// </summary>
/// <param name="chNo"></param>
/// <returns></returns>
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
