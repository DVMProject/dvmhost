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

#include "frames/fsc/FSCConnectResponse.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25::dfsi;
using namespace p25::dfsi::fsc;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a instance of the FSCConnectResponse class.
/// </summary>
FSCConnectResponse::FSCConnectResponse() : FSCResponse(),
    m_vcBasePort(0U)
{
    /* stub */
}

/// <summary>
/// Initializes a instance of the FSCConnectResponse class.
/// </summary>
/// <param name="data"></param>
FSCConnectResponse::FSCConnectResponse(uint8_t* data) : FSCResponse(data),
    m_vcBasePort(0U)
{
    decode(data);
}

/// <summary>
/// Decode a FSC connect frame.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool FSCConnectResponse::decode(const uint8_t* data)
{
    assert(data != nullptr);
    FSCResponse::decode(data);

    m_vcBasePort = __GET_UINT16B(data, 1U);                     // Voice Conveyance RTP Port

    return true;
}

/// <summary>
/// Encode a FSC connect frame.
/// </summary>
/// <param name="data"></param>
void FSCConnectResponse::encode(uint8_t* data)
{
    assert(data != nullptr);
    FSCResponse::encode(data);

    __SET_UINT16B(m_vcBasePort, data, 1U);                      // Voice Conveyance RTP Port
}
