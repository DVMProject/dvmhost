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
#include "p25/sndcp/SNDCPFactory.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::sndcp;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the SNDCPFactory class.
/// </summary>
SNDCPFactory::SNDCPFactory() = default;

/// <summary>
/// Finalizes a instance of SNDCPFactory class.
/// </summary>
SNDCPFactory::~SNDCPFactory() = default;

/// <summary>
/// Create an instance of a SNDCPPacket.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if packet was decoded, otherwise false.</returns>
std::unique_ptr<SNDCPPacket> SNDCPFactory::create(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t pduType = (uint8_t)((data[0U] >> 4) & 0x0FU);                           // SNDCP PDU Message Type

    switch (pduType) {
    case SNDCP_PDUType::ACT_TDS_CTX:
        return decode(new SNDCPCtxActRequest(), data);
        break;
    case SNDCP_PDUType::DEACT_TDS_CTX_REQ:
        return decode(new SNDCPCtxDeactivation(), data);
        break;
    case SNDCP_PDUType::RF_CONFIRMED:
        break;
    case SNDCP_PDUType::RF_UNCONFIRMED:
        break;
    default:
        LogError(LOG_P25, "SNDCPFactory::create(), unknown SNDCP PDU value, pduType = $%02X", pduType);
        break;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="tsbk"></param>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns></returns>
std::unique_ptr<SNDCPPacket> SNDCPFactory::decode(SNDCPPacket* packet, const uint8_t* data)
{
    assert(packet != nullptr);
    assert(data != nullptr);

    if (!packet->decode(data)) {
        return nullptr;
    }

    return std::unique_ptr<SNDCPPacket>(packet);
}
