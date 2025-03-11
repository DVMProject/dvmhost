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
#include "p25/kmm/KMMNoService.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMNoService class. */

KMMNoService::KMMNoService() : KMMFrame()
{
    m_messageId = KMM_MessageType::NO_SERVICE;
    m_respKind = KMM_ResponseKind::NONE;
}

/* Finalizes a instance of the KMMNoService class. */

KMMNoService::~KMMNoService() = default;

/* Decode a KMM modify key. */

bool KMMNoService::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    return true;
}

/* Encode a KMM modify key. */

void KMMNoService::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = KMM_NO_SERVICE_LENGTH;

    KMMFrame::encodeHeader(data);
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMNoService::copy(const KMMNoService& data)
{
    KMMFrame::copy(data);
}
