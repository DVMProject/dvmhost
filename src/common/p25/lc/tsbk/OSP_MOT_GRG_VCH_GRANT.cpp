// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/OSP_MOT_GRG_VCH_GRANT.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_MOT_GRG_VCH_GRANT class. */
OSP_MOT_GRG_VCH_GRANT::OSP_MOT_GRG_VCH_GRANT() : TSBK(),
    m_patchSuperGroupId(0U)
{
    m_lco = TSBKO::OSP_MOT_GRG_VCH_GRANT;
}

/* Decode a trunking signalling block. */
bool OSP_MOT_GRG_VCH_GRANT::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a trunking signalling block. */
void OSP_MOT_GRG_VCH_GRANT::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    m_mfId = MFG_MOT;

    if (m_patchSuperGroupId != 0U) {
        tsbkValue = 0U;                                                             // Priority
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel ID
        tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                     // Channel Number
        tsbkValue = (tsbkValue << 16) + m_patchSuperGroupId;                        // Patch Supergroup Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
    }
    else {
        LogError(LOG_P25, "OSP_MOT_GRG_VCH_GRANT::encode(), invalid values for TSBKO::OSP_MOT_GRG_VCH_GRANT, patchSuperGroupId = $%02X", m_patchSuperGroupId);
        return; // blatantly ignore creating this TSBK
    }

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */
std::string OSP_MOT_GRG_VCH_GRANT::toString(bool isp)
{
    return std::string("TSBKO, OSP_MOT_GRG_VCH_GRANT (Group Regroup Voice Channel Grant)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */
void OSP_MOT_GRG_VCH_GRANT::copy(const OSP_MOT_GRG_VCH_GRANT& data)
{
    TSBK::copy(data);

    m_patchSuperGroupId = data.m_patchSuperGroupId;
}
