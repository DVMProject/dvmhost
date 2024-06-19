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

#include "frames/fsc/FSCHeartbeat.h"
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
/// Initializes a instance of the FSCHeartbeat class.
/// </summary>
FSCHeartbeat::FSCHeartbeat() : FSCMessage()
{
    m_messageId = FSC_HEARTBEAT;
}

/// <summary>
/// Initializes a instance of the FSCHeartbeat class.
/// </summary>
/// <param name="data"></param>
FSCHeartbeat::FSCHeartbeat(uint8_t* data) : FSCMessage(data)
{
    decode(data);
}
