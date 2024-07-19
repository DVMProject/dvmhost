// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/fsc/FSCConnect.h"
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

/* Initializes a instance of the FSCConnect class. */

FSCConnect::FSCConnect() : FSCMessage(),
    m_vcBasePort(0U),
    m_vcSSRC(0U),
    m_fsHeartbeatPeriod(5U),
    m_hostHeartbeatPeriod(5U)
{
    m_messageId = FSCMessageType::FSC_CONNECT;
}

/* Initializes a instance of the FSCConnect class. */

FSCConnect::FSCConnect(uint8_t* data) : FSCMessage(data),
    m_vcBasePort(0U),
    m_vcSSRC(0U),
    m_fsHeartbeatPeriod(5U),
    m_hostHeartbeatPeriod(5U)
{
    decode(data);
}

/* Decode a FSC connect frame. */

bool FSCConnect::decode(const uint8_t* data)
{
    assert(data != nullptr);
    FSCMessage::decode(data);

    m_vcBasePort = __GET_UINT16B(data, 3U);                     // Voice Conveyance RTP Port
    m_vcSSRC = __GET_UINT32(data, 5U);                          // Voice Conveyance SSRC
    m_fsHeartbeatPeriod = data[9U];                             // Fixed Station Heartbeat Period
    m_hostHeartbeatPeriod = data[10U];                          // Host Heartbeat Period

    return true;
}

/* Encode a FSC connect frame. */

void FSCConnect::encode(uint8_t* data)
{
    assert(data != nullptr);
    FSCMessage::encode(data);

    __SET_UINT16B(m_vcBasePort, data, 3U);                      // Voice Conveyance RTP Port
    __SET_UINT32(m_vcSSRC, data, 5U);                           // Voice Conveyance SSRC
    data[9U] = m_fsHeartbeatPeriod;                             // Fixed Station Heartbeat Period
    data[10U] = m_hostHeartbeatPeriod;                          // Host Heartbeat Period
}
