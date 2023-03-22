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
#include "nxdn/lc/rcch/MESSAGE_TYPE_DST_ID_INFO.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn::lc::rcch;
using namespace nxdn::lc;
using namespace nxdn;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MESSAGE_TYPE_DST_ID_INFO class.
/// </summary>
MESSAGE_TYPE_DST_ID_INFO::MESSAGE_TYPE_DST_ID_INFO() : RCCH()
{
    m_messageType = nxdn::MESSAGE_TYPE_DST_ID_INFO;
}

/// <summary>
/// Decode layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_DST_ID_INFO::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    uint8_t rcch[22U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_DST_ID_INFO::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    uint8_t rcch[22U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[1U] = 0xC0U + NXDN_CALLSIGN_LENGTH_BYTES;                                  // Station ID Option - Start / End / Character Count
    rcch[2U] = (m_siteCallsign[0]);                                                 // Character 0
    for (uint8_t i = 1; i < NXDN_CALLSIGN_LENGTH_BYTES; i++) {
        rcch[i + 2U] = m_siteCallsign[i];                                           // Character 1 - 7
    }

    RCCH::encode(data, rcch, length, offset);
}
