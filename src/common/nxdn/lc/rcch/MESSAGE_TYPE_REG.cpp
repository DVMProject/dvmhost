// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024,2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_REG.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MESSAGE_TYPE_REG class. */

MESSAGE_TYPE_REG::MESSAGE_TYPE_REG() : RCCH()
{
    m_messageType = MessageType::RCCH_REG;
}

/* Decode RCCH data. */

void MESSAGE_TYPE_REG::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);

    m_regOption = rcch[1U] >> 3;                                                    // Registration Option

    m_locId = ((rcch[1U] & 0x07U) << 3) + ((rcch[2U] & 0xFFU) << 8U) +              // Location ID
        (rcch[3U] & 0xFFU);                                                         // ...

    m_srcId = (uint16_t)((rcch[4U] << 8) | rcch[5U]) & 0xFFFFU;                     // Source Radio Address
    m_dstId = (uint16_t)((rcch[6U] << 8) | rcch[7U]) & 0xFFFFU;                     // Talkgroup Address
    // bryanb: maybe process subscriber type? (byte 8 and 9)
    m_version = rcch[10U];                                                          // Version
}

/* Encode RCCH data. */

void MESSAGE_TYPE_REG::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[1U] = (m_regOption << 3) +                                                 // Registration Option
            ((m_siteData.locId() >> 22U) & 0x03U);                                  // Location ID

    uint16_t systemCode = (m_siteData.locId() >> 12U) << 7U;
    rcch[2U] = (systemCode >> 8U) & 0x03U;                                          // ...
    rcch[3U] = systemCode & 0xFFU;                                                  // ...

    rcch[4U] = (m_srcId >> 8U) & 0xFFU;                                             // Source Radio Address
    rcch[5U] = (m_srcId >> 0U) & 0xFFU;                                             // ...
    rcch[6U] = (m_dstId >> 8U) & 0xFFU;                                             // Talkgroup Address
    rcch[7U] = (m_dstId >> 0U) & 0xFFU;                                             // ...
    rcch[8U] = m_causeRsp;                                                          // Cause (MM)

    RCCH::encode(data, rcch, length, offset);
}

/* Returns a string that represents the current RCCH. */

std::string MESSAGE_TYPE_REG::toString(bool isp)
{
    return (isp) ? std::string("RCCH_REG (Registration Request)") :
        std::string("RCCH_REG (Registration Response)");
}
