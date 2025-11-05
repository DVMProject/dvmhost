// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/kmm/KMMRegistrationResponse.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMRegistrationResponse class. */

KMMRegistrationResponse::KMMRegistrationResponse() : KMMFrame(),
    m_status(KMM_Status::CMD_PERFORMED)
{
    m_messageId = KMM_MessageType::REG_RSP;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMRegistrationResponse class. */

KMMRegistrationResponse::~KMMRegistrationResponse() = default;

/* Gets the byte length of this KMMRegistrationResponse. */

uint32_t KMMRegistrationResponse::length() const
{
    uint32_t len = KMMFrame::length() + KMM_BODY_REGISTRATION_RSP_LENGTH;
    return len;
}

/* Decode a KMM modify key. */

bool KMMRegistrationResponse::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_status = data[10U + m_bodyOffset];                        // Status

    return true;
}

/* Encode a KMM modify key. */

void KMMRegistrationResponse::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    data[10U + m_bodyOffset] = m_status;                        // Status
}

/* Returns a string that represents the current KMM frame. */

std::string KMMRegistrationResponse::toString()
{
    return std::string("KMM, REG_RSP (Registration Response)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMRegistrationResponse::copy(const KMMRegistrationResponse& data)
{
    KMMFrame::copy(data);

    m_status = data.m_status;
}
