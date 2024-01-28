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
#include "p25/lc/tsbk/IOSP_UU_ANS.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the IOSP_UU_ANS class.
/// </summary>
IOSP_UU_ANS::IOSP_UU_ANS() : TSBK()
{
    m_lco = TSBK_IOSP_UU_ANS;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool IOSP_UU_ANS::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_emergency = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                   // Emergency Flag
    m_encrypted = (((tsbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;                   // Encryption Flag
    m_priority = (((tsbkValue >> 56) & 0xFFU) & 0x07U);                             // Priority
    m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                              // Answer Response
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                            // Target ID
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void IOSP_UU_ANS::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue =
        (m_emergency ? 0x80U : 0x00U) +                                             // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                             // Encrypted Flag
        (m_priority & 0x07U);                                                       // Priority
    tsbkValue = (tsbkValue << 32) + m_dstId;                                        // Target ID
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string IOSP_UU_ANS::toString(bool isp)
{
    if (isp) return std::string("TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Response)");
    else return std::string("TSBK_IOSP_UU_ANS (Unit-to-Unit Answer Request)");
}
