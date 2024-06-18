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
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/

#include "frames/StartOfStream.h"
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
/// Initializes a instance of the StartOfStream class.
/// </summary>
StartOfStream::StartOfStream() :
    m_nid(0U),
    m_errorCount(0U)
{
    /* stub */
}

/// <summary>
/// Initializes a instance of the StartOfStream class.
/// </summary>
/// <param name="data"></param>
StartOfStream::StartOfStream(uint8_t* data) :
    m_nid(0U),
    m_errorCount(0U)
{
    decode(data);
}

/// <summary>
/// Decode a start of stream frame.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool StartOfStream::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_nid = __GET_UINT16(data, 0U);         // Network Identifier
    m_errorCount = (data[2U] & 0x0FU);      // Error Count

    return true;
}

/// <summary>
/// Encode a start of stream frame.
/// </summary>
/// <param name="data"></param>
void StartOfStream::encode(uint8_t* data)
{
    assert(data != nullptr);

    __SET_UINT16(m_nid, data, 0U);          // Network Identifier
    data[2U] = m_errorCount & 0x0FU;        // Error Count
}
