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
#include "p25/lc/tsbk/OSP_MOT_GRG_DEL.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_MOT_GRG_DEL class. */

OSP_MOT_GRG_DEL::OSP_MOT_GRG_DEL() : TSBK(),
    m_patchSuperGroupId(0U),
    m_patchGroup1Id(0U),
    m_patchGroup2Id(0U),
    m_patchGroup3Id(0U)
{
    m_lco = TSBKO::OSP_MOT_GRG_DEL;
}

/* Decode a trunking signalling block. */

bool OSP_MOT_GRG_DEL::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a trunking signalling block. */

void OSP_MOT_GRG_DEL::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    m_mfId = MFG_MOT;

    if ((m_patchSuperGroupId != 0U) && (m_patchGroup1Id != 0U)) {
        tsbkValue = m_patchSuperGroupId;                                            // Patch Super Group Address
        tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;                            // Patch Group 1 Address

        if (m_patchGroup2Id != 0U) {
            tsbkValue = (tsbkValue << 16) + m_patchGroup2Id;                        // Patch Group 2 Address
        }
        else {
            tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;                        // Patch Group 1 Address
        }

        if (m_patchGroup3Id != 0U) {
            tsbkValue = (tsbkValue << 16) + m_patchGroup3Id;                        // Patch Group 3 Address
        }
        else {
            tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;                        // Patch Group 1 Address
        }
    }
    else {
        LogError(LOG_P25, "OSP_MOT_GRG_DEL::encode(), invalid values for TSBKO::OSP_MOT_GRG_DEL, patchSuperGroupId = $%02X, patchGroup1Id = $%02X",
            m_patchSuperGroupId, m_patchGroup1Id);
        return; // blatantly ignore creating this TSBK
    }

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string OSP_MOT_GRG_DEL::toString(bool isp)
{
    return std::string("TSBKO, OSP_MOT_GRG_DEL (Motorola / Group Regroup Delete)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void OSP_MOT_GRG_DEL::copy(const OSP_MOT_GRG_DEL& data)
{
    TSBK::copy(data);

    m_patchSuperGroupId = data.m_patchSuperGroupId;
    m_patchGroup1Id = data.m_patchGroup1Id;
    m_patchGroup2Id = data.m_patchGroup2Id;
    m_patchGroup3Id = data.m_patchGroup3Id;
}
