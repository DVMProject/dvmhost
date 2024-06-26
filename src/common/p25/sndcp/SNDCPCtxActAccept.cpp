// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/sndcp/SNDCPCtxActAccept.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::sndcp;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the SNDCPCtxActAccept class.
/// </summary>
SNDCPCtxActAccept::SNDCPCtxActAccept() : SNDCPPacket(),
    m_priority(4U),
    m_readyTimer(SNDCPReadyTimer::TEN_SECONDS),
    m_standbyTimer(SNDCPStandbyTimer::ONE_MINUTE),
    m_nat(SNDCPNAT::IPV4_STATIC_ADDR),
    m_ipAddress(0U),
    m_mtu(SNDCP_MTU_510)
{
    m_pduType = SNDCP_PDUType::ACT_TDS_CTX;
}

/// <summary>
/// Decode a SNDCP context activation response.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if packet was decoded, otherwise false.</returns>
bool SNDCPCtxActAccept::decode(const uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::decodeHeader(data, false);

    m_priority = (uint8_t)((data[1U] >> 4) & 0x0FU);                                // Priority
    m_readyTimer = (uint8_t)(data[1U] & 0x0FU);                                     // Ready Timer
    m_standbyTimer = (uint8_t)((data[2U] >> 4) & 0x0FU);                            // Standby Timer
    m_nat = (uint8_t)(data[2U] & 0x0FU);                                            // NAT

    m_ipAddress = 0U;                                                               // IP Address
    m_ipAddress = data[3U];
    m_ipAddress = (m_ipAddress << 8) + data[4U];
    m_ipAddress = (m_ipAddress << 8) + data[5U];
    m_ipAddress = (m_ipAddress << 8) + data[6U];

    m_mtu = (uint8_t)((data[9U] >> 4) & 0x0FU);                                     // MTU

    return true;
}

/// <summary>
/// Encode a SNDCP context activation response.
/// </summary>
/// <param name="data"></param>
void SNDCPCtxActAccept::encode(uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::encodeHeader(data, true);

    data[1U] = ((m_priority << 4U) & 0xF0U) +                                       // Priority
        (m_readyTimer & 0x0FU);                                                     // Ready Timer
    data[2U] = ((m_standbyTimer << 4U) & 0xF0U) +                                   // Standby Timer
        (m_nat & 0x0FU);                                                            // NAT

    data[3U] = (uint8_t)((m_ipAddress >> 24) & 0xFFU);                              // IP Address
    data[4U] = (uint8_t)((m_ipAddress >> 16) & 0xFFU);
    data[5U] = (uint8_t)((m_ipAddress >> 8) & 0xFFU);
    data[6U] = (uint8_t)((m_ipAddress >> 0) & 0xFFU);

    data[9U] = ((m_mtu << 4U) & 0xF0U);                                             // MTU
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void SNDCPCtxActAccept::copy(const SNDCPCtxActAccept& data)
{
    m_priority = data.m_priority;
    m_readyTimer = data.m_readyTimer;
    m_standbyTimer = data.m_standbyTimer;
    m_nat = data.m_nat;

    m_ipAddress = data.m_ipAddress;

    m_mtu = data.m_mtu;
}
