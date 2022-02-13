/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2021 by Bryan Biedenkapp N2PLL
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
#include "dmr/lc/PrivacyLC.h"
#include "edac/CRC.h"
#include "Utils.h"

using namespace dmr::lc;
using namespace dmr;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the PrivacyLC class.
/// </summary>
/// <param name="bytes"></param>
PrivacyLC::PrivacyLC(const uint8_t* bytes) :
    m_FID(FID_ETSI),
    m_dstId(0U),
    m_group(false),
    m_algId(0U),
    m_kId(0U),
    m_mi(NULL)
{
    assert(bytes != NULL);

    m_mi = new uint8_t[DMR_MI_LENGTH_BYTES];
    
    m_group = (bytes[0U] & 0x20U) == 0x20U;
    m_algId = bytes[0U] & 7;                                                    // Algorithm ID

    m_FID = bytes[1U];
    m_kId = bytes[2U];

    m_mi[0U] = bytes[3U];
    m_mi[1U] = bytes[4U];
    m_mi[2U] = bytes[5U];
    m_mi[3U] = bytes[6U];

    m_dstId = bytes[7U] << 16 | bytes[8U] << 8 | bytes[9U];                     // Destination Address
}
/// <summary>
/// Initializes a new instance of the PrivacyLC class.
/// </summary>
/// <param name="bits"></param>
PrivacyLC::PrivacyLC(const bool* bits) :
    m_FID(FID_ETSI),
    m_dstId(0U),
    m_group(false),
    m_algId(0U),
    m_kId(0U),
    m_mi(NULL)
{
    assert(bits != NULL);

    m_mi = new uint8_t[DMR_MI_LENGTH_BYTES];

    uint8_t temp1, temp2, temp3;
    Utils::bitsToByteBE(bits + 0U, temp1);

    m_group = (temp1 & 0x20U) == 0x20U;
    m_algId = temp1 & 7;                                                        // Algorithm ID

    Utils::bitsToByteBE(bits + 8U, temp2);
    m_FID = temp2;

    Utils::bitsToByteBE(bits + 16U, temp3);
    m_kId = temp3;

    uint8_t mi1, mi2, mi3, mi4;
    Utils::bitsToByteBE(bits + 24U, mi1);
    Utils::bitsToByteBE(bits + 32U, mi2);
    Utils::bitsToByteBE(bits + 40U, mi3);
    Utils::bitsToByteBE(bits + 48U, mi4);

    m_mi[0U] = mi1;
    m_mi[1U] = mi2;
    m_mi[2U] = mi3;
    m_mi[3U] = mi4;

    uint8_t d1, d2, d3;
    Utils::bitsToByteBE(bits + 56U, d1);
    Utils::bitsToByteBE(bits + 64U, d2);
    Utils::bitsToByteBE(bits + 72U, d3);

    m_dstId = d1 << 16 | d2 << 8 | d3;                                          // Destination Address
}
/// <summary>
/// Initializes a new instance of the PrivacyLC class.
/// </summary>
PrivacyLC::PrivacyLC() :
    m_FID(FID_ETSI),
    m_dstId(0U),
    m_group(false),
    m_algId(0U),
    m_kId(0U),
    m_mi(NULL)
{
    m_mi = new uint8_t[DMR_MI_LENGTH_BYTES];
}

/// <summary>
/// Finalizes a instance of the PrivacyLC class.
/// </summary>
PrivacyLC::~PrivacyLC()
{
    delete m_mi;
}

/// <summary>
///
/// </summary>
/// <param name="bytes"></param>
void PrivacyLC::getData(uint8_t* bytes) const
{
    assert(bytes != NULL);

    bytes[0U] = (m_group ? 0x20U : 0x00U) +
        (m_algId & 0x07U);                                                      // Algorithm ID

    bytes[1U] = m_FID;
    bytes[2U] = m_kId;

    bytes[3U] = m_mi[0U];
    bytes[4U] = m_mi[1U];
    bytes[5U] = m_mi[2U];
    bytes[6U] = m_mi[3U];

    bytes[7U] = m_dstId >> 16;                                                  // Destination Address
    bytes[8U] = m_dstId >> 8;                                                   // ..
    bytes[9U] = m_dstId >> 0;                                                   // ..
}

/// <summary>
///
/// </summary>
/// <param name="bits"></param>
void PrivacyLC::getData(bool* bits) const
{
    assert(bits != NULL);

    uint8_t bytes[10U];
    getData(bytes);

    Utils::byteToBitsBE(bytes[0U], bits + 0U);
    Utils::byteToBitsBE(bytes[1U], bits + 8U);
    Utils::byteToBitsBE(bytes[2U], bits + 16U);
    Utils::byteToBitsBE(bytes[3U], bits + 24U);
    Utils::byteToBitsBE(bytes[4U], bits + 32U);
    Utils::byteToBitsBE(bytes[5U], bits + 40U);
    Utils::byteToBitsBE(bytes[6U], bits + 48U);
    Utils::byteToBitsBE(bytes[7U], bits + 56U);
    Utils::byteToBitsBE(bytes[8U], bits + 64U);
    Utils::byteToBitsBE(bytes[9U], bits + 72U);
}
