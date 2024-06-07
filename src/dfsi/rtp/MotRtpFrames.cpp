// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/

#include "rtp/MotRtpFrames.h"
#include "common/p25/dfsi/DFSIDefines.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace dfsi;

// ---------------------------------------------------------------------------
//  Motorola full rate voice data
// ---------------------------------------------------------------------------

MotFullRateVoice::MotFullRateVoice() 
{
    frameType = P25_DFSI_LDU1_VOICE1;
    additionalData = nullptr;
    source = 0x02U;
    imbeData = new uint8_t[IMBE_BUF_LEN];
}

MotFullRateVoice::MotFullRateVoice(uint8_t* data)
{
    decode(data);
}

MotFullRateVoice::~MotFullRateVoice()
{
    delete[] imbeData;
    delete[] additionalData;
}

uint32_t MotFullRateVoice::size()
{
    uint32_t length = 0;

    // Set length appropriately based on frame type
    if (isVoice1or2or10or11()) {
        length += SHORTENED_LENGTH;
    } else {
        length += LENGTH;
    }

    // We're one byte short on these, for some reason, so add it here
    if (isVoice2or11()) {
        length += 1;
    }

    // These are weird
    if (isVoice9or18()) {
        length -= 1;
    }

    return length;
}

/// <summary>Decode a block of bytes into a full rate voice IMBE block</summary>
bool MotFullRateVoice::decode(uint8_t* data, bool shortened)
{
    // Sanity check
    assert(data != nullptr);

    imbeData = new uint8_t[IMBE_BUF_LEN];

    frameType = data[0U] & 0xFFU;

    if (isVoice2or11())
        shortened = true;

    if (shortened) {
        for (int i = 0; i < IMBE_BUF_LEN; i++) {
            imbeData[i] = data[i + 1U];
        }
        source = data[12U];
        // Forgot to set this originally and left additionalData uninitialized, whoops!
        additionalData = nullptr;
    } else {
        // Frames 0x6A and 0x73 are missing the 0x00 padding byte, so we start IMBE data 1 byte earlier
        uint8_t imbeStart = 5U;
        if (isVoice9or18()) {
            imbeStart = 4U;
        }

        additionalData = new uint8_t[ADDITIONAL_LENGTH];
        ::memset(additionalData, 0x00U, ADDITIONAL_LENGTH);
        ::memcpy(additionalData, data + 1U, ADDITIONAL_LENGTH);

        for (int i = 0; i < IMBE_BUF_LEN; i++) {
            imbeData[i] = data[i + imbeStart];
        }

        source = data[11 + imbeStart];
    }

    return true;
}

void MotFullRateVoice::encode(uint8_t data[], bool shortened)
{
    // Sanity check
    assert(data != nullptr);

    // Check if we're a shortened frame
    data[0U] = frameType;
    if (isVoice9or18()) {
        shortened = true;
    }

    // Copy based on shortened frame or not
    if (shortened) {
        for (int i = 0; i < IMBE_BUF_LEN; i++)
            data[i + 1U] = imbeData[i];
        ::memcpy(data + 1U, imbeData, IMBE_BUF_LEN);
    } else {
        // Starting index for the IMBE data
        uint8_t imbeStart = 5U;
        if (isVoice9or18()) {
            imbeStart = 4U;
        }

        // Check if we have additional data
        if (additionalData != nullptr) {
            ::memcpy(data + 1U, additionalData, ADDITIONAL_LENGTH);
        }

        // Copy rest of data
        ::memcpy(data + imbeStart, imbeData, IMBE_BUF_LEN);

        // Source byte at the end
        data[11U + imbeStart] = source;
    }

}

bool MotFullRateVoice::isVoice1or2or10or11()
{
    if (frameType == P25_DFSI_LDU1_VOICE1 || frameType == P25_DFSI_LDU1_VOICE2 || frameType == P25_DFSI_LDU2_VOICE10 || frameType == P25_DFSI_LDU2_VOICE11 ) {
        return true;
    } else {
        return false;
    }
}

bool MotFullRateVoice::isVoice2or11()
{
    if (frameType == P25_DFSI_LDU1_VOICE2 || frameType == P25_DFSI_LDU2_VOICE11) {
        return true;
    } else {
        return false;
    }
}

bool MotFullRateVoice::isVoice9or18()
{
    if (frameType == P25_DFSI_LDU1_VOICE9 || frameType == P25_DFSI_LDU2_VOICE18) {
        return true;
    } else {
        return false;
    }
}

// ---------------------------------------------------------------------------
//  Motorola start of stream frame
// ---------------------------------------------------------------------------

MotStartOfStream::MotStartOfStream()
{
    rt = DISABLED;
    startStop = START;
    streamType = VOICE;
}

MotStartOfStream::MotStartOfStream(uint8_t* data)
{
    decode(data);
}

bool MotStartOfStream::decode(uint8_t* data)
{
    // Sanity check
    assert(data != nullptr);

    // Get parameters
    rt = (RTFlag)data[2U];
    startStop = (StartStopFlag)data[3U];
    streamType = (StreamTypeFlag)data[4U];

    return true;
}

void MotStartOfStream::encode(uint8_t data[])
{
    // Sanity check
    assert(data != nullptr);

    // Copy data
    data[0U] = P25_DFSI_MOT_START_STOP;
    data[1U] = FIXED_MARKER;
    data[2U] = (uint8_t)rt;
    data[3U] = (uint8_t)startStop;
    data[4U] = (uint8_t)streamType;
}

// ---------------------------------------------------------------------------
//  Motorola start voice frame
// ---------------------------------------------------------------------------

MotStartVoiceFrame::MotStartVoiceFrame()
{
    icw = 0;
    rssi = 0;
    rssiValidity = INVALID;
    adjMM = 0;

    startOfStream = nullptr;
    fullRateVoice = nullptr;
}

MotStartVoiceFrame::MotStartVoiceFrame(uint8_t* data)
{
    decode(data);
}

MotStartVoiceFrame::~MotStartVoiceFrame()
{
    delete startOfStream;
    delete fullRateVoice;
}

bool MotStartVoiceFrame::decode(uint8_t* data)
{
    assert(data != nullptr);

    // Create a new startOfStream
    startOfStream = new MotStartOfStream();
    
    // Create a buffer to decode
    uint8_t startBuffer[startOfStream->LENGTH];
    ::memset(startBuffer, 0x00U, startOfStream->LENGTH);
    ::memcpy(startBuffer, data + 1U, 4);

    // Decode start of stream
    startOfStream->decode(startBuffer);

    // Decode the full rate voice the same way
    fullRateVoice = new MotFullRateVoice();
    uint8_t voiceBuffer[fullRateVoice->SHORTENED_LENGTH];
    ::memset(voiceBuffer, 0x00U, fullRateVoice->SHORTENED_LENGTH);
    voiceBuffer[0U] = data[0U];
    ::memcpy(voiceBuffer + 1U, data + 10U, fullRateVoice->SHORTENED_LENGTH - 2);
    fullRateVoice->decode(voiceBuffer);

    // Get rest of data
    icw = data[5U];
    rssi = data[6U];
    rssiValidity = (RssiValidityFlag)data[7U];
    adjMM = data[9U];

    return true;
}

void MotStartVoiceFrame::encode(uint8_t data[])
{
    // Sanity checks
    assert(data != nullptr);
    assert(startOfStream != nullptr);
    assert(fullRateVoice != nullptr);

    // Encode start of stream
    if (startOfStream != nullptr) {
        uint8_t buffer[startOfStream->LENGTH];
        startOfStream->encode(buffer);
        // Copy to data array
        ::memcpy(data + 1U, buffer + 1U, startOfStream->LENGTH - 1);
    }

    // Encode full rate voice
    if (fullRateVoice != nullptr) {
        uint8_t buffer[fullRateVoice->SHORTENED_LENGTH];
        fullRateVoice->encode(buffer);
        data[0U] = fullRateVoice->frameType;
        ::memcpy(data + 10U, buffer + 1U, fullRateVoice->SHORTENED_LENGTH - 1);
    }

    // Copy the rest
    data[5U] = icw;
    data[6U] = rssi;
    data[7U] = (uint8_t)rssiValidity;
    data[8U] = rssi;
    data[9U] = adjMM;
}

// ---------------------------------------------------------------------------
//  Motorola voice header 1
// ---------------------------------------------------------------------------

MotVoiceHeader1::MotVoiceHeader1()
{
    icw = 0;
    rssi = 0;
    rssiValidity = INVALID;
    source = 0x02U;

    startOfStream = nullptr;
    header = new uint8_t[HCW_LENGTH];
}

MotVoiceHeader1::MotVoiceHeader1(uint8_t* data)
{
    decode(data);
}

MotVoiceHeader1::~MotVoiceHeader1()
{
    delete startOfStream;
    delete[] header;
}

bool MotVoiceHeader1::decode(uint8_t* data)
{
    assert(data != nullptr);

    // Create a start of stream
    startOfStream = new MotStartOfStream();
    uint8_t buffer[startOfStream->LENGTH];
    ::memset(buffer, 0x00U, startOfStream->LENGTH);
    ::memcpy(buffer, data + 1U, 4);
    startOfStream->decode(buffer);

    // Decode the other stuff
    icw = data[5U];
    rssi = data[6U];
    rssiValidity = (RssiValidityFlag)data[7U];
    source = data[29];

    header = new uint8_t[HCW_LENGTH];
    ::memcpy(header, data + 10U, HCW_LENGTH);

    return true;
}

void MotVoiceHeader1::encode(uint8_t data[])
{
    assert(data != nullptr);
    assert(startOfStream != nullptr);

    data[0U] = P25_DFSI_MOT_VHDR_1;

    if (startOfStream != nullptr) {
        uint8_t buffer[startOfStream->LENGTH];
        ::memset(buffer, 0x00U, startOfStream->LENGTH);
        startOfStream->encode(buffer);
        ::memcpy(data + 1U, buffer + 1U, startOfStream->LENGTH - 1);
    }

    data[5U] = icw;
    data[6U] = rssi;
    data[7U] = (uint8_t)rssiValidity;
    data[8U] = rssi;

    if (header != nullptr) {
        ::memcpy(data + 9U, header, HCW_LENGTH);
    }

    data[LENGTH - 1] = source;
}

// ---------------------------------------------------------------------------
//  Motorola voice header 2
// ---------------------------------------------------------------------------

MotVoiceHeader2::MotVoiceHeader2()
{
    source = 0x02U;

    header = new uint8_t[HCW_LENGTH];
}

MotVoiceHeader2::MotVoiceHeader2(uint8_t* data)
{
    decode(data);
}

MotVoiceHeader2::~MotVoiceHeader2()
{
    delete[] header;
}

bool MotVoiceHeader2::decode(uint8_t* data)
{
    assert(data != nullptr);

    source = data[21];

    header = new uint8_t[HCW_LENGTH];
    ::memcpy(header, data + 1U, HCW_LENGTH);

    return true;
}

void MotVoiceHeader2::encode(uint8_t data[])
{
    assert(data != nullptr);

    data[0U] = P25_DFSI_MOT_VHDR_2;

    if (header != nullptr) {
        ::memcpy(data + 1U, header, HCW_LENGTH);
    }

    data[LENGTH - 1U] = source;
}