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
#include "p25/kmm/KMMDeregistrationCommand.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::kmm;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the KMMDeregistrationCommand class. */

KMMDeregistrationCommand::KMMDeregistrationCommand() : KMMFrame(),
    m_bodyFormat(0U),
    m_kmfRSI(9999999U)
{
    m_messageId = KMM_MessageType::DEREG_CMD;
    m_respKind = KMM_ResponseKind::IMMEDIATE;
}

/* Finalizes a instance of the KMMDeregistrationCommand class. */

KMMDeregistrationCommand::~KMMDeregistrationCommand() = default;

/* Decode a KMM modify key. */

bool KMMDeregistrationCommand::decode(const uint8_t* data)
{
    assert(data != nullptr);

    KMMFrame::decodeHeader(data);

    m_bodyFormat = data[10U];                                   // Body Format
    m_kmfRSI = GET_UINT24(data, 11U);                           // KMF RSI

    return true;
}

/* Encode a KMM modify key. */

void KMMDeregistrationCommand::encode(uint8_t* data)
{
    assert(data != nullptr);
    m_messageLength = KMM_DEREGISTRATION_CMD_LENGTH;
    m_bodyFormat = 0U; // reset this to none -- we don't support warm start right now

    KMMFrame::encodeHeader(data);

    data[10U] = m_bodyFormat;                                   // Body Format
    SET_UINT24(m_kmfRSI, data, 11U);                            // KMF RSI
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void KMMDeregistrationCommand::copy(const KMMDeregistrationCommand& data)
{
    KMMFrame::copy(data);

    m_bodyFormat = data.m_bodyFormat;
    m_kmfRSI = data.m_kmfRSI;
}
