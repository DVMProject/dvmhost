// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/lc/tsbk/OSP_SCCB_EXP.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the OSP_SCCB_EXP class.
/// </summary>
OSP_SCCB_EXP::OSP_SCCB_EXP() : TSBK(),
    m_sccbChannelId1(0U),
    m_sccbChannelNo(0U)
{
    m_lco = TSBK_OSP_SCCB_EXP;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_SCCB_EXP::decode(const uint8_t* data, bool rawTSBK)
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
void OSP_SCCB_EXP::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = m_siteData.rfssId();                                                // RF Sub-System ID
    tsbkValue = (tsbkValue << 8) + m_siteData.siteId();                             // Site ID

    tsbkValue = (tsbkValue << 4) + m_sccbChannelId1;                                // Channel (T) ID
    tsbkValue = (tsbkValue << 12) + m_sccbChannelNo;                                // Channel (T) Number
    tsbkValue = (tsbkValue << 12) + m_sccbChannelId1;                               // Channel (R) ID
    tsbkValue = (tsbkValue << 12) + m_sccbChannelNo;                                // Channel (R) Number

    if (m_sccbChannelId1 > 0) {
        tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();                   // System Service Class
    }
    else {
        tsbkValue = (tsbkValue << 8) + (P25_SVC_CLS_INVALID);                       // System Service Class
    }

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string OSP_SCCB_EXP::toString(bool isp)
{
    return std::string("TSBK_OSP_SCCB_EXP (Secondary Control Channel Broadcast - Explicit)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void OSP_SCCB_EXP::copy(const OSP_SCCB_EXP& data)
{
    TSBK::copy(data);

    m_sccbChannelId1 = data.m_sccbChannelId1;
    m_sccbChannelNo = data.m_sccbChannelNo;
}
