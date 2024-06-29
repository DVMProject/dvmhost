// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "nxdn/lc/rcch/RCCHFactory.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::defines;
using namespace nxdn::lc;
using namespace nxdn::lc::rcch;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the RCCHFactory class. */
RCCHFactory::RCCHFactory() = default;

/* Finalizes a instance of RCCHFactory class. */
RCCHFactory::~RCCHFactory() = default;

/* Create an instance of a RCCH. */
std::unique_ptr<RCCH> RCCHFactory::createRCCH(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != nullptr);

    uint8_t messageType = data[0U] & 0x3FU;                                         // Message Type

    // message type opcodes
    switch (messageType) {
    case MessageType::RTCH_VCALL:
    case MessageType::RCCH_VCALL_CONN:
        return decode(new MESSAGE_TYPE_VCALL_CONN(), data, length, offset);
    case MessageType::RTCH_DCALL_HDR:
        return decode(new MESSAGE_TYPE_DCALL_HDR(), data, length, offset);
    case MessageType::IDLE:
        return decode(new MESSAGE_TYPE_IDLE(), data, length, offset);
    case MessageType::RCCH_REG:
        return decode(new MESSAGE_TYPE_REG(), data, length, offset);
    case MessageType::RCCH_REG_C:
        return decode(new MESSAGE_TYPE_REG_C(), data, length, offset);
    case MessageType::RCCH_GRP_REG:
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

/* Internal helper to decode a RCCH link control message. */
std::unique_ptr<RCCH> RCCHFactory::decode(RCCH* rcch, const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(rcch != nullptr);
    assert(data != nullptr);

    rcch->decode(data, length, offset);
    return std::unique_ptr<RCCH>(rcch);
}
