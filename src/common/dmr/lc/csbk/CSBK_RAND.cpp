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
#include "dmr/lc/csbk/CSBK_RAND.h"

using namespace dmr::lc::csbk;
using namespace dmr::lc;
using namespace dmr;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK_RAND class.
/// </summary>
CSBK_RAND::CSBK_RAND() : CSBK(),
    m_serviceOptions(0U),
    m_serviceExtra(0U),
    m_serviceKind(0U)
{
    m_CSBKO = CSBKO_RAND;
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_RAND::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_serviceOptions = (uint8_t)((csbkValue >> 57U) & 0x7FU);                       // Service Options
    m_proxy = (((csbkValue >> 56U) & 0xFF) & 0x01U) == 0x01U;                       // Proxy Flag
    m_serviceExtra = (uint8_t)((csbkValue >> 52U) & 0x0FU);                         // Service Extras (content dependant on service)
    m_serviceKind = (uint8_t)((csbkValue >> 48U) & 0x0FU);                          // Service Kind
    m_dstId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                            // Target Radio Address
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_RAND::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = m_serviceOptions & 0x7FU;                                           // Service Options
    csbkValue = (csbkValue << 1) + (m_proxy ? 0x01U : 0x00);                        // Proxy Flag
    csbkValue = (csbkValue << 4) + (m_serviceExtra & 0x0FU);                        // Service Extras
    csbkValue = (csbkValue << 4) + (m_serviceKind & 0x0FU);                         // Service Kind
    csbkValue = (csbkValue << 24) + m_dstId;                                        // Target Radio Address
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/// <summary>
/// Returns a string that represents the current CSBK.
/// </summary>
/// <returns></returns>
std::string CSBK_RAND::toString()
{
    switch (m_serviceKind) {
    case SVC_KIND_IND_VOICE_CALL:       return std::string("CSBKO_RAND (Random Access), SVC_KIND_IND_VOICE_CALL (Individual Voice Call)");
    case SVC_KIND_GRP_VOICE_CALL:       return std::string("CSBKO_RAND (Random Access), SVC_KIND_GRP_VOICE_CALL (Group Voice Call)");
    case SVC_KIND_IND_DATA_CALL:        return std::string("CSBKO_RAND (Random Access), SVC_KIND_IND_DATA_CALL (Individual Data Call)");
    case SVC_KIND_GRP_DATA_CALL:        return std::string("CSBKO_RAND (Random Access), SVC_KIND_GRP_DATA_CALL (Group Data Call)");
    case SVC_KIND_IND_UDT_DATA_CALL:    return std::string("CSBKO_RAND (Random Access), SVC_KIND_IND_UDT_DATA_CALL (Individual UDT Short Data Call)");
    case SVC_KIND_GRP_UDT_DATA_CALL:    return std::string("CSBKO_RAND (Random Access), SVC_KIND_GRP_UDT_DATA_CALL (Group UDT Short Data Call)");
    case SVC_KIND_UDT_SHORT_POLL:       return std::string("CSBKO_RAND (Random Access), SVC_KIND_UDT_SHORT_POLL (UDT Short Data Polling Service)");
    case SVC_KIND_STATUS_TRANSPORT:     return std::string("CSBKO_RAND (Random Access), SVC_KIND_STATUS_TRANSPORT (Status Transport Service)");
    case SVC_KIND_CALL_DIVERSION:       return std::string("CSBKO_RAND (Random Access), SVC_KIND_CALL_DIVERSION (Call Diversion Service)");
    case SVC_KIND_CALL_ANSWER:          return std::string("CSBKO_RAND (Random Access), SVC_KIND_CALL_ANSWER (Call Answer Service)");
    case SVC_KIND_SUPPLEMENTARY_SVC:    return std::string("CSBKO_RAND (Random Access), SVC_KIND_SUPPLEMENTARY_SVC (Supplementary Service)");
    case SVC_KIND_REG_SVC:              return std::string("CSBKO_RAND (Random Access), SVC_KIND_REG_SVC (Registration Service)");
    case SVC_KIND_CANCEL_CALL:          return std::string("CSBKO_RAND (Random Access), SVC_KIND_CANCEL_CALL (Cancel Call Service)");
    default:                            return std::string("CSBKO_RAND (Random Access)");
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void CSBK_RAND::copy(const CSBK_RAND& data)
{
    CSBK::copy(data);

    m_serviceOptions = data.m_serviceOptions;
    m_serviceExtra = data.m_serviceExtra;
    m_serviceKind = data.m_serviceKind;
}
