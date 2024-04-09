// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Modem Host Software
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "common/Log.h"
#include "p25/lookups/P25AffiliationLookup.h"
#include "p25/Control.h"

using namespace p25::lookups;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the P25AffiliationLookup class.
/// </summary>
/// <param name="name">Name of lookup table.</param>
/// <param name="channelLookup">Instance of the channel lookup class.</param>
/// <param name="verbose">Flag indicating whether verbose logging is enabled.</param>
P25AffiliationLookup::P25AffiliationLookup(Control* p25, ::lookups::ChannelLookup* chLookup, bool verbose) : ::lookups::AffiliationLookup("P25 Affiliation", chLookup, verbose),
    m_p25(p25)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the P25AffiliationLookup class.
/// </summary>
P25AffiliationLookup::~P25AffiliationLookup() = default;

/// <summary>
/// Helper to release the channel grant for the destination ID.
/// </summary>
/// <param name="dstId"></param>
/// <param name="releaseAll"></param>
bool P25AffiliationLookup::releaseGrant(uint32_t dstId, bool releaseAll)
{
    bool ret = ::lookups::AffiliationLookup::releaseGrant(dstId, releaseAll);
    if (ret) {
        if (m_rfGrantChCnt > 0U) {
            m_p25->m_siteData.setChCnt(m_chLookup->rfChSize() + m_rfGrantChCnt);
        }
        else {
            m_p25->m_siteData.setChCnt(m_chLookup->rfChSize());
        }
    }

    return ret;
}

/// <summary>
/// Helper to release group affiliations.
/// </summary>
/// <param name="dstId"></param>
/// <param name="releaseAll"></param>
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
