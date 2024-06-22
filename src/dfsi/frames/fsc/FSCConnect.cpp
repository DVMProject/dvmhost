// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - DFSI Peer Application
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI Peer Application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/

#include "frames/fsc/FSCConnect.h"
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

/// <summary>
/// Initializes a instance of the FSCConnect class.
/// </summary>
FSCConnect::FSCConnect() : FSCMessage(),
    m_vcBasePort(0U),
    m_vcSSRC(0U),
    m_fsHeartbeatPeriod(5U),
    m_hostHeartbeatPeriod(5U)
{
    m_messageId = FSCMessageType::FSC_CONNECT;
}

/// <summary>
/// Initializes a instance of the FSCConnect class.
/// </summary>
/// <param name="data"></param>
FSCConnect::FSCConnect(uint8_t* data) : FSCMessage(data),
    m_vcBasePort(0U),
    m_vcSSRC(0U),
    m_fsHeartbeatPeriod(5U),
    m_hostHeartbeatPeriod(5U)
{
    decode(data);
}

/// <summary>
/// Decode a FSC connect frame.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
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

/// <summary>
/// Encode a FSC connect frame.
/// </summary>
/// <param name="data"></param>
void FSCConnect::encode(uint8_t* data)
{
    assert(data != nullptr);
    FSCMessage::encode(data);

    __SET_UINT16B(m_vcBasePort, data, 3U);                      // Voice Conveyance RTP Port
    __SET_UINT32(m_vcSSRC, data, 5U);                           // Voice Conveyance SSRC
    data[9U] = m_fsHeartbeatPeriod;                             // Fixed Station Heartbeat Period
    data[10U] = m_hostHeartbeatPeriod;                          // Host Heartbeat Period
}
