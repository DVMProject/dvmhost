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
#include "p25/kmm/KMMHello.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMHello class. */

KMMHello::KMMHello() : KMMFrame(),
    m_flag(KMM_HelloFlag::IDENT_ONLY)
{
    m_messageId = KMM_MessageType::HELLO;
    m_respKind = KMM_ResponseKind::DELAYED;
}

/* Finalizes a instance of the KMMHello class. */

KMMHello::~KMMHello() = default;

/* Decode a KMM modify key. */

bool KMMHello::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_flag = data[10U];                                         // Hello Flag

    return true;
}

/* Encode a KMM modify key. */

void KMMHello::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = KMM_HELLO_LENGTH;

    KMMFrame::encodeHeader(data);

    data[10U] = m_flag;                                         // Hello Flag
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMHello::copy(const KMMHello& data)
{
    KMMFrame::copy(data);

    m_flag = data.m_flag;
}
