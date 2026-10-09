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
#include "p25/kmm/KMMChangeover.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMChangeover class. */

KMMChangeover::KMMChangeover() : KMMFrame(),
    m_supersededKeysetId(0U),
    m_activeKeysetId(0U)
{
    m_messageId = KMM_MessageType::CHANGEOVER_CMD;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMChangeover class. */

KMMChangeover::~KMMChangeover() = default;

/* Gets the byte length of this KMMChangeover. */

uint32_t KMMChangeover::length() const
{
    uint32_t len = KMMFrame::length() + 3U;
    return len;
}

/* Decode a KMM changeover command. */

bool KMMChangeover::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    uint8_t noChangeovers = data[10U + m_bodyOffset];

    if (noChangeovers >= 1U) {
        m_supersededKeysetId = data[11U + m_bodyOffset];
        m_activeKeysetId = data[12U + m_bodyOffset];
    }

    // we don't support extra instructions -- only the first one

    return true;
}

/* Encode a KMM changeover command. */

void KMMChangeover::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    data[10U + m_bodyOffset] = 1U; // number of changeovers
    data[11U + m_bodyOffset] = m_supersededKeysetId;
    data[12U + m_bodyOffset] = m_activeKeysetId;
}

/* Returns a string that represents the current KMM frame. */

std::string KMMChangeover::toString()
{
    if (m_messageId == KMM_MessageType::CHANGEOVER_RSP)
        return std::string("KMM, CHANGEOVER_RSP (Changeover Response)");

    return std::string("KMM, CHANGEOVER_CMD (Changeover Command)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMChangeover::copy(const KMMChangeover& data)
{
    KMMFrame::copy(data);

    m_supersededKeysetId = data.m_supersededKeysetId;
    m_activeKeysetId = data.m_activeKeysetId;
}
