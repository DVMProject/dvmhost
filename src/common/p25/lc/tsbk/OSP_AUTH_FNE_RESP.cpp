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
#include "p25/lc/tsbk/OSP_AUTH_FNE_RESP.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the OSP_AUTH_FNE_RESP class.
/// </summary>
OSP_AUTH_FNE_RESP::OSP_AUTH_FNE_RESP() : TSBK(),
    m_authRes(nullptr)
{
    m_lco = TSBK_OSP_AUTH_FNE_RESP;

    m_authRes = new uint8_t[P25_AUTH_RES_LENGTH_BYTES];
    ::memset(m_authRes, 0x00U, P25_AUTH_RES_LENGTH_BYTES);
}

/// <summary>
/// Finalizes a instance of OSP_AUTH_FNE_RESP class.
/// </summary>
OSP_AUTH_FNE_RESP::~OSP_AUTH_FNE_RESP()
{
    if (m_authRes != nullptr) {
        delete[] m_authRes;
        m_authRes = nullptr;
    }
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_AUTH_FNE_RESP::decode(const uint8_t* data, bool rawTSBK)
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
void OSP_AUTH_FNE_RESP::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = (tsbkValue << 8) + m_authRes[0U];                                   // Result b3
    tsbkValue = (tsbkValue << 8) + m_authRes[1U];                                   // Result b2
    tsbkValue = (tsbkValue << 8) + m_authRes[2U];                                   // Result b1
    tsbkValue = (tsbkValue << 8) + m_authRes[3U];                                   // Result b0
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string OSP_AUTH_FNE_RESP::toString(bool isp)
{
    return std::string("TSBK_OSP_AUTH_FNE_RESP (Authentication FNE Response)");
}

/// <summary>Sets the authentication result.</summary>
/// <param name="mi"></param>
void OSP_AUTH_FNE_RESP::setAuthRes(const uint8_t* res)
{
    assert(res != nullptr);

    if (m_authRes != nullptr) {
        delete[] m_authRes;
    }

    m_authRes = new uint8_t[P25_AUTH_RES_LENGTH_BYTES];
    ::memcpy(m_authRes, res, P25_AUTH_RES_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void OSP_AUTH_FNE_RESP::copy(const OSP_AUTH_FNE_RESP& data)
{
    TSBK::copy(data);

    if (m_authRes != nullptr) {
        delete[] m_authRes;
    }

    m_authRes = new uint8_t[P25_AUTH_RES_LENGTH_BYTES];
    ::memcpy(m_authRes, data.m_authRes, P25_AUTH_RES_LENGTH_BYTES);
}
