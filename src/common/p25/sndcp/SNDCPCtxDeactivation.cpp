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
#include "p25/sndcp/SNDCPCtxDeactivation.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::sndcp;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the SNDCPCtxDeactivation class. */
SNDCPCtxDeactivation::SNDCPCtxDeactivation() : SNDCPPacket(),
    m_deactType(SNDCPDeactivationType::DEACT_ALL)
{
    m_pduType = SNDCP_PDUType::DEACT_TDS_CTX_REQ;
}

/* Decode a SNDCP context deactivation packet. */
bool SNDCPCtxDeactivation::decode(const uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::decodeHeader(data, false);

    m_deactType = data[1U];                                                         // Deactivation Type

    return true;
}

/* Encode a SNDCP context deactivation packet. */
void SNDCPCtxDeactivation::encode(uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::encodeHeader(data, true);

    data[1U] = m_deactType;                                                         // Deactivation Type
}

/* Internal helper to copy the the class. */
void SNDCPCtxDeactivation::copy(const SNDCPCtxDeactivation& data)
{
    m_deactType = data.m_deactType;
}
