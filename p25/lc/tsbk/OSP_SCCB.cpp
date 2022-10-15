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
#include "p25/lc/tsbk/OSP_SCCB.h"
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
/// Initializes a new instance of the OSP_SCCB class.
/// </summary>
OSP_SCCB::OSP_SCCB() : TSBK(),
    m_sccbChannelId1(0U),
    m_sccbChannelId2(0U)
{
    m_lco = TSBK_OSP_SCCB;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool OSP_SCCB::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

    /* stub */

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void OSP_SCCB::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    ulong64_t tsbkValue = 0U;

    tsbkValue = m_siteData.rfssId();                                                // RF Sub-System ID
    tsbkValue = (tsbkValue << 8) + m_siteData.siteId();                             // Site ID
    tsbkValue = (tsbkValue << 16) + m_sccbChannelId1;                               // SCCB Channel ID 1
    if (m_sccbChannelId1 > 0) {
        tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();                   // System Service Class
    }
    else {
        tsbkValue = (tsbkValue << 8) + (P25_SVC_CLS_INVALID);                       // System Service Class
    }
    tsbkValue = (tsbkValue << 16) + m_sccbChannelId2;                               // SCCB Channel ID 2
    if (m_sccbChannelId2 > 0) {
        tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();                   // System Service Class
    }
    else {
        tsbkValue = (tsbkValue << 8) + (P25_SVC_CLS_INVALID);                       // System Service Class
    }

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void OSP_SCCB::copy(const OSP_SCCB& data)
{
    TSBK::copy(data);

    m_sccbChannelId1 = data.m_sccbChannelId1;
    m_sccbChannelId2 = data.m_sccbChannelId2;
}
