// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/P25Defines.h"
#include "common/p25/dfsi/frames/MotPDUFrame.h"
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

/* Initializes a instance of the MotPDUFrame class. */

MotPDUFrame::MotPDUFrame() :
    startOfStream(nullptr),
    pduHeaderData(nullptr)
{
    pduHeaderData = new uint8_t[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(pduHeaderData, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    startOfStream = new MotStartOfStream();
}

/* Initializes a instance of the MotPDUFrame class. */

MotPDUFrame::MotPDUFrame(uint8_t* data) :
    startOfStream(nullptr),
    pduHeaderData(nullptr)
{
    pduHeaderData = new uint8_t[P25_PDU_HEADER_LENGTH_BYTES];
    ::memset(pduHeaderData, 0x00U, P25_PDU_HEADER_LENGTH_BYTES);

    decode(data);
}

/* Finalizes a instance of the MotPDUFrame class. */

MotPDUFrame::~MotPDUFrame()
{
    if (startOfStream != nullptr)
        delete startOfStream;
    if (pduHeaderData != nullptr)
        delete[] pduHeaderData;
}

/* Decode a TSBK frame. */

bool MotPDUFrame::decode(const uint8_t* data)
{
    assert(data != nullptr);

    // create a new start of stream
    if (startOfStream != nullptr)
        delete startOfStream;
    startOfStream = new MotStartOfStream();

    // create a buffer to decode the start record skipping the 10th byte (adjMM)
    uint8_t startBuffer[MotStartOfStream::LENGTH];
    ::memset(startBuffer, 0x00U, MotStartOfStream::LENGTH);
    ::memcpy(startBuffer + 1U, data, 4U);

    // decode start of stream
    startOfStream->decode(startBuffer);

    ::memcpy(pduHeaderData, data + 9U, P25_PDU_HEADER_LENGTH_BYTES);

    return true;
}

/* Encode a TSBK frame. */

void MotPDUFrame::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(startOfStream != nullptr);

    // encode start of stream - scope is intentional
    {
        uint8_t buffer[MotStartOfStream::LENGTH];
        startOfStream->encode(buffer);

        // copy to data array (skipping first and last bytes)
        ::memcpy(data + 1U, buffer + 1U, 4U);
    }

    // encode TSBK - scope is intentional
    {
        data[0U] = DFSIFrameType::PDU;
        ::memcpy(data + 9U, pduHeaderData, P25_PDU_HEADER_LENGTH_BYTES);
    }
}
