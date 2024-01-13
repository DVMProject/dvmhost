/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
/*
*   Copyright (C) 2023-2024 by Bryan Biedenkapp N2PLL
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
#include "network/RTPHeader.h"
#include "Clock.h"
#include "Utils.h"

using namespace system_clock;
using namespace network::frame;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

hrc::hrc_t RTPHeader::m_wcStart = hrc::hrc_t();
uint32_t RTPHeader::m_prevTimestamp = INVALID_TS;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the RTPHeader class.
/// </summary>
RTPHeader::RTPHeader() :
    m_version(2U),
    m_padding(false),
    m_extension(false),
    m_cc(0U),
    m_marker(false),
    m_payloadType(0U),
    m_seq(0U),
    m_timestamp(INVALID_TS),
    m_ssrc(0U)
{
    std::random_device rd;
    std::mt19937 mt(rd());
    m_random = mt;
}

/// <summary>
/// Finalizes a instance of the RTPHeader class.
/// </summary>
RTPHeader::~RTPHeader()
{
    /* stub */
}

/// <summary>
/// Decode a RTP header.
/// </summary>
/// <param name="data"></param>
bool RTPHeader::decode(const uint8_t* data)
{
    assert(data != nullptr);

    // check for invalid version
    if (((data[0U] >> 6) & 0x03) != 0x02U) {
        return false;
    }

    m_version = (data[0U] >> 6) & 0x03U;                                        // RTP Version
    m_padding = ((data[0U] & 0x20U) == 0x20U);                                  // Padding Flag
    m_extension = ((data[0U] & 0x10U) == 0x10U);                                // Extension Header Flag
    m_cc = (data[0U] & 0x0FU);                                                  // CSRC Count
    m_marker = ((data[1U] & 0x80U) == 0x80U);                                   // Marker Flag
    m_payloadType = (data[1U] & 0x7FU);                                         // Payload Type
    m_seq = (data[2U] << 8) | (data[3U] << 0);                                  // Sequence

    m_timestamp = __GET_UINT32(data, 4U);                                       // Timestamp
    m_ssrc = __GET_UINT32(data, 8U);                                            // Synchronization Source ID

    return true;
}

/// <summary>
/// Encode a RTP header.
/// </summary>
/// <param name="data"></param>
void RTPHeader::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = (m_version << 6) +                                               // RTP Version
        (m_padding ? 0x20U : 0x00U) +                                           // Padding Flag
        (m_extension ? 0x10U : 0x00U) +                                         // Extension Header Flag
        (m_cc & 0x0FU);                                                         // CSRC Count
    data[1U] = (m_marker ? 0x80U : 0x00U) +                                     // Marker Flag
        (m_payloadType & 0x7FU);                                                // Payload Type
    data[2U] = (m_seq >> 8) & 0xFFU;                                            // Sequence MSB
    data[3U] = (m_seq >> 0) & 0xFFU;                                            // Sequence LSB

    if (m_prevTimestamp == INVALID_TS) {
        uint64_t timeSinceStart = hrc::diffNow(m_wcStart);
        uint64_t microSeconds = timeSinceStart * RTP_GENERIC_CLOCK_RATE;
        m_timestamp = uint32_t(microSeconds / 1000000);
        m_prevTimestamp = m_timestamp;
    }
    else {
        m_timestamp = m_prevTimestamp + (RTP_GENERIC_CLOCK_RATE / 133);
        m_prevTimestamp = m_timestamp;
    }

    __SET_UINT32(m_timestamp, data, 4U);                                        // Timestamp
    __SET_UINT32(m_ssrc, data, 8U);                                             // Synchronization Source Identifier
}

/// <summary>
/// Helper to reset the start timestamp.
/// </summary>
void RTPHeader::resetStartTime()
{
    m_wcStart = hrc::hrc_t();
    m_prevTimestamp = INVALID_TS;
}
