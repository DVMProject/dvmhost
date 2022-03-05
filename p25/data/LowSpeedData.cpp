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
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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
#include "p25/data/LowSpeedData.h"
#include "p25/P25Utils.h"

using namespace p25::data;
using namespace p25;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t CCS_PARITY[] = {
    0x00U, 0x39U, 0x72U, 0x4BU, 0xE4U, 0xDDU, 0x96U, 0xAFU, 0xF1U, 0xC8U, 0x83U, 0xBAU, 0x15U, 0x2CU, 0x67U, 0x5EU,
    0xDBU, 0xE2U, 0xA9U, 0x90U, 0x3FU, 0x06U, 0x4DU, 0x74U, 0x2AU, 0x13U, 0x58U, 0x61U, 0xCEU, 0xF7U, 0xBCU, 0x85U,
    0x8FU, 0xB6U, 0xFDU, 0xC4U, 0x6BU, 0x52U, 0x19U, 0x20U, 0x7EU, 0x47U, 0x0CU, 0x35U, 0x9AU, 0xA3U, 0xE8U, 0xD1U,
    0x54U, 0x6DU, 0x26U, 0x1FU, 0xB0U, 0x89U, 0xC2U, 0xFBU, 0xA5U, 0x9CU, 0xD7U, 0xEEU, 0x41U, 0x78U, 0x33U, 0x0AU,
    0x27U, 0x1EU, 0x55U, 0x6CU, 0xC3U, 0xFAU, 0xB1U, 0x88U, 0xD6U, 0xEFU, 0xA4U, 0x9DU, 0x32U, 0x0BU, 0x40U, 0x79U,
    0xFCU, 0xC5U, 0x8EU, 0xB7U, 0x18U, 0x21U, 0x6AU, 0x53U, 0x0DU, 0x34U, 0x7FU, 0x46U, 0xE9U, 0xD0U, 0x9BU, 0xA2U,
    0xA8U, 0x91U, 0xDAU, 0xE3U, 0x4CU, 0x75U, 0x3EU, 0x07U, 0x59U, 0x60U, 0x2BU, 0x12U, 0xBDU, 0x84U, 0xCFU, 0xF6U,
    0x73U, 0x4AU, 0x01U, 0x38U, 0x97U, 0xAEU, 0xE5U, 0xDCU, 0x82U, 0xBBU, 0xF0U, 0xC9U, 0x66U, 0x5FU, 0x14U, 0x2DU,
    0x4EU, 0x77U, 0x3CU, 0x05U, 0xAAU, 0x93U, 0xD8U, 0xE1U, 0xBFU, 0x86U, 0xCDU, 0xF4U, 0x5BU, 0x62U, 0x29U, 0x10U,
    0x95U, 0xACU, 0xE7U, 0xDEU, 0x71U, 0x48U, 0x03U, 0x3AU, 0x64U, 0x5DU, 0x16U, 0x2FU, 0x80U, 0xB9U, 0xF2U, 0xCBU,
    0xC1U, 0xF8U, 0xB3U, 0x8AU, 0x25U, 0x1CU, 0x57U, 0x6EU, 0x30U, 0x09U, 0x42U, 0x7BU, 0xD4U, 0xEDU, 0xA6U, 0x9FU,
    0x1AU, 0x23U, 0x68U, 0x51U, 0xFEU, 0xC7U, 0x8CU, 0xB5U, 0xEBU, 0xD2U, 0x99U, 0xA0U, 0x0FU, 0x36U, 0x7DU, 0x44U,
    0x69U, 0x50U, 0x1BU, 0x22U, 0x8DU, 0xB4U, 0xFFU, 0xC6U, 0x98U, 0xA1U, 0xEAU, 0xD3U, 0x7CU, 0x45U, 0x0EU, 0x37U,
    0xB2U, 0x8BU, 0xC0U, 0xF9U, 0x56U, 0x6FU, 0x24U, 0x1DU, 0x43U, 0x7AU, 0x31U, 0x08U, 0xA7U, 0x9EU, 0xD5U, 0xECU,
    0xE6U, 0xDFU, 0x94U, 0xADU, 0x02U, 0x3BU, 0x70U, 0x49U, 0x17U, 0x2EU, 0x65U, 0x5CU, 0xF3U, 0xCAU, 0x81U, 0xB8U,
    0x3DU, 0x04U, 0x4FU, 0x76U, 0xD9U, 0xE0U, 0xABU, 0x92U, 0xCCU, 0xF5U, 0xBEU, 0x87U, 0x28U, 0x11U, 0x5AU, 0x63U };

const uint32_t MAX_CCS_ERRS = 4U;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the LowSpeedData class.
/// </summary>
LowSpeedData::LowSpeedData() :
    m_lsd1(0x00U),
    m_lsd2(0x00U)
{
    /* stub */
}

/// <summary>
/// Finalizes a new instance of the LowSpeedData class.
/// </summary>
LowSpeedData::~LowSpeedData()
{
    /* stub */
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
LowSpeedData& LowSpeedData::operator=(const LowSpeedData& data)
{
    if (this != &data) {
        m_lsd1 = data.m_lsd1;
        m_lsd2 = data.m_lsd2;
    }

    return *this;
}

/// <summary>
/// Decodes embedded low speed data.
/// </summary>
/// <param name="data"></param>
void LowSpeedData::process(uint8_t* data)
{
    assert(data != NULL);

    uint8_t lsd[4U];
    P25Utils::decode(data, lsd, 1546U, 1578U);

    for (uint32_t a = 0x00U; a < 0x100U; a++) {
        uint8_t ccs[2U];
        ccs[0U] = a;
        ccs[1U] = encode(ccs[0U]);

        uint32_t errs = P25Utils::compare(ccs, lsd + 0U, 2U);
        if (errs < MAX_CCS_ERRS) {
            lsd[0U] = ccs[0U];
            lsd[1U] = ccs[1U];
            break;
        }
    }

    for (uint32_t a = 0x00U; a < 0x100U; a++) {
        uint8_t ccs[2U];
        ccs[0U] = a;
        ccs[1U] = encode(ccs[0U]);

        uint32_t errs = P25Utils::compare(ccs, lsd + 2U, 2U);
        if (errs < MAX_CCS_ERRS) {
            lsd[2U] = ccs[0U];
            lsd[3U] = ccs[1U];
            break;
        }
    }

    m_lsd1 = lsd[0U];
    m_lsd2 = lsd[2U];

    P25Utils::encode(lsd, data, 1546U, 1578U);
}

/// <summary>
/// Encode embedded low speed data.
/// </summary>
/// <param name="data"></param>
void LowSpeedData::encode(uint8_t* data) const
{
    assert(data != NULL);

    uint8_t lsd[4U];
    lsd[0U] = m_lsd1;
    lsd[1U] = encode(m_lsd1);
    lsd[2U] = m_lsd2;
    lsd[3U] = encode(m_lsd2);

    P25Utils::encode(lsd, data, 1546U, 1578U);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
///
/// </summary>
/// <param name="in"></param>
uint8_t LowSpeedData::encode(uint8_t in) const
{
    return CCS_PARITY[in];
}
