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

#include "frames/MotVoiceHeader2.h"
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
/// Initializes a instance of the MotVoiceHeader2 class.
/// </summary>
MotVoiceHeader2::MotVoiceHeader2() :
    header(nullptr),
    m_source(SOURCE_QUANTAR)
{
    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
}

/// <summary>
/// Initializes a instance of the MotVoiceHeader2 class.
/// </summary>
/// <param name="data"></param>
MotVoiceHeader2::MotVoiceHeader2(uint8_t* data) :
    header(nullptr),
    m_source(SOURCE_QUANTAR)
{
    decode(data);
}

/// <summary>
/// Finalizes a instance of the MotVoiceHeader2 class.
/// </summary>
MotVoiceHeader2::~MotVoiceHeader2()
{
    if (header != nullptr)
        delete[] header;
}

/// <summary>
/// Decode a voice header 2 frame.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool MotVoiceHeader2::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_source = (SourceFlag)data[21];

    if (header != nullptr) {
        delete[] header;
    }

    header = new uint8_t[HCW_LENGTH];
    ::memset(header, 0x00U, HCW_LENGTH);
    ::memcpy(header, data + 1U, HCW_LENGTH);

    return true;
}

/// <summary>
/// Encode a voice header 2 frame.
/// </summary>
/// <param name="data"></param>
void MotVoiceHeader2::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = P25_DFSI_MOT_VHDR_2;

    if (header != nullptr) {
        ::memcpy(data + 1U, header, HCW_LENGTH);
    }

    data[LENGTH - 1U] = (uint8_t)m_source;
}
