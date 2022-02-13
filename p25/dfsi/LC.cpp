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
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
*   Copyright (C) 2017-2019 by Bryan Biedenkapp N2PLL
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
#include "p25/P25Defines.h"
#include "p25/dfsi/DFSIDefines.h"
#include "p25/dfsi/LC.h"
#include "p25/P25Utils.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::dfsi;
using namespace p25;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
/// <param name="siteData"></param>
LC::LC() :
    m_rtModeFlag(P25_DFSI_RT_ENABLED),
    m_startStopFlag(P25_DFSI_START_FLAG),
    m_typeFlag(P25_DFSI_TYPE_VOICE),
    m_control(),
    m_lsd()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of LC class.
/// </summary>
LC::~LC()
{
    /* stub */
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
LC& LC::operator=(const LC& data)
{
    if (this != &data) {
        m_frameType = data.m_frameType;
        m_rtModeFlag = data.m_rtModeFlag;
        m_startStopFlag = data.m_startStopFlag;
        m_typeFlag = data.m_typeFlag;

        m_control = data.m_control;
        m_lsd = data.m_lsd;
    }

    return *this;
}

/// <summary>
/// Decode a NID start/stop.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeNID(const uint8_t* data)
{
    assert(data != NULL);

    m_frameType = data[0U];                                                         // Frame Type

    return decodeStart(data);
}

/// <summary>
/// Encode a NID start/stop.
/// </summary>
/// <param name="data"></param>
void LC::encodeNID(uint8_t* data)
{
    assert(data != NULL);

    uint8_t raw[P25_DFSI_SS_FRAME_LENGTH_BYTES];
    ::memset(raw, 0x00U, P25_DFSI_SS_FRAME_LENGTH_BYTES);

    raw[0U] = m_frameType;                                                          // Frame Type
    encodeStart(raw + 1U);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Decode start data.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if decoded, otherwise false.</returns>
bool LC::decodeStart(const uint8_t* data)
{
    assert(data != NULL);

    m_rtModeFlag = data[1U];                                                        // RT Mode Flag
    m_startStopFlag = data[2U];                                                     // Start/Stop Flag
    m_typeFlag = data[3U];                                                          // Type Flag

    return true;
}

/// <summary>
/// Encode start data.
/// </summary>
/// <param name="data"></param>
void LC::encodeStart(uint8_t* data)
{
    assert(data != NULL);

    uint8_t raw[P25_DFSI_START_LENGTH_BYTES];
    ::memset(raw, 0x00U, P25_DFSI_START_LENGTH_BYTES);

    raw[0U] = 0x02U;
    raw[1U] = m_rtModeFlag;                                                         // RT Mode Flag
    raw[2U] = m_startStopFlag;                                                      // Start/Stop Flag
    raw[3U] = m_typeFlag;                                                           // Type flag

    ::memcpy(data, raw, P25_DFSI_START_LENGTH_BYTES);
}
