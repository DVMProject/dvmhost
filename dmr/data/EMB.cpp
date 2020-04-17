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
#include "dmr/data/EMB.h"
#include "edac/QR1676.h"

using namespace dmr::data;
using namespace dmr;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the EMB class.
/// </summary>
EMB::EMB() :
    m_colorCode(0U),
    m_PI(false),
    m_LCSS(0U)
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the EMB class.
/// </summary>
EMB::~EMB()
{
    /* stub */
}

/// <summary>
/// Decodes DMR embedded signalling data.
/// </summary>
/// <param name="data"></param>
void EMB::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint8_t DMREMB[2U];
    DMREMB[0U] = (data[13U] << 4) & 0xF0U;
    DMREMB[0U] |= (data[14U] >> 4) & 0x0FU;
    DMREMB[1U] = (data[18U] << 4) & 0xF0U;
    DMREMB[1U] |= (data[19U] >> 4) & 0x0FU;

    // decode QR (16,7,6) FEC
    edac::QR1676::decode(DMREMB);

    m_colorCode = (DMREMB[0U] >> 4) & 0x0FU;
    m_PI = (DMREMB[0U] & 0x08U) == 0x08U;
    m_LCSS = (DMREMB[0U] >> 1) & 0x03U;
}

/// <summary>
/// Encodes DMR embedded signalling data.
/// </summary>
/// <param name="data"></param>
void EMB::encode(uint8_t* data) const
{
    assert(data != NULL);

    uint8_t DMREMB[2U];
    DMREMB[0U] = (m_colorCode << 4) & 0xF0U;
    DMREMB[0U] |= m_PI ? 0x08U : 0x00U;
    DMREMB[0U] |= (m_LCSS << 1) & 0x06U;
    DMREMB[1U] = 0x00U;

    // encode QR (16,7,6) FEC
    edac::QR1676::encode(DMREMB);

    data[13U] = (data[13U] & 0xF0U) | ((DMREMB[0U] >> 4U) & 0x0FU);
    data[14U] = (data[14U] & 0x0FU) | ((DMREMB[0U] << 4U) & 0xF0U);
    data[18U] = (data[18U] & 0xF0U) | ((DMREMB[1U] >> 4U) & 0x0FU);
    data[19U] = (data[19U] & 0x0FU) | ((DMREMB[1U] << 4U) & 0xF0U);
}
