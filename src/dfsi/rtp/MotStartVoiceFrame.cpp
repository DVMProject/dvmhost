// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/

#include "rtp/MotStartVoiceFrame.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace dfsi;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a instance of the MotStartVoiceFrame class.
/// </summary>
MotStartVoiceFrame::MotStartVoiceFrame()
{
    icw = ICW_DIU;
    rssi = 0;
    rssiValidity = INVALID;
    nRssi = 0;
    adjMM = 0;

    startOfStream = nullptr;
    fullRateVoice = nullptr;
}

/// <summary>
/// Initializes a instance of the MotStartVoiceFrame class.
/// </summary>
/// <param name="data"></param>
MotStartVoiceFrame::MotStartVoiceFrame(uint8_t* data)
{
    decode(data);
}

/// <summary>
/// Finalizes a instance of the MotStartVoiceFrame class.
/// </summary>
MotStartVoiceFrame::~MotStartVoiceFrame()
{
    delete startOfStream;
    delete fullRateVoice;
}

/// <summary>
/// Decode a start voice frame.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool MotStartVoiceFrame::decode(const uint8_t* data)
{
    assert(data != nullptr);

    // Create a new startOfStream
    startOfStream = new MotStartOfStream();
    
    // Create a buffer to decode the start record skipping the 10th byte (adjMM)
    uint8_t startBuffer[startOfStream->LENGTH];
    ::memset(startBuffer, 0x00U, startOfStream->LENGTH);
    ::memcpy(startBuffer, data, 9U);

    // Decode start of stream
    startOfStream->decode(startBuffer);

    // Decode the full rate voice frames
    fullRateVoice = new MotFullRateVoice();
    uint8_t voiceBuffer[fullRateVoice->SHORTENED_LENGTH];
    ::memset(voiceBuffer, 0x00U, fullRateVoice->SHORTENED_LENGTH);
    voiceBuffer[0U] = data[0U];
    ::memcpy(voiceBuffer + 1U, data + 10U, fullRateVoice->SHORTENED_LENGTH - 1);
    fullRateVoice->decode(voiceBuffer, true);

    // Get rest of data
    icw = (ICWFlag)data[5U];
    rssi = data[6U];
    rssiValidity = (RssiValidityFlag)data[7U];
    nRssi = data[8U];
    adjMM = data[9U];

    return true;
}

/// <summary>
/// Encode a start voice frame.
/// </summary>
/// <param name="data"></param>
void MotStartVoiceFrame::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(startOfStream != nullptr);
    assert(fullRateVoice != nullptr);

    // Encode start of stream
    if (startOfStream != nullptr) {
        uint8_t buffer[startOfStream->LENGTH];
        startOfStream->encode(buffer);
        // Copy to data array (skipping first and last bytes)
        ::memcpy(data + 1U, buffer + 1U, startOfStream->LENGTH - 2);
    }

    // Encode full rate voice
    if (fullRateVoice != nullptr) {
        uint8_t buffer[fullRateVoice->SHORTENED_LENGTH];
        fullRateVoice->encode(buffer, true);
        data[0U] = fullRateVoice->frameType;
        ::memcpy(data + 10U, buffer + 1U, fullRateVoice->SHORTENED_LENGTH - 1);
    }

    // Copy the rest
    data[5U] = icw;
    data[6U] = rssi;
    data[7U] = rssiValidity;
    data[8U] = nRssi;
    data[9U] = adjMM;
}
