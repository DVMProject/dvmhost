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
#include "common/p25/dfsi/frames/MotFullRateVoice.h"
#include "common/p25/P25Defines.h"
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

/* Initializes a instance of the MotFullRateVoice class. */

MotFullRateVoice::MotFullRateVoice() :
    imbeData(nullptr),
    additionalData(nullptr),
    m_frameType(DFSIFrameType::LDU1_VOICE1),
    m_source(SourceFlag::QUANTAR)
{
    imbeData = new uint8_t[RAW_IMBE_LENGTH_BYTES];
    ::memset(imbeData, 0x00U, RAW_IMBE_LENGTH_BYTES);
}

/* Initializes a instance of the MotFullRateVoice class. */

MotFullRateVoice::MotFullRateVoice(uint8_t* data)
{
    // set our pointers to null since it doesn't get initialized otherwise
    imbeData = nullptr;
    additionalData = nullptr;
    // decode
    decode(data);
}

/* Finalizes a instance of the MotFullRateVoice class. */

MotFullRateVoice::~MotFullRateVoice()
{
    if (imbeData != nullptr)
        delete[] imbeData;
    if (additionalData != nullptr)
        delete[] additionalData;
}

/* */

uint32_t MotFullRateVoice::size()
{
    uint32_t length = 0;

    // set length appropriately based on frame type
    if (isVoice1or2or10or11()) {
        length += SHORTENED_LENGTH;
    } else {
        length += LENGTH;
    }

    // these are weird
    if (isVoice9or18()) {
        length -= 1;
    }

    return length;
}

/* Decode a full rate voice frame. */

bool MotFullRateVoice::decode(const uint8_t* data, bool shortened)
{
    assert(data != nullptr);

    if (imbeData != nullptr)
        delete imbeData;
    imbeData = new uint8_t[RAW_IMBE_LENGTH_BYTES];
    ::memset(imbeData, 0x00U, RAW_IMBE_LENGTH_BYTES);

    m_frameType = (DFSIFrameType::E)data[0U];

    if (isVoice2or11()) {
        shortened = true;
    }
        
    if (shortened) {
        ::memcpy(imbeData, data + 1U, RAW_IMBE_LENGTH_BYTES);
        m_source = (SourceFlag::E)data[12U];
        // Forgot to set this originally and left additionalData uninitialized, whoops!
        additionalData = nullptr;
    } else {
        // Frames 0x6A and 0x73 are missing the 0x00 padding byte, so we start IMBE data 1 byte earlier
        uint8_t imbeStart = 5U;
        if (isVoice9or18()) {
            imbeStart = 4U;
        }

        if (additionalData != nullptr)
            delete[] additionalData;
        additionalData = new uint8_t[ADDITIONAL_LENGTH];
        ::memset(additionalData, 0x00U, ADDITIONAL_LENGTH);
        ::memcpy(additionalData, data + 1U, ADDITIONAL_LENGTH);

        // copy IMBE data based on our imbe start position
        ::memcpy(imbeData, data + imbeStart, RAW_IMBE_LENGTH_BYTES);

        m_source = (SourceFlag::E)data[RAW_IMBE_LENGTH_BYTES + imbeStart];
    }

    return true;
}

/* Encode a full rate voice frame. */

void MotFullRateVoice::encode(uint8_t* data, bool shortened)
{
    assert(data != nullptr);
    assert(imbeData != nullptr);

    // check if we're a shortened frame
    data[0U] = m_frameType;
    if (isVoice2or11()) {
        shortened = true;
    }

    // copy based on shortened frame or not
    if (shortened) {
        ::memcpy(data + 1U, imbeData, RAW_IMBE_LENGTH_BYTES);
        data[12U] = (uint8_t)m_source;
    } 
    // if not shortened, our IMBE data start position depends on frame type
    else {
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
        ::memcpy(data + imbeStart, imbeData, RAW_IMBE_LENGTH_BYTES);

        // Source byte at the end
        data[11U + imbeStart] = (uint8_t)m_source;
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper indicating if the frame is voice 1, 2, 10 or 11. */

bool MotFullRateVoice::isVoice1or2or10or11()
{
    if ( (m_frameType == DFSIFrameType::LDU1_VOICE1) || (m_frameType == DFSIFrameType::LDU1_VOICE2) || 
         (m_frameType == DFSIFrameType::LDU2_VOICE10) || (m_frameType == DFSIFrameType::LDU2_VOICE11) ) {
        return true;
    } else {
        return false;
    }
}

/* Helper indicating if the frame is voice 2 or 11. */

bool MotFullRateVoice::isVoice2or11()
{
    if ( (m_frameType == DFSIFrameType::LDU1_VOICE2) || (m_frameType == DFSIFrameType::LDU2_VOICE11) ) {
        return true;
    } else {
        return false;
    }
}

/* Helper indicating if the frame is voice 9 or 18. */

bool MotFullRateVoice::isVoice9or18()
{
    if ( (m_frameType == DFSIFrameType::LDU1_VOICE9) || (m_frameType == DFSIFrameType::LDU2_VOICE18) ) {
        return true;
    } else {
        return false;
    }
}
