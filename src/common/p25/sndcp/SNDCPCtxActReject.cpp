// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/sndcp/SNDCPCtxActReject.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::sndcp;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the SNDCPCtxActReject class. */
SNDCPCtxActReject::SNDCPCtxActReject() : SNDCPPacket(),
    m_rejectCode(SNDCPRejectReason::ANY_REASON)
{
    m_pduType = SNDCP_PDUType::ACT_TDS_CTX_REJECT;
}

/* Decode a SNDCP context activation reject packet. */
bool SNDCPCtxActReject::decode(const uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::decodeHeader(data, false);

    m_rejectCode = data[1U];                                                        // Reject Code

    return true;
}

/* Encode a SNDCP context activation reject packet. */
void SNDCPCtxActReject::encode(uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::encodeHeader(data, true);

    data[1U] = m_rejectCode;                                                        // Reject Code
}

/* Internal helper to copy the the class. */
void SNDCPCtxActReject::copy(const SNDCPCtxActReject& data)
{
    m_rejectCode = data.m_rejectCode;
}
