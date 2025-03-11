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
#include "p25/kmm/KMMNegativeAck.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMNegativeAck class. */

KMMNegativeAck::KMMNegativeAck() : KMMFrame(),
    m_messageId(0U),
    m_messageNo(0U),
    m_status(KMM_Status::CMD_NOT_PERFORMED)
{
    m_messageId = KMM_MessageType::NAK;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMNegativeAck class. */

KMMNegativeAck::~KMMNegativeAck() = default;

/* Decode a KMM NAK. */

bool KMMNegativeAck::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_messageId = data[10U];                                    // Message ID
    m_messageNo = __GET_UINT16B(data, 11U);                     // Message Number
    m_status = data[13U];                                       // Status

    return true;
}

/* Encode a KMM NAK. */

void KMMNegativeAck::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = KMM_NEGATIVE_ACK_LENGTH;

    KMMFrame::encodeHeader(data);

    data[10U] = m_messageId;                                    // Message ID
    __SET_UINT16B(m_messageNo, data, 11U);                      // Message Number
    data[13U] = m_status;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMNegativeAck::copy(const KMMNegativeAck& data)
{
    KMMFrame::copy(data);

    m_messageId = data.m_messageId;
    m_messageNo = data.m_messageNo;
    m_status = data.m_status;
}
