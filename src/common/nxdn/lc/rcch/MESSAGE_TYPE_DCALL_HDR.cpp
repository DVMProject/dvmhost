// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_DCALL_HDR.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MESSAGE_TYPE_DCALL_HDR class. */

MESSAGE_TYPE_DCALL_HDR::MESSAGE_TYPE_DCALL_HDR() : RCCH()
{
    m_messageType = MessageType::RTCH_DCALL_HDR;
}

/* Decode RCCH data. */

void MESSAGE_TYPE_DCALL_HDR::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);

    m_callType = (rcch[2U] >> 5) & 0x07U;                                           // Call Type
    m_emergency = (rcch[1U] & 0x80U) == 0x80U;                                      // Emergency Flag
    m_priority = (rcch[1U] & 0x20U) == 0x20U;                                       // Priority Flag
    m_duplex = (rcch[2U] & 0x10U) == 0x10U;                                         // Half/Full Duplex Flag
    m_transmissionMode = (rcch[2U] & 0x07U);                                        // Transmission Mode
    m_srcId = (uint16_t)((rcch[3U] << 8) | rcch[4U]) & 0xFFFFU;                     // Source Radio Address
    m_dstId = (uint16_t)((rcch[5U] << 8) | rcch[6U]) & 0xFFFFU;                     // Target Radio Address
}

/* Encode RCCH data. */

void MESSAGE_TYPE_DCALL_HDR::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[1U] = (m_emergency ? 0x80U : 0x00U) +                                      // Emergency Flag
        (m_priority ? 0x20U : 0x00U);                                               // Priority Flag
    rcch[2U] = ((m_callType & 0x07U) << 5) +                                        // Call Type
        (m_duplex ? 0x10U : 0x00U) +                                                // Half/Full Duplex Flag
        (m_transmissionMode & 0x07U);                                               // Transmission Mode

    rcch[3U] = (m_srcId >> 8U) & 0xFFU;                                             // Source Radio Address
    rcch[4U] = (m_srcId >> 0U) & 0xFFU;                                             // ...
    rcch[5U] = (m_dstId >> 8U) & 0xFFU;                                             // Target Radio Address
    rcch[6U] = (m_dstId >> 0U) & 0xFFU;                                             // ...

    rcch[7U] = m_causeRsp;                                                          // Cause (VD)
    rcch[9U] = (m_siteData.locId() >> 8) & 0xFFU;                                   // Location ID
    rcch[10U] = (m_siteData.locId() >> 0) & 0xFFU;                                  // ...

    RCCH::encode(data, rcch, length, offset);
}

/* Returns a string that represents the current RCCH. */

std::string MESSAGE_TYPE_DCALL_HDR::toString(bool isp)
{
    return std::string("RTCH_DCALL_HDR (Data Call Header)");
}
