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
#include "dmr/lc/csbk/CSBK_RAND.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_RAND class. */
CSBK_RAND::CSBK_RAND() : CSBK(),
    m_serviceOptions(0U),
    m_serviceExtra(0U),
    m_serviceKind(0U)
{
    m_CSBKO = CSBKO::RAND;
}

/* Decode a control signalling block. */
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

/* Encode a control signalling block. */
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

/* Returns a string that represents the current CSBK. */
std::string CSBK_RAND::toString()
{
    switch (m_serviceKind) {
    case ServiceKind::IND_VOICE_CALL:       return std::string("CSBKO, RAND (Random Access), IND_VOICE_CALL (Individual Voice Call)");
    case ServiceKind::GRP_VOICE_CALL:       return std::string("CSBKO, RAND (Random Access), GRP_VOICE_CALL (Group Voice Call)");
    case ServiceKind::IND_DATA_CALL:        return std::string("CSBKO, RAND (Random Access), IND_DATA_CALL (Individual Data Call)");
    case ServiceKind::GRP_DATA_CALL:        return std::string("CSBKO, RAND (Random Access), GRP_DATA_CALL (Group Data Call)");
    case ServiceKind::IND_UDT_DATA_CALL:    return std::string("CSBKO, RAND (Random Access), IND_UDT_DATA_CALL (Individual UDT Short Data Call)");
    case ServiceKind::GRP_UDT_DATA_CALL:    return std::string("CSBKO, RAND (Random Access), GRP_UDT_DATA_CALL (Group UDT Short Data Call)");
    case ServiceKind::UDT_SHORT_POLL:       return std::string("CSBKO, RAND (Random Access), UDT_SHORT_POLL (UDT Short Data Polling Service)");
    case ServiceKind::STATUS_TRANSPORT:     return std::string("CSBKO, RAND (Random Access), STATUS_TRANSPORT (Status Transport Service)");
    case ServiceKind::CALL_DIVERSION:       return std::string("CSBKO, RAND (Random Access), CALL_DIVERSION (Call Diversion Service)");
    case ServiceKind::CALL_ANSWER:          return std::string("CSBKO, RAND (Random Access), CALL_ANSWER (Call Answer Service)");
    case ServiceKind::SUPPLEMENTARY_SVC:    return std::string("CSBKO, RAND (Random Access), SUPPLEMENTARY_SVC (Supplementary Service)");
    case ServiceKind::REG_SVC:              return std::string("CSBKO, RAND (Random Access), REG_SVC (Registration Service)");
    case ServiceKind::CANCEL_CALL:          return std::string("CSBKO, RAND (Random Access), CANCEL_CALL (Cancel Call Service)");
    default:                                return std::string("CSBKO, RAND (Random Access)");
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */
void CSBK_RAND::copy(const CSBK_RAND& data)
{
    CSBK::copy(data);

    m_serviceOptions = data.m_serviceOptions;
    m_serviceExtra = data.m_serviceExtra;
    m_serviceKind = data.m_serviceKind;
}
