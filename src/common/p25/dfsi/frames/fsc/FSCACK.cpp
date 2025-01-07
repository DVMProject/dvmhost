// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/fsc/FSCACK.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25::dfsi;
using namespace p25::dfsi::frames;
using namespace p25::dfsi::frames::fsc;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a instance of the FSCACK class. */

FSCACK::FSCACK() : FSCMessage(),
    m_ackMessageId(FSCMessageType::FSC_INVALID),
    m_ackVersion(1U),
    m_ackCorrelationTag(0U),
    m_responseCode(FSCAckResponseCode::CONTROL_ACK),
    m_respLength(0U)
{
    m_messageId = FSCMessageType::FSC_ACK;
}

/* Initializes a instance of the FSCACK class. */

FSCACK::FSCACK(const uint8_t* data) : FSCMessage(data),
    m_ackMessageId(FSCMessageType::FSC_INVALID),
    m_ackVersion(1U),
    m_ackCorrelationTag(0U),
    m_responseCode(FSCAckResponseCode::CONTROL_ACK),
    m_respLength(0U)
{
    FSCACK::decode(data);
}

/* Decode a FSC ACK frame. */

bool FSCACK::decode(const uint8_t* data)
{
    assert(data != nullptr);
    FSCMessage::decode(data);

    m_ackMessageId = (FSCMessageType::E)(data[2U]);             // Ack Message ID
    m_ackVersion = data[3U];                                    // Ack Message Version
    m_ackCorrelationTag = data[4U];                             // Ack Message Correlation Tag
    m_responseCode = (FSCAckResponseCode::E)(data[5U]);         // Response Code
    m_respLength = data[6U];                                    // Response Data Length

    if (m_respLength > 0) {
        if (responseData != nullptr)
            delete responseData;
        responseData = new uint8_t[m_respLength];
        ::memset(responseData, 0x00U, m_respLength);
        ::memcpy(responseData, data, m_respLength);
    }
    else {
        if (responseData != nullptr)
            delete responseData;
        responseData = nullptr;
    }

    return true;
}

/* Encode a FSC ACK frame. */

void FSCACK::encode(uint8_t* data)
{
    assert(data != nullptr);
    FSCMessage::encode(data);

    data[2U] = (uint8_t)(m_ackMessageId);                       // Ack Message ID
    data[3U] = m_ackVersion;                                    // Ack Message Version
    data[4U] = m_ackCorrelationTag;                             // Ack Message Correlation Tag
    data[5U] = (uint8_t)(m_responseCode);                       // Response Code
    data[6U] = m_respLength;                                    // Response Data Length

    if (m_respLength > 0U && responseData != nullptr) {
        ::memcpy(data, responseData, m_respLength);
    }
}
