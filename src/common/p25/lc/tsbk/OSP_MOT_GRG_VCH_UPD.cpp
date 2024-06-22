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
#include "Defines.h"
#include "p25/lc/tsbk/OSP_MOT_GRG_VCH_UPD.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the OSP_MOT_GRG_VCH_UPD class.
/// </summary>
OSP_MOT_GRG_VCH_UPD::OSP_MOT_GRG_VCH_UPD() : TSBK(),
    m_patchGroup1Id(0U),
    m_patchGroup2Id(0U)
{
    m_lco = TSBKO::OSP_MOT_GRG_VCH_UPD;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_MOT_GRG_VCH_UPD::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void OSP_MOT_GRG_VCH_UPD::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    m_mfId = MFG_MOT;

    tsbkValue = m_siteData.channelId();                                             // Channel ID
    tsbkValue = (tsbkValue << 4) + m_siteData.channelNo();                          // Channel Number
    tsbkValue = (tsbkValue << 12) + m_patchGroup1Id;                                // Patch Group 1
    tsbkValue = (tsbkValue << 16) + m_siteData.channelId();                         // Channel ID
    tsbkValue = (tsbkValue << 4) + m_siteData.channelNo();                          // Channel Number
    tsbkValue = (tsbkValue << 12) + m_patchGroup2Id;                                // Patch Group 2

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string OSP_MOT_GRG_VCH_UPD::toString(bool isp)
{
    return std::string("TSBKO, OSP_MOT_GRG_VCH_UPD (Group Regroup Voice Channel Grant Update)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void OSP_MOT_GRG_VCH_UPD::copy(const OSP_MOT_GRG_VCH_UPD& data)
{
    TSBK::copy(data);

    m_patchGroup1Id = data.m_patchGroup1Id;
    m_patchGroup2Id = data.m_patchGroup2Id;
}
