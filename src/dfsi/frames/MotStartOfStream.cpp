// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "frames/MotStartOfStream.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace p25::dfsi;
using namespace p25::dfsi::defines;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a instance of the MotStartOfStream class. */

MotStartOfStream::MotStartOfStream() :
    m_marker(FIXED_MARKER),
    m_rt(RTFlag::DISABLED),
    m_startStop(StartStopFlag::START),
    m_streamType(StreamTypeFlag::VOICE)
{
    /* stub */
}

/* Initializes a instance of the MotStartOfStream class. */

MotStartOfStream::MotStartOfStream(uint8_t* data) :
    m_marker(FIXED_MARKER),
    m_rt(RTFlag::DISABLED),
    m_startStop(StartStopFlag::START),
    m_streamType(StreamTypeFlag::VOICE)
{
    decode(data);
}

/* Decode a start of stream frame. */

bool MotStartOfStream::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_rt = (RTFlag::E)data[2U];
    m_startStop = (StartStopFlag::E)data[3U];
    m_streamType = (StreamTypeFlag::E)data[4U];

    return true;
}

/* Encode a start of stream frame. */

void MotStartOfStream::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = DFSIFrameType::MOT_START_STOP;
    data[1U] = FIXED_MARKER;
    data[2U] = m_rt;
    data[3U] = m_startStop;
    data[4U] = m_streamType;
}
