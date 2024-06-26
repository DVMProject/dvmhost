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
#include "p25/sndcp/SNDCPCtxDeactivation.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::sndcp;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the SNDCPCtxDeactivation class.
/// </summary>
SNDCPCtxDeactivation::SNDCPCtxDeactivation() : SNDCPPacket(),
    m_deactType(SNDCPDeactivationType::DEACT_ALL)
{
    m_pduType = SNDCP_PDUType::DEACT_TDS_CTX_REQ;
}

/// <summary>
/// Decode a SNDCP context deactivation packet.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if packet was decoded, otherwise false.</returns>
bool SNDCPCtxDeactivation::decode(const uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::decodeHeader(data, false);

    m_deactType = data[1U];                                                         // Deactivation Type

    return true;
}

/// <summary>
/// Encode a SNDCP context deactivation packet.
/// </summary>
/// <param name="data"></param>
void SNDCPCtxDeactivation::encode(uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::encodeHeader(data, true);

    data[1U] = m_deactType;                                                         // Deactivation Type
}

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void SNDCPCtxDeactivation::copy(const SNDCPCtxDeactivation& data)
{
    m_deactType = data.m_deactType;
}
