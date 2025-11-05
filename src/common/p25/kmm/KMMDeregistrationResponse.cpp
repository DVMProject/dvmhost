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
#include "p25/kmm/KMMDeregistrationResponse.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMDeregistrationResponse class. */

KMMDeregistrationResponse::KMMDeregistrationResponse() : KMMFrame(),
    m_status(KMM_Status::CMD_PERFORMED)
{
    m_messageId = KMM_MessageType::DEREG_RSP;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMDeregistrationResponse class. */

KMMDeregistrationResponse::~KMMDeregistrationResponse() = default;

/* Gets the byte length of this KMMDeregistrationResponse. */

uint32_t KMMDeregistrationResponse::length() const
{
    return KMMFrame::length() + KMM_BODY_DEREGISTRATION_RSP_LENGTH;
}

/* Decode a KMM modify key. */

bool KMMDeregistrationResponse::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_status = data[10U + m_bodyOffset];                        // Status

    return true;
}

/* Encode a KMM modify key. */

void KMMDeregistrationResponse::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = length();

    KMMFrame::encodeHeader(data);

    data[10U + m_bodyOffset] = m_status;                        // Status
}

/* Returns a string that represents the current KMM frame. */

std::string KMMDeregistrationResponse::toString()
{
    return std::string("KMM, DEREG_RSP (Deregistration Response)");
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMDeregistrationResponse::copy(const KMMDeregistrationResponse& data)
{
    KMMFrame::copy(data);

    m_status = data.m_status;
}
