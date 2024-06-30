// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "frames/fsc/FSCMessage.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25::dfsi;
using namespace p25::dfsi::fsc;

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

FSCMessage::FSCMessage(uint8_t* data) :
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
