// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/fsc/FSCDisconnect.h"
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

/* Initializes a instance of the FSCDisconnect class. */

FSCDisconnect::FSCDisconnect() : FSCMessage()
{
    m_messageId = FSCMessageType::FSC_DISCONNECT;
}

/* Initializes a instance of the FSCDisconnect class. */

FSCDisconnect::FSCDisconnect(uint8_t* data) : FSCMessage(data)
{
    decode(data);
}
