// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "frames/FullRateVoice.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace p25::dfsi;
using namespace p25::dfsi::defines;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a instance of the FullRateVoice class. */

FullRateVoice::FullRateVoice() :
    imbeData(nullptr),
    additionalData(nullptr),
    m_frameType(DFSIFrameType::LDU1_VOICE1),
    m_totalErrors(0U),
    m_muteFrame(false),
    m_lostFrame(false),
    m_superframeCnt(0U),
    m_busy(0U)
{
    imbeData = new uint8_t[IMBE_BUF_LEN];
    ::memset(imbeData, 0x00U, IMBE_BUF_LEN);
}

/* Initializes a instance of the FullRateVoice class. */

FullRateVoice::FullRateVoice(uint8_t* data) :
    imbeData(nullptr),
    additionalData(nullptr),
    m_frameType(DFSIFrameType::LDU1_VOICE1),
    m_totalErrors(0U),
    m_muteFrame(false),
    m_lostFrame(false),
    m_superframeCnt(0U),
    m_busy(0U)
{
    decode(data);
}

/* Finalizes a instance of the FullRateVoice class. */

FullRateVoice::~FullRateVoice()
{
    if (imbeData != nullptr)
        delete[] imbeData;
    if (additionalData != nullptr)
        delete[] additionalData;
}

/* Decode a full rate voice frame. */

bool FullRateVoice::decode(const uint8_t* data)
{
    assert(data != nullptr);

    if (imbeData != nullptr)
        delete imbeData;
    imbeData = new uint8_t[IMBE_BUF_LEN];
    ::memset(imbeData, 0x00U, IMBE_BUF_LEN);

    m_frameType = (DFSIFrameType::E)data[0U];                   // Frame Type
    ::memcpy(imbeData, data + 1U, IMBE_BUF_LEN);                // IMBE

    m_totalErrors = (uint8_t)((data[12U] >> 5) & 0x07U);        // Total Errors
    m_muteFrame = (data[12U] & 0x02U) == 0x02U;                 // Mute Frame Flag
    m_lostFrame = (data[12U] & 0x01U) == 0x01U;                 // Lost Frame Flag
    m_superframeCnt = (uint8_t)((data[13U] >> 2) & 0x03U);      // Superframe Counter
    m_busy = (uint8_t)(data[13U] & 0x03U);

    if (isVoice3thru8() || isVoice12thru17() || isVoice9or10()) {
        if (additionalData != nullptr)
            delete additionalData;
        additionalData = new uint8_t[ADDITIONAL_LENGTH];
        ::memset(additionalData, 0x00U, ADDITIONAL_LENGTH);

        if (isVoice9or10()) {
            // CAI 9 and 10 are 3 bytes of additional data not 4
            ::memcpy(additionalData, data + 14U, ADDITIONAL_LENGTH - 1U);
        } else {
            ::memcpy(additionalData, data + 14U, ADDITIONAL_LENGTH);
        }
    } else {
        if (additionalData != nullptr)
            delete additionalData;
        additionalData = nullptr;
    }

    return true;
}

/* Encode a full rate voice frame. */

void FullRateVoice::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(imbeData != nullptr);

    data[0U] = m_frameType;                                     // Frame Type
    ::memcpy(data + 1U, imbeData, IMBE_BUF_LEN);                // IMBE

    data[12U] = (uint8_t)(((m_totalErrors & 0x07U) << 5) +      // Total Errors
        (m_muteFrame ? 0x02U : 0x00U) +                         // Mute Frame Flag
        (m_lostFrame ? 0x01U : 0x00U));                         // Lost Frame Flag
    data[13U] = (uint8_t)(((m_superframeCnt & 0x03U) << 2) +    // Superframe Count
        (m_busy & 0x03U));                                      // Busy Status

    if ((isVoice3thru8() || isVoice12thru17() || isVoice9or10()) &&
        additionalData != nullptr) {
        if (isVoice9or10()) {
            // CAI 9 and 10 are 3 bytes of additional data not 4
            ::memcpy(data + 14U, additionalData, ADDITIONAL_LENGTH - 1U);
        } else {
            ::memcpy(data + 14U, additionalData, ADDITIONAL_LENGTH);
        }
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Helper indicating if the frame is voice 3 through 8. */

bool FullRateVoice::isVoice3thru8()
{
    if ( (m_frameType == DFSIFrameType::LDU1_VOICE3) || (m_frameType == DFSIFrameType::LDU1_VOICE4) || (m_frameType == DFSIFrameType::LDU1_VOICE5) || 
         (m_frameType == DFSIFrameType::LDU1_VOICE6) || (m_frameType == DFSIFrameType::LDU1_VOICE7) || (m_frameType == DFSIFrameType::LDU1_VOICE8) ) {
        return true;
    } else {
        return false;
    }
}

/* Helper indicating if the frame is voice 12 through 17. */

bool FullRateVoice::isVoice12thru17()
{
    if ( (m_frameType == DFSIFrameType::LDU2_VOICE12) || (m_frameType == DFSIFrameType::LDU2_VOICE13) || (m_frameType == DFSIFrameType::LDU2_VOICE14) ||
         (m_frameType == DFSIFrameType::LDU2_VOICE15) || (m_frameType == DFSIFrameType::LDU2_VOICE16) || (m_frameType == DFSIFrameType::LDU2_VOICE17) ) {
        return true;
    } else {
        return false;
    }
}

/* Helper indicating if the frame is voice 9 or 10. */

bool FullRateVoice::isVoice9or10()
{
    if ( (m_frameType == DFSIFrameType::LDU1_VOICE9) || (m_frameType == DFSIFrameType::LDU2_VOICE10) ) {
        return true;
    } else {
        return false;
    }
}
