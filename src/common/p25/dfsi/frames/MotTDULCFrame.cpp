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

    uint8_t tdulcBuffer[9U];
    ::memcpy(tdulcBuffer, data + DFSI_MOT_START_LEN, 9U);

    ::memset(tdulcData, 0x00U, P25_TDULC_FRAME_LENGTH_BYTES);
    tdulcData[0U] = (uint8_t)((tdulcBuffer[0U] >> 3) & 0x3FU);
    tdulcData[1U] = (uint8_t)(((tdulcBuffer[0U] & 0x07U) << 3) | ((tdulcBuffer[1U] >> 4) & 0x07U));
    tdulcData[2U] = (uint8_t)(((tdulcBuffer[1U] & 0x0FU) << 2) | ((tdulcBuffer[2U] >> 5) & 0x03U));
    tdulcData[3U] = (uint8_t)(tdulcBuffer[2U] & 0x1FU);
    tdulcData[4U] = (uint8_t)((tdulcBuffer[3U] >> 3) & 0x3FU);
    tdulcData[5U] = (uint8_t)(((tdulcBuffer[3U] & 0x07U) << 3) | ((tdulcBuffer[4U] >> 4) & 0x07U));
    tdulcData[6U] = (uint8_t)(((tdulcBuffer[4U] & 0x0FU) << 2) | ((tdulcBuffer[5U] >> 5) & 0x03U));
    tdulcData[7U] = (uint8_t)(tdulcBuffer[5U] & 0x1FU);
    tdulcData[8U] = (uint8_t)((tdulcBuffer[6U] >> 3) & 0x3FU);
    tdulcData[9U] = (uint8_t)(((tdulcBuffer[6U] & 0x07U) << 3) | ((tdulcBuffer[7U] >> 4) & 0x07U));
    tdulcData[10U] = (uint8_t)(((tdulcBuffer[7U] & 0x0FU) << 2) | ((tdulcBuffer[8U] >> 5) & 0x03U));
    tdulcData[11U] = (uint8_t)(tdulcBuffer[8U] & 0x1FU);

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

        data[DFSI_MOT_START_LEN + 1U] = (uint8_t)((tdulcData[0U] & 0x3FU) << 3) | ((tdulcData[1U] >> 3) & 0x07U);
        data[DFSI_MOT_START_LEN + 2U] = (uint8_t)((tdulcData[1U] & 0x0FU) << 4) | ((tdulcData[2U] >> 2) & 0x0FU);
        data[DFSI_MOT_START_LEN + 3U] = (uint8_t)((tdulcData[2U] & 0x03U)) | (tdulcData[3U] & 0x3FU);

        data[DFSI_MOT_START_LEN + 4U] = (uint8_t)((tdulcData[4U] & 0x3FU) << 3) | ((tdulcData[5U] >> 3) & 0x07U);
        data[DFSI_MOT_START_LEN + 5U] = (uint8_t)((tdulcData[5U] & 0x0FU) << 4) | ((tdulcData[6U] >> 2) & 0x0FU);
        data[DFSI_MOT_START_LEN + 6U] = (uint8_t)((tdulcData[6U] & 0x03U)) | (tdulcData[7U] & 0x3FU);

        data[DFSI_MOT_START_LEN + 7U] = (uint8_t)((tdulcData[8U] & 0x3FU) << 3) | ((tdulcData[9U] >> 3) & 0x07U);
        data[DFSI_MOT_START_LEN + 8U] = (uint8_t)((tdulcData[9U] & 0x0FU) << 4) | ((tdulcData[10U] >> 2) & 0x0FU);
        data[DFSI_MOT_START_LEN + 9U] = (uint8_t)((tdulcData[10U] & 0x03U)) | (tdulcData[11U] & 0x3FU);

        data[DFSI_MOT_START_LEN + 11U] = DFSI_BUSY_BITS_IDLE;
    }
}
