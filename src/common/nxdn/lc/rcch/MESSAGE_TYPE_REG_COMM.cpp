// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "nxdn/lc/rcch/MESSAGE_TYPE_REG_COMM.h"

using namespace nxdn::lc::rcch;
using namespace nxdn::lc;
using namespace nxdn;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MESSAGE_TYPE_REG_COMM class.
/// </summary>
MESSAGE_TYPE_REG_COMM::MESSAGE_TYPE_REG_COMM() : RCCH()
{
    m_messageType = RCCH_MESSAGE_TYPE_REG_COMM;
}

/// <summary>
/// Decode layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_REG_COMM::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    RCCH::decode(data, rcch, length, offset);
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void MESSAGE_TYPE_REG_COMM::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[NXDN_RCCH_LC_LENGTH_BYTES + 4U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    rcch[2U] = (m_siteData.locId() >> 8) & 0xFFU;                                   // Location ID
    rcch[3U] = (m_siteData.locId() >> 0) & 0xFFU;                                   // ...
    rcch[4U] = (m_dstId >> 8U) & 0xFFU;                                             // Target Radio Address
    rcch[5U] = (m_dstId >> 0U) & 0xFFU;                                             // ...

    RCCH::encode(data, rcch, length, offset);
}

/// <summary>
/// Returns a string that represents the current RCCH.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MESSAGE_TYPE_REG_COMM::toString(bool isp)
{
    return std::string("RCCH_MESSAGE_TYPE_REG_COMM (Registration Command)");
}
