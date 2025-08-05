// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/MotStartOfStream.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace p25::dfsi;
using namespace p25::dfsi::defines;
using namespace p25::dfsi::frames;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a instance of the MotStartOfStream class. */

MotStartOfStream::MotStartOfStream() :
    m_format(DFSI_MOT_ICW_FMT_TYPE3),
    m_opcode(MotStartStreamOpcode::TRANSMIT),
    icw(nullptr)
{
    icw = new uint8_t[DFSI_MOT_ICW_LENGTH];
    ::memset(icw, 0x00U, DFSI_MOT_ICW_LENGTH);
}

/* Initializes a instance of the MotStartOfStream class. */

MotStartOfStream::MotStartOfStream(uint8_t* data) :
    m_format(DFSI_MOT_ICW_FMT_TYPE3),
    m_opcode(MotStartStreamOpcode::TRANSMIT),
    icw(nullptr)
{
    icw = new uint8_t[DFSI_MOT_ICW_LENGTH];
    ::memset(icw, 0x00U, DFSI_MOT_ICW_LENGTH);

    decode(data);
}

/* Finalizes a instance of the MotStartOfStream class. */

MotStartOfStream::~MotStartOfStream()
{
    if (icw != nullptr)
        delete icw;
}

/* Decode a start of stream frame. */

bool MotStartOfStream::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_format = data[1U] & 0x3FU;
    m_opcode = (MotStartStreamOpcode::E)data[2U];
    ::memcpy(icw, data + 3U, DFSI_MOT_ICW_LENGTH);

    return true;
}

/* Encode a start of stream frame. */

void MotStartOfStream::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = DFSIFrameType::MOT_START_STOP;
    data[1U] = m_format & 0x3FU;
    data[2U] = m_opcode;
    ::memcpy(data + 3U, icw, DFSI_MOT_ICW_LENGTH);
}
