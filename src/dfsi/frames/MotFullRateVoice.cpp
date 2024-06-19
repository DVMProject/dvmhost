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

#include "frames/MotFullRateVoice.h"
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
/// Initializes a instance of the MotFullRateVoice class.
/// </summary>
MotFullRateVoice::MotFullRateVoice() :
    imbeData(nullptr),
    additionalData(nullptr),
    m_frameType(P25_DFSI_LDU1_VOICE1),
    m_source(SOURCE_QUANTAR)
{
    imbeData = new uint8_t[IMBE_BUF_LEN];
    ::memset(imbeData, 0x00U, IMBE_BUF_LEN);
}

/// <summary>
/// Initializes a instance of the MotFullRateVoice class.
/// </summary>
/// <param name="data"></param>
MotFullRateVoice::MotFullRateVoice(uint8_t* data)
{
    // set our pointers to null since it doesn't get initialized otherwise
    imbeData = nullptr;
    additionalData = nullptr;
    // decode
    decode(data);
}

/// <summary>
/// Finalizes a instance of the MotFullRateVoice class.
/// </summary>
MotFullRateVoice::~MotFullRateVoice()
{
    if (imbeData != nullptr)
        delete[] imbeData;
    if (additionalData != nullptr)
        delete[] additionalData;
}

/// <summary>
///
/// </summary>
/// <returns></returns>
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

/// <summary>
/// Decode a full rate voice frame.
/// </summary>
/// <param name="data"></param>
/// <param name="shortened"></param>
/// <returns></returns>
bool MotFullRateVoice::decode(const uint8_t* data, bool shortened)
{
    assert(data != nullptr);

    if (imbeData != nullptr)
        delete imbeData;
    imbeData = new uint8_t[IMBE_BUF_LEN];
    ::memset(imbeData, 0x00U, IMBE_BUF_LEN);

    m_frameType = data[0U];

    if (isVoice2or11()) {
        shortened = true;
    }
        
    if (shortened) {
        ::memcpy(imbeData, data + 1U, IMBE_BUF_LEN);
        m_source = (SourceFlag)data[12U];
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
        ::memcpy(imbeData, data + imbeStart, IMBE_BUF_LEN);

        m_source = (SourceFlag)data[IMBE_BUF_LEN + imbeStart];
    }

    return true;
}

/// <summary>
/// Encode a full rate voice frame.
/// </summary>
/// <param name="data"></param>
/// <param name="shortened"></param>
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
        ::memcpy(data + 1U, imbeData, IMBE_BUF_LEN);
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
        ::memcpy(data + imbeStart, imbeData, IMBE_BUF_LEN);

        // Source byte at the end
        data[11U + imbeStart] = (uint8_t)m_source;
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <returns></returns>
bool MotFullRateVoice::isVoice1or2or10or11()
{
    if ( (m_frameType == P25_DFSI_LDU1_VOICE1) || (m_frameType == P25_DFSI_LDU1_VOICE2) || (m_frameType == P25_DFSI_LDU2_VOICE10) || (m_frameType == P25_DFSI_LDU2_VOICE11) ) {
        return true;
    } else {
        return false;
    }
}

/// <summary>
///
/// </summary>
/// <returns></returns>
bool MotFullRateVoice::isVoice2or11()
{
    if ( (m_frameType == P25_DFSI_LDU1_VOICE2) || (m_frameType == P25_DFSI_LDU2_VOICE11) ) {
        return true;
    } else {
        return false;
    }
}

/// <summary>
///
/// </summary>
/// <returns></returns>
bool MotFullRateVoice::isVoice9or18()
{
    if ( (m_frameType == P25_DFSI_LDU1_VOICE9) || (m_frameType == P25_DFSI_LDU2_VOICE18) ) {
        return true;
    } else {
        return false;
    }
}
