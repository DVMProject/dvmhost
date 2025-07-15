// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/MotVoiceHeader1.h"
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

/* Initializes a instance of the MotVoiceHeader1 class. */

MotVoiceHeader1::MotVoiceHeader1() :
    header(nullptr),
    startOfStream(nullptr),
    m_icw(ICWFlag::DIU),
    m_rssiValidity(RssiValidityFlag::INVALID),
    m_rssi(0U)
{
    startOfStream = new MotStartOfStream();

    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
}

/* Initializes a instance of the MotVoiceHeader1 class. */

MotVoiceHeader1::MotVoiceHeader1(uint8_t* data) :
    header(nullptr),
    startOfStream(nullptr),
    m_icw(ICWFlag::DIU),
    m_rssiValidity(RssiValidityFlag::INVALID),
    m_rssi(0U)
{
    decode(data);
}

/* Finalizes a instance of the MotVoiceHeader1 class. */

MotVoiceHeader1::~MotVoiceHeader1()
{
    if (startOfStream != nullptr)
        delete startOfStream;
    if (header != nullptr)
        delete[] header;
}

/* Decode a voice header 1 frame. */

bool MotVoiceHeader1::decode(const uint8_t* data)
{
    assert(data != nullptr);

    // create a start of stream
    if (startOfStream != nullptr)
        delete startOfStream;
    startOfStream = new MotStartOfStream();

    uint8_t buffer[MotStartOfStream::LENGTH];
    ::memset(buffer, 0x00U, MotStartOfStream::LENGTH);
    
    // we copy the bytes from [1:4]
    ::memcpy(buffer + 1U, data + 1U, 4);
    startOfStream->decode(buffer);

    // decode the other stuff
    m_icw = (ICWFlag::E)data[5U];                       // this field is dubious and questionable
    //data[6U];                                         // unknown -- based on testing this is not related to RSSI
    m_rssiValidity = (RssiValidityFlag::E)data[7U];     // this field is dubious and questionable

    m_rssi = data[8U];

    // our header includes the trailing source and check bytes
    if (header != nullptr)
        delete[] header;
    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
    ::memcpy(header, data + 9U, HCW_LENGTH);

    return true;
}

/* Encode a voice header 1 frame. */

void MotVoiceHeader1::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(startOfStream != nullptr);

    data[0U] = DFSIFrameType::MOT_VHDR_1;

    // scope is intentional
    {
        uint8_t buffer[MotStartOfStream::LENGTH];
        ::memset(buffer, 0x00U, MotStartOfStream::LENGTH);
        startOfStream->encode(buffer);

        // copy the 4 start record bytes from the start of stream frame
        ::memcpy(data + 1U, buffer + 1U, 4U);
    }

    data[5U] = m_icw;                                   // this field is dubious and questionable
    data[6U] = 0U;                                      // unknown -- based on testing this is not related to RSSI
    data[7U] = m_rssiValidity;                          // this field is dubious and questionable

    data[8U] = m_rssi;

    // our header includes the trailing source and check bytes
    if (header != nullptr) {
        ::memcpy(data + 9U, header, HCW_LENGTH);
    }
}
