// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/fsc/FSCMessage.h"
#include "common/p25/dfsi/frames/Frames.h"
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

/* Initializes a instance of the FSCMessage class. */

FSCMessage::FSCMessage() :
    m_messageId(FSCMessageType::FSC_INVALID),
    m_version(1U),
    m_correlationTag(0U)
{
    /* stub */
}

/* Initializes a instance of the FSCMessage class. */

FSCMessage::FSCMessage(const uint8_t* data) :
    m_messageId(FSCMessageType::FSC_INVALID),
    m_version(1U),
    m_correlationTag(0U)
{
    decode(data);
}

/* Decode a FSC message frame. */

bool FSCMessage::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_messageId = (FSCMessageType::E)(data[0U]);                // Message ID
    m_version = data[1U];                                       // Message Version

    if (m_messageId != FSCMessageType::FSC_HEARTBEAT && m_messageId != FSCMessageType::FSC_ACK)
        m_correlationTag = data[2U];                            // Message Correlation Tag

    return true;
}

/* Encode a FSC message frame. */

void FSCMessage::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = (uint8_t)m_messageId;                            // Message ID
    data[1U] = m_version;                                       // Message Version

    if (m_messageId != FSCMessageType::FSC_HEARTBEAT && m_messageId != FSCMessageType::FSC_ACK)
        data[2U] = m_correlationTag;                            // Message Correlation Tag
}

/* Create an instance of a FSCMessage. */

std::unique_ptr<FSCMessage> FSCMessage::createMessage(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t msg[FSCMessage::LENGTH + 1U];
    ::memset(msg, 0x00U, FSCMessage::LENGTH);

    uint8_t messageId = (FSCMessageType::E)(msg[0U]);           // Message ID

    // standard P25 reference opcodes
    switch (messageId) {
    case FSCMessageType::FSC_CONNECT:
        return std::unique_ptr<FSCMessage>(new FSCConnect(data));
    case FSCMessageType::FSC_HEARTBEAT:
        return std::unique_ptr<FSCMessage>(new FSCHeartbeat(data));
    case FSCMessageType::FSC_ACK:
        return std::unique_ptr<FSCMessage>(new FSCACK(data));
    case FSCMessageType::FSC_DISCONNECT:
        return std::unique_ptr<FSCMessage>(new FSCDisconnect(data));
    default:
        LogError(LOG_P25, "FSCMessage::create(), unknown message value, messageId = $%02X", messageId);
        break;
    }

    return nullptr;
}
