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
*   Copyright (C) 2018 by Jonathan Naylor G4KLX
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
#include "nxdn/NXDNDefines.h"
#include "nxdn/channel/LICH.h"

using namespace nxdn;
using namespace nxdn::channel;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the LICH class.
/// </summary>
LICH::LICH() :
    m_rfct(NXDN_LICH_RFCT_RCCH),
    m_fct(NXDN_LICH_USC_SACCH_NS),
    m_option(0U),
    m_direction(NXDN_LICH_DIRECTION_OUTBOUND),
    m_data(NULL)
{
    m_data = new uint8_t[1U];
}

/// <summary>
/// Initializes a copy instance of the LICH class.
/// </summary>
/// <param name="data"></param>
LICH::LICH(const LICH& data) :
    m_rfct(NXDN_LICH_RFCT_RCCH),
    m_fct(NXDN_LICH_USC_SACCH_NS),
    m_option(0U),
    m_direction(NXDN_LICH_DIRECTION_OUTBOUND),
    m_data(NULL)
{
    copy(data);
}

/// <summary>
/// Finalizes a instance of LICH class.
/// </summary>
LICH::~LICH()
{
    delete[] m_data;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
LICH& LICH::operator=(const LICH& data)
{
    if (&data != this) {
        m_data[0U] = data.m_data[0U];

        m_rfct = data.m_rfct;
        m_fct = data.m_fct;
        m_option = data.m_option;
        m_direction = data.m_direction;
    }

    return *this;
}

/// <summary>
/// Decode a link information channel.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if LICH was decoded, otherwise false.</returns>
bool LICH::decode(const uint8_t* data)
{
    assert(data != NULL);

    uint32_t offset = NXDN_FSW_LENGTH_BITS;
    for (uint32_t i = 0U; i < (NXDN_LICH_LENGTH_BITS / 2U); i++, offset += 2U) {
        bool b = READ_BIT(data, offset);
        WRITE_BIT(m_data, i, b);
    }

    bool newParity = getParity();
    bool origParity = (m_data[0U] & 0x01U) == 0x01U;

    m_rfct = (m_data[0U] >> 6) & 0x03U;
    m_fct = (m_data[0U] >> 4) & 0x03U;
    m_option = (m_data[0U] >> 2) & 0x03U;
    m_direction = (m_data[0U] >> 1) & 0x01U;

    return origParity == newParity;
}

/// <summary>
/// Encode a link information channel.
/// </summary>
/// <param name="data"></param>
void LICH::encode(uint8_t* data)
{
    assert(data != NULL);

    m_data[0U] &= 0x3FU;
    m_data[0U] |= (m_rfct << 6) & 0xC0U;

    m_data[0U] &= 0xCFU;
    m_data[0U] |= (m_fct << 4) & 0x30U;

    m_data[0U] &= 0xF3U;
    m_data[0U] |= (m_option << 2) & 0x0CU;

    m_data[0U] &= 0xFDU;
    m_data[0U] |= (m_direction << 1) & 0x02U;

    bool parity = getParity();
    if (parity)
        m_data[0U] |= 0x01U;
    else
        m_data[0U] &= 0xFEU;

    uint32_t offset = NXDN_FSW_LENGTH_BITS;
    for (uint32_t i = 0U; i < (NXDN_LICH_LENGTH_BITS / 2U); i++) {
        bool b = READ_BIT(m_data, i);
        WRITE_BIT(data, offset, b);
        offset++;
        WRITE_BIT(data, offset, true);
        offset++;
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void LICH::copy(const LICH& data)
{
    m_data = new uint8_t[1U];
    m_data[0U] = data.m_data[0U];

    m_rfct = (m_data[0U] >> 6) & 0x03U;
    m_fct = (m_data[0U] >> 4) & 0x03U;
    m_option = (m_data[0U] >> 2) & 0x03U;
    m_direction = (m_data[0U] >> 1) & 0x01U;
}

/// <summary>
/// 
/// </summary>
/// <returns></returns>
bool LICH::getParity() const
{
    switch (m_data[0U] & 0xF0U) {
    case 0x80U:
    case 0xB0U:
        return true;
    default:
        return false;
    }
}
