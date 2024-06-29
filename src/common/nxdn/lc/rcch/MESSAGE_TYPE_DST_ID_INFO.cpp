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
#include "nxdn/lc/rcch/MESSAGE_TYPE_DST_ID_INFO.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MESSAGE_TYPE_DST_ID_INFO class. */

MESSAGE_TYPE_DST_ID_INFO::MESSAGE_TYPE_DST_ID_INFO() : RCCH()
{
    m_messageType = MessageType::DST_ID_INFO;
}

/* Decode RCCH data. */

void MESSAGE_TYPE_DST_ID_INFO::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);
}

/* Encode RCCH data. */

void MESSAGE_TYPE_DST_ID_INFO::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[1U] = 0xC0U + CALLSIGN_LENGTH_BYTES;                                       // Station ID Option - Start / End / Character Count
    rcch[2U] = (m_siteCallsign[0]);                                                 // Character 0
    for (uint8_t i = 1; i < CALLSIGN_LENGTH_BYTES; i++) {
        rcch[i + 2U] = m_siteCallsign[i];                                           // Character 1 - 7
    }

    RCCH::encode(data, rcch, length, offset);
}

/* Returns a string that represents the current RCCH. */

std::string MESSAGE_TYPE_DST_ID_INFO::toString(bool isp)
{
    return std::string("DST_ID_INFO (Digital Station ID)");
}
