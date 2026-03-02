// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/P25Defines.h"
#include "common/p25/dfsi/frames/MotTDULCFrame.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace p25::defines;
using namespace p25::dfsi;
using namespace p25::dfsi::defines;
using namespace p25::dfsi::frames;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a instance of the MotTDULCFrame class. */

MotTDULCFrame::MotTDULCFrame() :
    startOfStream(nullptr),
    tdulcData(nullptr)
{
    tdulcData = new uint8_t[P25_TDULC_FRAME_LENGTH_BYTES];
    ::memset(tdulcData, 0x00U, P25_TDULC_FRAME_LENGTH_BYTES);

    startOfStream = new MotStartOfStream();
}

/* Initializes a instance of the MotTDULCFrame class. */

MotTDULCFrame::MotTDULCFrame(uint8_t* data) :
    startOfStream(nullptr),
    tdulcData(nullptr)
{
    tdulcData = new uint8_t[P25_TDULC_FRAME_LENGTH_BYTES];
    ::memset(tdulcData, 0x00U, P25_TDULC_FRAME_LENGTH_BYTES);

    decode(data);
}

/* Finalizes a instance of the MotTDULCFrame class. */

MotTDULCFrame::~MotTDULCFrame()
{
    if (startOfStream != nullptr)
        delete startOfStream;
    if (tdulcData != nullptr)
        delete[] tdulcData;
}

/* Decode a TDULC frame. */

bool MotTDULCFrame::decode(const uint8_t* data)
{
    assert(data != nullptr);

    // create a new start of stream
    if (startOfStream != nullptr)
        delete startOfStream;
    startOfStream = new MotStartOfStream();

    // create a buffer to decode the start record
    uint8_t startBuffer[DFSI_MOT_START_LEN];
    ::memset(startBuffer, 0x00U, DFSI_MOT_START_LEN);
    ::memcpy(startBuffer + 1U, data, DFSI_MOT_START_LEN - 1U);

    // decode start of stream
    startOfStream->decode(startBuffer);

    ::memcpy(tdulcData, data + DFSI_MOT_START_LEN, P25_TDULC_PAYLOAD_LENGTH_BYTES + 1U);

    return true;
}

/* Encode a TDULC frame. */

void MotTDULCFrame::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(startOfStream != nullptr);

    // encode start of stream - scope is intentional
    {
        uint8_t buffer[DFSI_MOT_START_LEN];
        startOfStream->encode(buffer);

        // copy to data array
        ::memcpy(data + 1U, buffer + 1U, DFSI_MOT_START_LEN - 1U);
    }

    // encode TDULC - scope is intentional
    {
        data[0U] = DFSIFrameType::MOT_TDULC;
        ::memcpy(data + DFSI_MOT_START_LEN, tdulcData, P25_TDULC_PAYLOAD_LENGTH_BYTES + 1U);

        data[DFSI_MOT_START_LEN + 11U] = DFSI_BUSY_BITS_IDLE;
    }
}
