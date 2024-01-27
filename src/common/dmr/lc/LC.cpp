/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2020-2021 by Bryan Biedenkapp N2PLL
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
#include "dmr/DMRDefines.h"
#include "dmr/lc/LC.h"
#include "Utils.h"

using namespace dmr::lc;
using namespace dmr;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
/// <param name="flco">Full-link Control Opcode.</param>
/// <param name="srcId">Source ID.</param>
/// <param name="dstId">Destination ID.</param>
LC::LC(uint8_t flco, uint32_t srcId, uint32_t dstId) :
    m_PF(false),
    m_FLCO(flco),
    m_FID(FID_ETSI),
    m_srcId(srcId),
    m_dstId(dstId),
    m_emergency(false),
    m_encrypted(false),
    m_broadcast(false),
    m_ovcm(false),
    m_priority(CALL_PRIORITY_2),
    m_R(false)
{
    /* stub */
}
/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
/// <param name="data"></param>
LC::LC(const uint8_t* data) :
    m_PF(false),
    m_FLCO(FLCO_GROUP),
    m_FID(FID_ETSI),
    m_srcId(0U),
    m_dstId(0U),
    m_emergency(false),
    m_encrypted(false),
    m_broadcast(false),
    m_ovcm(false),
    m_priority(CALL_PRIORITY_2),
    m_R(false)
{
    assert(data != nullptr);

    m_PF = (data[0U] & 0x80U) == 0x80U;
    m_R = (data[0U] & 0x40U) == 0x40U;

    m_FLCO = data[0U] & 0x3FU;

    m_FID = data[1U];

    m_emergency = (data[2U] & 0x80U) == 0x80U;                                  // Emergency Flag
    m_encrypted = (data[2U] & 0x40U) == 0x40U;                                  // Encryption Flag
    m_broadcast = (data[2U] & 0x08U) == 0x08U;                                  // Broadcast Flag
    m_ovcm = (data[2U] & 0x04U) == 0x04U;                                       // OVCM Flag
    m_priority = (data[2U] & 0x03U);                                            // Priority

    m_dstId = data[3U] << 16 | data[4U] << 8 | data[5U];                        // Destination Address
    m_srcId = data[6U] << 16 | data[7U] << 8 | data[8U];                        // Source Address
}
/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
/// <param name="bits"></param>
LC::LC(const bool* bits) :
    m_PF(false),
    m_FLCO(FLCO_GROUP),
    m_FID(FID_ETSI),
    m_srcId(0U),
    m_dstId(0U),
    m_emergency(false),
    m_encrypted(false),
    m_broadcast(false),
    m_ovcm(false),
    m_priority(CALL_PRIORITY_2),
    m_R(false)
{
    assert(bits != nullptr);

    m_PF = bits[0U];
    m_R = bits[1U];

    uint8_t temp1, temp2, temp3;
    Utils::bitsToByteBE(bits + 0U, temp1);
    m_FLCO = temp1 & 0x3FU;

    Utils::bitsToByteBE(bits + 8U, temp2);
    m_FID = temp2;

    Utils::bitsToByteBE(bits + 16U, temp3);

    m_emergency = (temp3 & 0x80U) == 0x80U;                                     // Emergency Flag
    m_encrypted = (temp3 & 0x40U) == 0x40U;                                     // Encryption Flag
    m_broadcast = (temp3 & 0x08U) == 0x08U;                                     // Broadcast Flag
    m_ovcm = (temp3 & 0x04U) == 0x04U;                                          // OVCM Flag
    m_priority = (temp3 & 0x03U);                                               // Priority

    uint8_t d1, d2, d3;
    Utils::bitsToByteBE(bits + 24U, d1);
    Utils::bitsToByteBE(bits + 32U, d2);
    Utils::bitsToByteBE(bits + 40U, d3);

    uint8_t s1, s2, s3;
    Utils::bitsToByteBE(bits + 48U, s1);
    Utils::bitsToByteBE(bits + 56U, s2);
    Utils::bitsToByteBE(bits + 64U, s3);

    m_srcId = s1 << 16 | s2 << 8 | s3;                                          // Source Address
    m_dstId = d1 << 16 | d2 << 8 | d3;                                          // Destination Address
}
/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
LC::LC() :
    m_PF(false),
    m_FLCO(FLCO_GROUP),
    m_FID(FID_ETSI),
    m_srcId(0U),
    m_dstId(0U),
    m_emergency(false),
    m_encrypted(false),
    m_broadcast(false),
    m_ovcm(false),
    m_priority(CALL_PRIORITY_2),
    m_R(false)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the LC class.
/// </summary>
LC::~LC() = default;

/// <summary>
///
/// </summary>
/// <param name="data"></param>
void LC::getData(uint8_t* data) const
{
    assert(data != nullptr);

    data[0U] = (uint8_t)m_FLCO;

    if (m_PF)
        data[0U] |= 0x80U;

    if (m_R)
        data[0U] |= 0x40U;

    data[1U] = m_FID;

    data[2U] = (m_emergency ? 0x80U : 0x00U) +                                  // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
        (m_broadcast ? 0x08U : 0x00U) +                                         // Broadcast Flag
        (m_ovcm ? 0x04U : 0x00U) +                                              // OVCM Flag
        (m_priority & 0x03U);                                                   // Priority

    data[3U] = m_dstId >> 16;                                                   // Destination Address
    data[4U] = m_dstId >> 8;                                                    // ..
    data[5U] = m_dstId >> 0;                                                    // ..

    data[6U] = m_srcId >> 16;                                                   // Source Address
    data[7U] = m_srcId >> 8;                                                    // ..
    data[8U] = m_srcId >> 0;                                                    // ..
}

/// <summary>
///
/// </summary>
/// <param name="bits"></param>
void LC::getData(bool* bits) const
{
    assert(bits != nullptr);

    uint8_t bytes[9U];
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
}
