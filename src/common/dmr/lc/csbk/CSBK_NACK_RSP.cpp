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
#include "dmr/lc/csbk/CSBK_NACK_RSP.h"

using namespace dmr::lc::csbk;
using namespace dmr::lc;
using namespace dmr;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK_NACK_RSP class.
/// </summary>
CSBK_NACK_RSP::CSBK_NACK_RSP() : CSBK(),
    m_serviceKind(0U)
{
    m_CSBKO = CSBKO_NACK_RSP;
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_NACK_RSP::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_GI = (((csbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                          // Group/Individual Flag
    m_serviceKind = (((csbkValue >> 56) & 0xFFU) & 0x3FU);                          // Service Kind
    m_reason = (uint8_t)((csbkValue >> 48) & 0xFFU);                                // Reason Code
    m_srcId = (uint32_t)((csbkValue >> 24) & 0xFFFFFFU);                            // Source Radio Address
    m_dstId = (uint32_t)(csbkValue & 0xFFFFFFU);                                    // Target Radio Address

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_NACK_RSP::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = 0x80U +                                                             // Additional Information Field (always 1)
        (m_GI ? 0x40U : 0x00U) +                                                    // Source Type
        (m_serviceKind & 0x3FU);                                                    // Service Kind
    csbkValue = (csbkValue << 8) + m_reason;                                        // Reason Code
    csbkValue = (csbkValue << 24) + m_srcId;                                        // Source Radio Address
    csbkValue = (csbkValue << 24) + m_dstId;                                        // Target Radio Address

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/// <summary>
/// Returns a string that represents the current CSBK.
/// </summary>
/// <returns></returns>
std::string CSBK_NACK_RSP::toString()
{
    return std::string("CSBKO_NACK_RSP (Negative Acknowledgement Response)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void CSBK_NACK_RSP::copy(const CSBK_NACK_RSP& data)
{
    CSBK::copy(data);

    m_serviceKind = data.m_serviceKind;
}
