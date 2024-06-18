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

#include "rtp/MotStartOfStream.h"
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
/// Initializes a instance of the MotStartOfStream class.
/// </summary>
MotStartOfStream::MotStartOfStream()
{
    rt = DISABLED;
    startStop = START;
    streamType = VOICE;
}

/// <summary>
/// Initializes a instance of the MotStartOfStream class.
/// </summary>
/// <param name="data"></param>
MotStartOfStream::MotStartOfStream(uint8_t* data)
{
    decode(data);
}

/// <summary>
/// Decode a start of stream frame.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool MotStartOfStream::decode(const uint8_t* data)
{
    assert(data != nullptr);

    // Get parameters
    rt = (RTFlag)data[2U];
    startStop = (StartStopFlag)data[3U];
    streamType = (StreamTypeFlag)data[4U];

    return true;
}

/// <summary>
/// Encode a start of stream frame.
/// </summary>
/// <param name="data"></param>
void MotStartOfStream::encode(uint8_t* data)
{
    assert(data != nullptr);

    // Copy data
    data[0U] = P25_DFSI_MOT_START_STOP;
    data[1U] = FIXED_MARKER;
    data[2U] = rt;
    data[3U] = startStop;
    data[4U] = streamType;
}
