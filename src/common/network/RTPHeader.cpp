// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
 *
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

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RTPHeader class. */
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
    /* stub */
}

/* Finalizes a instance of the RTPHeader class. */
RTPHeader::~RTPHeader() = default;

/* Decode a RTP header. */
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

/* Encode a RTP header. */
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

    if (m_timestamp == INVALID_TS) {
        uint64_t timeSinceStart = hrc::diffNow(m_wcStart);
        uint64_t microSeconds = timeSinceStart * RTP_GENERIC_CLOCK_RATE;
        m_timestamp = uint32_t(microSeconds / 1000000);
    }

    __SET_UINT32(m_timestamp, data, 4U);                                        // Timestamp
    __SET_UINT32(m_ssrc, data, 8U);                                             // Synchronization Source Identifier
}

/* Helper to reset the start timestamp. */
void RTPHeader::resetStartTime()
{
    m_wcStart = hrc::hrc_t();
}
