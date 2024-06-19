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

#include "frames/ControlOctet.h"
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
/// Initializes a instance of the ControlOctet class.
/// </summary>
ControlOctet::ControlOctet() :
    m_signal(false),
    m_compact(true),
    m_blockHeaderCnt(0U)
{
    /* stub */
}

/// <summary>
/// Initializes a instance of the ControlOctet class.
/// </summary>
/// <param name="data"></param>
ControlOctet::ControlOctet(uint8_t* data) :
    m_signal(false),
    m_compact(true),
    m_blockHeaderCnt(0U)
{
    decode(data);
}

/// <summary>
/// Decode a control octet frame.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool ControlOctet::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_signal = (data[0U] & 0x07U) == 0x07U;                     // Signal Flag
    m_compact = (data[0U] & 0x06U) == 0x06U;                    // Compact Flag
    m_blockHeaderCnt = (uint8_t)(data[0U] & 0x3FU);             // Block Header Count

    return true;
}

/// <summary>
/// Encode a control octet frame.
/// </summary>
/// <param name="data"></param>
void ControlOctet::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = (uint8_t)((m_signal ? 0x07U : 0x00U) +           // Signal Flag
            (m_compact ? 0x06U : 0x00U) +                       // Control Flag
            (m_blockHeaderCnt & 0x3F));
}
