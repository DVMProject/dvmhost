/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "nxdn/lc/rcch/RCCHFactory.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn::lc::rcch;
using namespace nxdn::lc;
using namespace nxdn;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the RCCHFactory class.
/// </summary>
RCCHFactory::RCCHFactory()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of RCCHFactory class.
/// </summary>
RCCHFactory::~RCCHFactory()
{
    /* stub */
}

/// <summary>
/// Create an instance of a RCCH.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
/// <returns>True, if RCCH was decoded, otherwise false.</returns>
std::unique_ptr<RCCH> RCCHFactory::createRCCH(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t rcch[22U];
    ::memset(rcch, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES + 4U);

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(data, offset);
        WRITE_BIT(rcch, i, b);
    }

    uint8_t messageType = data[0U] & 0x3FU;                                         // Message Type

    // message type opcodes
    switch (messageType) {
    case RTCH_MESSAGE_TYPE_VCALL:
    case RCCH_MESSAGE_TYPE_VCALL_CONN:
        return decode(new MESSAGE_TYPE_VCALL_CONN(), data, length, offset);
    case RTCH_MESSAGE_TYPE_DCALL_HDR:
        return decode(new MESSAGE_TYPE_DCALL_HDR(), data, length, offset);
    case nxdn::MESSAGE_TYPE_IDLE:
        return decode(new MESSAGE_TYPE_IDLE(), data, length, offset);
    case RCCH_MESSAGE_TYPE_REG:
        return decode(new MESSAGE_TYPE_REG(), data, length, offset);
    case RCCH_MESSAGE_TYPE_REG_C:
        return decode(new MESSAGE_TYPE_REG_C(), data, length, offset);
    case RCCH_MESSAGE_TYPE_GRP_REG:
        return decode(new MESSAGE_TYPE_GRP_REG(), data, length, offset);
    default:
        LogError(LOG_NXDN, "RCCH::decodeRCCH(), unknown RCCH value, messageType = $%02X", messageType);
        return nullptr;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="rcch"></param>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
/// <returns></returns>
std::unique_ptr<RCCH> RCCHFactory::decode(RCCH* rcch, const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(rcch != nullptr);
    assert(data != nullptr);

    rcch->decode(data, length, offset);
    return std::unique_ptr<RCCH>(rcch);
}
