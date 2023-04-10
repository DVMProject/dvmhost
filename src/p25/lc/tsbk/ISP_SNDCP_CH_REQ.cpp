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
#include "p25/lc/tsbk/ISP_SNDCP_CH_REQ.h"
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
/// Initializes a new instance of the ISP_SNDCP_CH_REQ class.
/// </summary>
ISP_SNDCP_CH_REQ::ISP_SNDCP_CH_REQ() : TSBK(),
    m_dataServiceOptions(0U),
    m_dataAccessControl(0U)
{
    m_lco = TSBK_ISP_SNDCP_CH_REQ;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool ISP_SNDCP_CH_REQ::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_dataServiceOptions = (uint8_t)((tsbkValue >> 56) & 0xFFU);                    // Data Service Options
    m_dataAccessControl = (uint32_t)((tsbkValue >> 40) & 0xFFFFFFFFU);              // Data Access Control
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void ISP_SNDCP_CH_REQ::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    /* stub */
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void ISP_SNDCP_CH_REQ::copy(const ISP_SNDCP_CH_REQ& data)
{
    TSBK::copy(data);

    m_dataServiceOptions = data.m_dataServiceOptions;
    m_dataAccessControl = data.m_dataAccessControl;
}
