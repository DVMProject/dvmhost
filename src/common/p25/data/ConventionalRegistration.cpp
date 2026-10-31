// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/data/ConventionalRegistration.h"
#include "common/p25/P25Defines.h"

#include <cstring>

using namespace p25::data;
using namespace p25::defines;

// ---------------------------------------------------------------------------
//  Structure Members
// ---------------------------------------------------------------------------

/* Decodes a conventional registration PDU from the given data buffer. */

bool ConventionalRegistration::decode(const uint8_t* data, uint32_t length, ConventionalRegistration& registration)
{
    if (data == nullptr || length < DISCONNECT_LENGTH)
        return false;

    // extract the type of the registration from the data buffer
    uint8_t type = (data[0U] >> 4U) & 0x0FU;
    if (type != PDURegType::CONNECT && type != PDURegType::DISCONNECT &&
        type != PDURegType::ACCEPT && type != PDURegType::DENY)
        return false;
    if (type != PDURegType::DISCONNECT && length < LENGTH)
        return false;

    // extract the logical link identifier from the data buffer
    uint32_t llId = GET_UINT24(data, 1U);
    if (llId == 0U)
        return false;

    // populate the registration object with the extracted data
    registration.type = type;
    registration.options = data[0U] & 0x0FU;
    registration.llId = llId;
    registration.ipAddress = type == PDURegType::DISCONNECT ? 0U : GET_UINT32(data, 8U);

    return true;
}

/* Encodes the conventional registration PDU into the given data buffer. */

bool ConventionalRegistration::encode(uint8_t* data, uint32_t capacity) const
{
    // determine the length of the encoded PDU based on the type of registration
    uint32_t encodedLength = type == PDURegType::DISCONNECT ? DISCONNECT_LENGTH : LENGTH;
    if (data == nullptr || capacity < encodedLength || llId == 0U)
        return false;
    if (type != PDURegType::CONNECT && type != PDURegType::DISCONNECT &&
        type != PDURegType::ACCEPT && type != PDURegType::DENY)
        return false;

    ::memset(data, 0x00U, encodedLength);
    data[0U] = uint8_t(((type & 0x0FU) << 4U) | (options & 0x0FU));
    SET_UINT24(llId, data, 1U);
    if (type != PDURegType::DISCONNECT) {
        SET_UINT32(ipAddress, data, 8U);
    }

    return true;
}
