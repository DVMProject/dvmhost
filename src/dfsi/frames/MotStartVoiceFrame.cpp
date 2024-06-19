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
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/

#include "frames/MotStartVoiceFrame.h"
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
MotStartVoiceFrame::MotStartVoiceFrame() :
    startOfStream(nullptr),
    fullRateVoice(nullptr),
    m_icw(ICW_DIU),
    m_rssi(0U),
    m_rssiValidity(INVALID),
    m_nRssi(0U),
    m_adjMM(0U)
{
    startOfStream = new MotStartOfStream();
    fullRateVoice = new MotFullRateVoice();
}

/// <summary>
/// Initializes a instance of the MotStartVoiceFrame class.
/// </summary>
/// <param name="data"></param>
MotStartVoiceFrame::MotStartVoiceFrame(uint8_t* data) :
    startOfStream(nullptr),
    fullRateVoice(nullptr),
    m_icw(ICW_DIU),
    m_rssi(0U),
    m_rssiValidity(INVALID),
    m_nRssi(0U),
    m_adjMM(0U)
{
    decode(data);
}

/// <summary>
/// Finalizes a instance of the MotStartVoiceFrame class.
/// </summary>
MotStartVoiceFrame::~MotStartVoiceFrame()
{
    if (startOfStream != nullptr)
        delete startOfStream;
    if (fullRateVoice != nullptr)
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

    // create a new start of stream
    if (startOfStream != nullptr)
        delete startOfStream;
    startOfStream = new MotStartOfStream();

    // create a buffer to decode the start record skipping the 10th byte (adjMM)
    uint8_t startBuffer[MotStartOfStream::LENGTH];
    ::memset(startBuffer, 0x00U, MotStartOfStream::LENGTH);
    ::memcpy(startBuffer, data, 9U);

    // decode start of stream
    startOfStream->decode(startBuffer);

    // decode the full rate voice frames
    if (fullRateVoice != nullptr)
        delete fullRateVoice;
    fullRateVoice = new MotFullRateVoice();
    
    uint8_t voiceBuffer[MotFullRateVoice::SHORTENED_LENGTH];
    ::memset(voiceBuffer, 0x00U, MotFullRateVoice::SHORTENED_LENGTH);
    voiceBuffer[0U] = data[0U];
    ::memcpy(voiceBuffer + 1U, data + 10U, MotFullRateVoice::SHORTENED_LENGTH - 1);
    fullRateVoice->decode(voiceBuffer, true);

    // get rest of data
    m_icw = (ICWFlag)data[5U];
    m_rssi = data[6U];
    m_rssiValidity = (RssiValidityFlag)data[7U];
    m_nRssi = data[8U];
    m_adjMM = data[9U];

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

    // encode start of stream - scope is intentional
    {
        uint8_t buffer[MotStartOfStream::LENGTH];
        startOfStream->encode(buffer);

        // copy to data array (skipping first and last bytes)
        ::memcpy(data + 1U, buffer + 1U, MotStartOfStream::LENGTH - 2);
    }

    // encode full rate voice - scope is intentional
    {
        uint8_t buffer[MotFullRateVoice::SHORTENED_LENGTH];
        fullRateVoice->encode(buffer, true);

        data[0U] = fullRateVoice->getFrameType();
        ::memcpy(data + 10U, buffer + 1U, MotFullRateVoice::SHORTENED_LENGTH - 1);
    }

    // Copy the rest
    data[5U] = (uint8_t)m_icw;
    data[6U] = m_rssi;
    data[7U] = (uint8_t)m_rssiValidity;
    data[8U] = m_nRssi;
    data[9U] = m_adjMM;
}
