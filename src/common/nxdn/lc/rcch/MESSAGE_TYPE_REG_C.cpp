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
#include "nxdn/lc/rcch/MESSAGE_TYPE_REG_C.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MESSAGE_TYPE_REG_C class. */

MESSAGE_TYPE_REG_C::MESSAGE_TYPE_REG_C() : RCCH()
{
    m_messageType = MessageType::RCCH_REG_C;
}

/* Decode RCCH data. */

void MESSAGE_TYPE_REG_C::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);

    m_regOption = rcch[1U] >> 3;                                                    // Registration Option
    m_locId = (uint16_t)((rcch[2U] << 8) | rcch[3U]) & 0xFFFFU;                     // Location ID
    m_srcId = (uint16_t)((rcch[4U] << 8) | rcch[5U]) & 0xFFFFU;                     // Source Radio Address
}

/* Encode RCCH data. */

void MESSAGE_TYPE_REG_C::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[2U] = (m_siteData.locId() >> 8) & 0xFFU;                                   // Location ID
    rcch[3U] = (m_siteData.locId() >> 0) & 0xFFU;                                   // ...
    rcch[4U] = (m_dstId >> 8U) & 0xFFU;                                             // Target Radio Address
    rcch[5U] = (m_dstId >> 0U) & 0xFFU;                                             // ...
    rcch[6U] = m_causeRsp;                                                          // Cause (MM)

    RCCH::encode(data, rcch, length, offset);
}

/* Returns a string that represents the current RCCH. */

std::string MESSAGE_TYPE_REG_C::toString(bool isp)
{
    return (isp) ? std::string("RCCH_REG_C (Registration Clear Request)") :
        std::string("RCCH_REG_C (Registration Clear Response)");
}
