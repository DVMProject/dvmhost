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
#include "p25/kmm/KMMFrame.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a copy instance of the KMMFrame class. */

KMMFrame::KMMFrame(const KMMFrame& data) : KMMFrame()
{
    copy(data);
}

/* Initializes a new instance of the KMMFrame class. */

KMMFrame::KMMFrame() :
    m_messageId(KMM_MessageType::NULL_CMD),
    m_messageLength(KMM_FRAME_LENGTH),
    m_respKind(KMM_ResponseKind::NONE),
    m_complete(true),
    m_mfMessageNumber(0U),
    m_mfMac(KMM_MAC::NO_MAC)
{
    /* stub */
}

/* Finalizes a instance of the KMMFrame class. */

KMMFrame::~KMMFrame() = default;

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to decode a KMM header. */

bool KMMFrame::decodeHeader(const uint8_t* data)
{
    assert(data != nullptr);

    m_messageId = data[0U];                                                         // Message ID
    m_messageLength = GET_UINT16(data, 1U);                                         // Message Length

    m_respKind = (data[3U] >> 6U) & 0x03U;                                          // Response Kind
    m_mfMessageNumber = (data[3U] >> 4U) & 0x03U;                                   // Message Number
    m_mfMac = (data[3U] >> 2U) & 0x03U;                                             // MAC

    bool done = (data[3U] & 0x01U) == 0x01U;                                        // Done Flag
    if (!done)
        m_complete = true;
    else
        m_complete = false;

    m_dstLlId = GET_UINT24(data, 4U);                                               // Destination RSI
    m_srcLlId = GET_UINT24(data, 7U);                                               // Source RSI

    return true;
}

/* Internal helper to encode a KMM header. */

void KMMFrame::encodeHeader(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = m_messageId;                                                         // Message ID
    SET_UINT16(m_messageLength, data, 1U);                                          // Message Length

    data[3U] = ((m_respKind & 0x03U) << 6U) +                                       // Response Kind
        ((m_mfMessageNumber & 0x03U) << 4U) +                                       // Message Number
        ((m_mfMac & 0x03U) << 2U) +                                                 // MAC
        ((!m_complete) ? 0x01U : 0x00U);                                            // Done Flag

    SET_UINT24(m_dstLlId, data, 4U);                                                // Destination RSI
    SET_UINT24(m_srcLlId, data, 7U);                                                // Source RSI
}

/* Internal helper to copy the the class. */

void KMMFrame::copy(const KMMFrame& data)
{
    m_messageId = data.m_messageId;
    m_messageLength = data.m_messageLength;
    m_respKind = data.m_respKind;
    m_complete = data.m_complete;

    m_mfMessageNumber = data.m_mfMessageNumber;
    m_mfMac = data.m_mfMac;
}
