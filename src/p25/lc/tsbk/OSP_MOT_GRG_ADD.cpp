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
#include "Defines.h"
#include "p25/lc/tsbk/OSP_MOT_GRG_ADD.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the OSP_MOT_GRG_ADD class.
/// </summary>
OSP_MOT_GRG_ADD::OSP_MOT_GRG_ADD() : TSBK()
{
    m_lco = TSBK_OSP_MOT_GRG_ADD;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_MOT_GRG_ADD::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

    /* stub */

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void OSP_MOT_GRG_ADD::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    ulong64_t tsbkValue = 0U;

    m_mfId = P25_MFG_MOT;

    if ((m_patchSuperGroupId != 0U)) {
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

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void OSP_MOT_GRG_ADD::copy(const OSP_MOT_GRG_ADD& data)
{
    TSBK::copy(data);

    m_patchSuperGroupId = data.m_patchSuperGroupId;
    m_patchGroup1Id = data.m_patchGroup1Id;
    m_patchGroup2Id = data.m_patchGroup2Id;
    m_patchGroup3Id = data.m_patchGroup3Id;
}
