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

#include "frames/MotVoiceHeader1.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace p25::dfsi;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a instance of the MotVoiceHeader1 class.
/// </summary>
MotVoiceHeader1::MotVoiceHeader1() :
    header(nullptr),
    startOfStream(nullptr),
    m_icw(ICW_DIU),
    m_rssi(0U),
    m_rssiValidity(INVALID),
    m_nRssi(0U)
{
    startOfStream = new MotStartOfStream();

    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
}

/// <summary>
/// Initializes a instance of the MotVoiceHeader1 class.
/// </summary>
/// <param name="data"></param>
MotVoiceHeader1::MotVoiceHeader1(uint8_t* data) :
    header(nullptr),
    startOfStream(nullptr),
    m_icw(ICW_DIU),
    m_rssi(0U),
    m_rssiValidity(INVALID),
    m_nRssi(0U)
{
    decode(data);
}

/// <summary>
/// Finalizes a instance of the MotVoiceHeader1 class.
/// </summary>
MotVoiceHeader1::~MotVoiceHeader1()
{
    if (startOfStream != nullptr)
        delete startOfStream;
    if (header != nullptr)
        delete[] header;
}

/// <summary>
/// Decode a voice header 1 frame.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
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
    m_icw = (ICWFlag)data[5U];
    m_rssi = data[6U];
    m_rssiValidity = (RssiValidityFlag)data[7U];
    m_nRssi = data[8U];

    // our header includes the trailing source and check bytes
    if (header != nullptr)
        delete[] header;
    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
    ::memcpy(header, data + 9U, HCW_LENGTH);

    return true;
}

/// <summary>
/// Encode a voice header 1 frame.
/// </summary>
/// <param name="data"></param>
void MotVoiceHeader1::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(startOfStream != nullptr);

    data[0U] = P25_DFSI_MOT_VHDR_1;

    // scope is intentional
    {
        uint8_t buffer[MotStartOfStream::LENGTH];
        ::memset(buffer, 0x00U, MotStartOfStream::LENGTH);
        startOfStream->encode(buffer);

        // copy the 4 start record bytes from the start of stream frame
        ::memcpy(data + 1U, buffer + 1U, 4U);
    }

    data[5U] = (uint8_t)m_icw;
    data[6U] = m_rssi;
    data[7U] = (uint8_t)m_rssiValidity;
    data[8U] = m_nRssi;

    // our header includes the trailing source and check bytes
    if (header != nullptr) {
        ::memcpy(data + 9U, header, HCW_LENGTH);
    }
}
