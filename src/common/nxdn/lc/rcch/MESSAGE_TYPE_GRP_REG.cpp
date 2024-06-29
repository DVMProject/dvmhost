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
#include "nxdn/lc/rcch/MESSAGE_TYPE_GRP_REG.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MESSAGE_TYPE_GRP_REG class. */
MESSAGE_TYPE_GRP_REG::MESSAGE_TYPE_GRP_REG() : RCCH()
{
    m_messageType = MessageType::RCCH_GRP_REG;
}

/* Decode RCCH data. */
void MESSAGE_TYPE_GRP_REG::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);

    m_regOption = rcch[1U];                                                         // Group Registration Option
    m_srcId = (uint16_t)((rcch[2U] << 8) | rcch[3U]) & 0xFFFFU;                     // Source Radio Address
    m_dstId = (uint16_t)((rcch[4U] << 8) | rcch[5U]) & 0xFFFFU;                     // Target Radio Address
}

/* Encode RCCH data. */
void MESSAGE_TYPE_GRP_REG::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[2U] = (m_srcId >> 8U) & 0xFFU;                                             // Source Radio Address
    rcch[3U] = (m_srcId >> 0U) & 0xFFU;                                             // ...
    rcch[4U] = (m_dstId >> 8U) & 0xFFU;                                             // Target Radio Address
    rcch[5U] = (m_dstId >> 0U) & 0xFFU;                                             // ...
    rcch[6U] = m_causeRsp;                                                          // Cause (MM)
    rcch[8U] = (m_siteData.locId() >> 8) & 0xFFU;                                   // Location ID
    rcch[9U] = (m_siteData.locId() >> 0) & 0xFFU;                                   // ...

    RCCH::encode(data, rcch, length, offset);
}

/* Returns a string that represents the current RCCH. */
std::string MESSAGE_TYPE_GRP_REG::toString(bool isp)
{
    return (isp) ? std::string("RCCH_GRP_REG (Group Registration Request)") :
        std::string("RCCH_GRP_REG (Group Registration Response)");
}
