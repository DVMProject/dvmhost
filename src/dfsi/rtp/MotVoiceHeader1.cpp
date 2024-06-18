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

#include "rtp/MotVoiceHeader1.h"
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
/// Initializes a instance of the MotVoiceHeader1 class.
/// </summary>
MotVoiceHeader1::MotVoiceHeader1()
{
    icw = ICW_DIU;
    rssi = 0;
    rssiValidity = INVALID;
    nRssi = 0;

    startOfStream = nullptr;
    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
}

/// <summary>
/// Initializes a instance of the MotVoiceHeader1 class.
/// </summary>
/// <param name="data"></param>
MotVoiceHeader1::MotVoiceHeader1(uint8_t* data)
{
    decode(data);
}

/// <summary>
/// Finalizes a instance of the MotVoiceHeader1 class.
/// </summary>
MotVoiceHeader1::~MotVoiceHeader1()
{
    delete startOfStream;
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

    // Create a start of stream
    startOfStream = new MotStartOfStream();
    uint8_t buffer[startOfStream->LENGTH];
    ::memset(buffer, 0x00U, startOfStream->LENGTH);
    // We copy the bytes from [1:4] 
    ::memcpy(buffer + 1U, data + 1U, 4);
    startOfStream->decode(buffer);

    // Decode the other stuff
    icw = (ICWFlag)data[5U];
    rssi = data[6U];
    rssiValidity = (RssiValidityFlag)data[7U];
    nRssi = data[8U];

    // Our header includes the trailing source and check bytes
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

    if (startOfStream != nullptr) {
        uint8_t buffer[startOfStream->LENGTH];
        ::memset(buffer, 0x00U, startOfStream->LENGTH);
        startOfStream->encode(buffer);
        // Copy the 4 start record bytes from the start of stream frame
        ::memcpy(data + 1U, buffer + 1U, 4U);
    }

    data[5U] = icw;
    data[6U] = rssi;
    data[7U] = (uint8_t)rssiValidity;
    data[8U] = nRssi;

    // Our header includes the trailing source and check bytes
    if (header != nullptr) {
        ::memcpy(data + 9U, header, HCW_LENGTH);
    }
}
