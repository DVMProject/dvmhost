// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/fsc/FSCSelChannel.h"
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

/* Initializes a instance of the FSCSelChannel class. */

FSCSelChannel::FSCSelChannel() : FSCMessage(),
    m_rxChan(1U),
    m_txChan(1U)
{
    m_messageId = FSCMessageType::FSC_SEL_CHAN;
}

/* Decode a FSC select channel frame. */

bool FSCSelChannel::decode(const uint8_t* data)
{
    assert(data != nullptr);
    FSCMessage::decode(data);

    m_rxChan = data[3U];                                        // Receive Channel
    m_txChan = data[4U];                                        // Transmit Channel

    return true;
}

/* Encode a FSC select channel frame. */

void FSCSelChannel::encode(uint8_t* data)
{
    assert(data != nullptr);
    FSCMessage::encode(data);

    data[3U] = m_rxChan;                                        // Receive Channel
    data[4U] = m_txChan;                                        // Transmit Channel
}
