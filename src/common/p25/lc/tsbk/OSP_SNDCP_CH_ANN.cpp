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
#include "p25/lc/tsbk/OSP_SNDCP_CH_ANN.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the OSP_SNDCP_CH_ANN class.
/// </summary>
OSP_SNDCP_CH_ANN::OSP_SNDCP_CH_ANN() : TSBK(),
    m_sndcpAutoAccess(true),
    m_sndcpDAC(1U)
{
    m_lco = TSBKO::OSP_SNDCP_CH_ANN;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_SNDCP_CH_ANN::decode(const uint8_t* data, bool rawTSBK)
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
void OSP_SNDCP_CH_ANN::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    uint32_t calcSpace = (uint32_t)(m_siteIdenEntry.chSpaceKhz() / 0.125);
    float calcTxOffset = m_siteIdenEntry.txOffsetMhz() * 1000000;

    uint32_t txFrequency = (uint32_t)((m_siteIdenEntry.baseFrequency() + ((calcSpace * 125) * m_siteData.channelNo())));
    uint32_t rxFrequency = (uint32_t)(txFrequency + calcTxOffset);

    uint32_t rootFreq = rxFrequency - m_siteIdenEntry.baseFrequency();
    uint32_t rxChNo = rootFreq / (m_siteIdenEntry.chSpaceKhz() * 1000);

    tsbkValue = 0U;                                                                 //
    tsbkValue = (m_emergency ? 0x80U : 0x00U) +                                     // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U);                                              // Encrypted Flag
    tsbkValue = (tsbkValue << 8) +
        (m_sndcpAutoAccess ? 0x80U : 0x00U) +                                       // Autonomous Access
        (m_sndcpAutoAccess ? 0x40U : 0x00U);                                        // Requested Access
    tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                          // Channel (T) ID
    tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                         // Channel (T) Number
    tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                          // Channel (R) ID
    tsbkValue = (tsbkValue << 12) + (rxChNo & 0xFFFU);                              // Channel (R) Number
    tsbkValue = (tsbkValue << 16) + m_sndcpDAC;                                     // Data Access Control

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string OSP_SNDCP_CH_ANN::toString(bool isp)
{
    return std::string("TSBKO, OSP_SNDCP_CH_ANN (SNDCP Data Channel Announcement)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void OSP_SNDCP_CH_ANN::copy(const OSP_SNDCP_CH_ANN& data)
{
    TSBK::copy(data);

    m_sndcpAutoAccess = data.m_sndcpAutoAccess;
    m_sndcpDAC = data.m_sndcpDAC;
}
