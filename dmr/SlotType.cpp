/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
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
#include "dmr/SlotType.h"
#include "edac/Golay2087.h"

using namespace dmr;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the SlotType class.
/// </summary>
SlotType::SlotType() :
    m_colorCode(0U),
    m_dataType(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the SlotType class.
/// </summary>
SlotType::~SlotType()
{
    /* stub */
}

/// <summary>
/// Decodes DMR slot type.
/// </summary>
/// <param name="data"></param>
void SlotType::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint8_t DMRSlotType[3U];
    DMRSlotType[0U] = (data[12U] << 2) & 0xFCU;
    DMRSlotType[0U] |= (data[13U] >> 6) & 0x03U;

    DMRSlotType[1U] = (data[13U] << 2) & 0xC0U;
    DMRSlotType[1U] |= (data[19U] << 2) & 0x3CU;
    DMRSlotType[1U] |= (data[20U] >> 6) & 0x03U;

    DMRSlotType[2U] = (data[20U] << 2) & 0xF0U;

    uint8_t code = edac::Golay2087::decode(DMRSlotType);

    m_colorCode = (code >> 4) & 0x0FU;
    m_dataType = (code >> 0) & 0x0FU;
}

/// <summary>
/// Encodes DMR slot type.
/// </summary>
/// <param name="data"></param>
void SlotType::encode(uint8_t* data) const
{
    assert(data != NULL);

    uint8_t DMRSlotType[3U];
    DMRSlotType[0U] = (m_colorCode << 4) & 0xF0U;
    DMRSlotType[0U] |= (m_dataType << 0) & 0x0FU;
    DMRSlotType[1U] = 0x00U;
    DMRSlotType[2U] = 0x00U;

    edac::Golay2087::encode(DMRSlotType);

    data[12U] = (data[12U] & 0xC0U) | ((DMRSlotType[0U] >> 2) & 0x3FU);
    data[13U] = (data[13U] & 0x0FU) | ((DMRSlotType[0U] << 6) & 0xC0U) | ((DMRSlotType[1U] >> 2) & 0x30U);
    data[19U] = (data[19U] & 0xF0U) | ((DMRSlotType[1U] >> 2) & 0x0FU);
    data[20U] = (data[20U] & 0x03U) | ((DMRSlotType[1U] << 6) & 0xC0U) | ((DMRSlotType[2U] >> 2) & 0x3CU);
}
