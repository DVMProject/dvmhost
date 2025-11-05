// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/kmm/KMMRekeyAck.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMRekeyAck class. */

KMMRekeyAck::KMMRekeyAck() : KMMFrame(),
    m_messageId(0U),
    m_numberOfKeyStatus(0U),
    m_keystatus()
{
    m_messageId = KMM_MessageType::REKEY_ACK;
    m_respKind = KMM_ResponseKind::NONE;
}

/* Finalizes a instance of the KMMRekeyAck class. */

KMMRekeyAck::~KMMRekeyAck() = default;

/* Gets the byte length of this KMMRekeyAck. */

uint32_t KMMRekeyAck::length() const
{
    uint32_t len = KMMFrame::length() + KMM_BODY_REKEY_ACK_LENGTH;
    len += m_keystatus.size() * 4U;

    return len;
}

/* Decode a KMM rekey ack. */

bool KMMRekeyAck::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_messageId = data[10U + m_bodyOffset];                     // Message ID
    m_numberOfKeyStatus = data[11U + m_bodyOffset];             // Number of Key Status

    uint16_t offset = 0U;
    for (uint8_t i = 0U; i < m_numberOfKeyStatus; i++) {
        KeyStatus keyStatus = KeyStatus();

        keyStatus.algId(data[12U + (m_bodyOffset + offset)]);

        uint16_t kId = GET_UINT16(data, 13U + (m_bodyOffset + offset));
        keyStatus.kId(kId);

        keyStatus.status(data[15U + (m_bodyOffset + offset)]);

        m_keystatus.push_back(keyStatus);
        offset += 4U;
    }

    return true;
}

/* Encode a KMM rekey ack. */

void KMMRekeyAck::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    data[10U + m_bodyOffset] = m_messageId;                     // Message ID
    data[11U + m_bodyOffset] = m_numberOfKeyStatus;             // Number of Key Status

    uint16_t offset = 0U;
    for (auto keyStatus : m_keystatus) {
        data[12U + (m_bodyOffset + offset)] = keyStatus.algId();
        SET_UINT16(keyStatus.kId(), data, 13U + (m_bodyOffset + offset));

        data[15U + (m_bodyOffset + offset)] = keyStatus.status();
        offset += 4U;
    }
}

/* Returns a string that represents the current KMM frame. */

std::string KMMRekeyAck::toString()
{
    return std::string("KMM, REKEY_ACK (Rekey Acknowledge)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMRekeyAck::copy(const KMMRekeyAck& data)
{
    KMMFrame::copy(data);

    m_messageId = data.m_messageId;
    m_numberOfKeyStatus = data.m_numberOfKeyStatus;

    m_keystatus = data.m_keystatus;
}
