// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/kmm/KMMChangeRSI.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMChangeRSI class. */

KMMChangeRSI::KMMChangeRSI() : KMMFrame(),
    m_oldRSI(0U),
    m_newRSI(0U),
    m_initialMessageNumber(0U),
    m_status(KMM_Status::CMD_NOT_PERFORMED)
{
    m_messageId = KMM_MessageType::CHANGE_RSI_CMD;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMChangeRSI class. */

KMMChangeRSI::~KMMChangeRSI() = default;

/* Gets the byte length of this KMM frame. */

uint32_t KMMChangeRSI::length() const
{
    // Count + old RSI + new RSI + (new MN for command | status for response).
    return KMMFrame::length() +
        (m_messageId == KMM_MessageType::CHANGE_RSI_RSP ? 8U : 9U);
}

/* Decode a KMM Change-RSI command or response. */

bool KMMChangeRSI::decode(const uint8_t* data)
{
    assert(data != nullptr);

    decodeHeader(data);

    if (data[10U + m_bodyOffset] == 0U)
        return false;

    m_oldRSI = GET_UINT24(data, 11U + m_bodyOffset);
    m_newRSI = GET_UINT24(data, 14U + m_bodyOffset);

    if (m_messageId == KMM_MessageType::CHANGE_RSI_RSP)
        m_status = data[17U + m_bodyOffset];
    else
        m_initialMessageNumber = GET_UINT16(data, 17U + m_bodyOffset);

    return true;
}

/* Encode a KMM Change-RSI command or response. */

void KMMChangeRSI::encode(uint8_t* data)
{
    assert(data != nullptr);

    m_messageLength = length();
    encodeHeader(data);

    data[10U + m_bodyOffset] = 1U;
    SET_UINT24(m_oldRSI, data, 11U + m_bodyOffset);
    SET_UINT24(m_newRSI, data, 14U + m_bodyOffset);

    if (m_messageId == KMM_MessageType::CHANGE_RSI_RSP) {
        data[17U + m_bodyOffset] = m_status;
    }
    else {
        SET_UINT16(m_initialMessageNumber, data, 17U + m_bodyOffset);
    }
}

/* Returns a string that represents the current KMM frame. */

std::string KMMChangeRSI::toString()
{
    if (m_messageId == KMM_MessageType::CHANGE_RSI_RSP)
        return std::string("KMM, CHANGE_RSI_RSP (Change RSI Response)");

    return std::string("KMM, CHANGE_RSI_CMD (Change RSI Command)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMChangeRSI::copy(const KMMChangeRSI& data)
{
    KMMFrame::copy(data);

    m_oldRSI = data.m_oldRSI;
    m_newRSI = data.m_newRSI;
    m_initialMessageNumber = data.m_initialMessageNumber;
    m_status = data.m_status;
}
