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
#include "p25/lookups/P25AffiliationLookup.h"
#include "p25/packet/Trunk.h"
#include "p25/Control.h"
#include "Log.h"

using namespace p25::lookups;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the P25AffiliationLookup class.
/// </summary>
/// <param name="name">Name of lookup table.</param>
/// <param name="verbose">Flag indicating whether verbose logging is enabled.</param>
P25AffiliationLookup::P25AffiliationLookup(Control* p25, bool verbose) : ::lookups::AffiliationLookup("P25 Affiliation", verbose),
    m_p25(p25)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the P25AffiliationLookup class.
/// </summary>
P25AffiliationLookup::~P25AffiliationLookup()
{
    /* stub */
}

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
            m_p25->m_siteData.setChCnt(getRFChCnt() + m_rfGrantChCnt);
        }
        else {
            m_p25->m_siteData.setChCnt(getRFChCnt());
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
            m_p25->m_trunk->writeRF_TSDU_U_Dereg_Ack(srcId);
        }
    }

    return srcToRel;
}
