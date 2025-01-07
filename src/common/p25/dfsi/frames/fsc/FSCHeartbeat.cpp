// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/fsc/FSCHeartbeat.h"
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

/* Initializes a instance of the FSCHeartbeat class. */

FSCHeartbeat::FSCHeartbeat() : FSCMessage()
{
    m_messageId = FSCMessageType::FSC_HEARTBEAT;
}

/* Initializes a instance of the FSCHeartbeat class. */

FSCHeartbeat::FSCHeartbeat(const uint8_t* data) : FSCMessage(data)
{
    /* stub */
}
