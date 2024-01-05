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
#include "Log.h"

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
    m_outbound(true),
    m_lich(0U)
{
    /* stub */
}

/// <summary>
/// Initializes a copy instance of the LICH class.
/// </summary>
/// <param name="data"></param>
LICH::LICH(const LICH& data) :
    m_rfct(NXDN_LICH_RFCT_RCCH),
    m_fct(NXDN_LICH_USC_SACCH_NS),
    m_option(0U),
    m_outbound(true),
    m_lich(0U)
{
    copy(data);
}

/// <summary>
/// Finalizes a instance of LICH class.
/// </summary>
LICH::~LICH()
{
    /* stub */
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
LICH& LICH::operator=(const LICH& data)
{
    if (&data != this) {
        m_lich = data.m_lich;

        m_rfct = data.m_rfct;
        m_fct = data.m_fct;
        m_option = data.m_option;
        m_outbound = data.m_outbound;
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
    assert(data != nullptr);

    uint8_t lich[1U];
    ::memset(lich, 0x00U, 1U);

    uint32_t offset = NXDN_FSW_LENGTH_BITS;
    for (uint32_t i = 0U; i < (NXDN_LICH_LENGTH_BITS / 2U); i++, offset += 2U) {
        bool b = READ_BIT(data, offset);
        WRITE_BIT(lich, i, b);
    }

    m_lich = lich[0U];

#if DEBUG_NXDN_LICH
    LogDebug(LOG_NXDN, "LICH::decode(), m_lich = %02X", m_lich);
#endif

    bool newParity = getParity();
    bool origParity = (m_lich & 0x01U) == 0x01U;

    m_rfct = (m_lich >> 6) & 0x03U;
    m_fct = (m_lich >> 4) & 0x03U;
    m_option = (m_lich >> 2) & 0x03U;
    m_outbound = ((m_lich >> 1) & 0x01U) == 0x01U;

    return origParity == newParity;
}

/// <summary>
/// Encode a link information channel.
/// </summary>
/// <param name="data"></param>
void LICH::encode(uint8_t* data)
{
    assert(data != nullptr);

    m_lich = 0U;

	m_lich &= 0x3FU;
    m_lich |= (m_rfct & 0x03U) << 6;

	m_lich &= 0xCFU;
    m_lich |= (m_fct & 0x03U) << 4;

	m_lich &= 0xF3U;
    m_lich |= (m_option & 0x03U) << 2;

	m_lich &= 0xFDU;
    m_lich |= (m_outbound ? 0x01U : 0x00U) << 1;

#if DEBUG_NXDN_LICH
    LogDebug(LOG_NXDN, "LICH::encode(), m_lich = %02X", m_lich);
#endif

    bool parity = getParity();
    if (parity)
        m_lich |= 0x01U;
    else
        m_lich &= 0xFEU;

    uint8_t lich[1U];
    ::memset(lich, 0x00U, 1U);

    lich[0U] = m_lich;

    uint32_t offset = NXDN_FSW_LENGTH_BITS;
    for (uint32_t i = 0U; i < (NXDN_LICH_LENGTH_BITS / 2U); i++) {
        bool b = READ_BIT(lich, i);
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
    m_lich = data.m_lich;

    m_rfct = (m_lich >> 6) & 0x03U;
    m_fct = (m_lich >> 4) & 0x03U;
    m_option = (m_lich >> 2) & 0x03U;
    m_outbound = ((m_lich >> 1) & 0x01U) == 0x01U;
}

/// <summary>
///
/// </summary>
/// <returns></returns>
bool LICH::getParity() const
{
    switch (m_lich & 0xF0U) {
    case 0x80U:
    case 0xB0U:
        return true;
    default:
        return false;
    }
}
