/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "p25/lc/tsbk/ISP_AUTH_RESP.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the ISP_AUTH_RESP class.
/// </summary>
ISP_AUTH_RESP::ISP_AUTH_RESP() : TSBK(),
    m_authStandalone(false),
    m_authRes(NULL)
{
    m_lco = TSBK_ISP_AUTH_RESP;

    m_authRes = new uint8_t[P25_AUTH_RES_LENGTH_BYTES];
    ::memset(m_authRes, 0x00U, P25_AUTH_RES_LENGTH_BYTES);
}

/// <summary>
/// Finalizes a instance of ISP_AUTH_RESP class.
/// </summary>
ISP_AUTH_RESP::~ISP_AUTH_RESP()
{
    if (m_authRes != NULL) {
        delete[] m_authRes;
        m_authRes = NULL;
    }
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool ISP_AUTH_RESP::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                     // Additional Info. Flag
    m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                               // Service Type
    m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                              // Reason
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                            // Target Radio Address
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void ISP_AUTH_RESP::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    /* stub */
}

/// <summary>Gets the authentication result.</summary>
/// <returns></returns>
void ISP_AUTH_RESP::getAuthRes(uint8_t* res) const
{
    assert(res != NULL);

    ::memcpy(res, m_authRes, P25_AUTH_RES_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void ISP_AUTH_RESP::copy(const ISP_AUTH_RESP& data)
{
    TSBK::copy(data);

    m_authStandalone = data.m_authStandalone;

    if (m_authRes != NULL) {
        delete[] m_authRes;
    }

    m_authRes = new uint8_t[P25_AUTH_RES_LENGTH_BYTES];
    ::memcpy(m_authRes, data.m_authRes, P25_AUTH_RES_LENGTH_BYTES);
}
