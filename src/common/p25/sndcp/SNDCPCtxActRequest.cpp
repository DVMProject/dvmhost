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
#include "p25/sndcp/SNDCPCtxActRequest.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::sndcp;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the SNDCPCtxActRequest class. */

SNDCPCtxActRequest::SNDCPCtxActRequest() : SNDCPPacket(),
    m_nat(SNDCPNAT::IPV4_NO_ADDRESS),
    m_ipAddress(0U),
    m_dsut(SNDCP_DSUT::ALT_T_AND_C_DATA_VOICE),
    m_mdpco(0U)
{
    m_pduType = SNDCP_PDUType::ACT_TDS_CTX;
}

/* Decode a SNDCP context activation request. */

bool SNDCPCtxActRequest::decode(const uint8_t* data)
{
    assert(data != nullptr);

    SNDCPPacket::decodeHeader(data, false);

    m_nsapi = (uint8_t)((data[1U] >> 4) & 0x0FU);                                   // NSAPI
    m_nat = (uint8_t)((data[1U]) & 0x0FU);                                          // NAT

    m_ipAddress = 0U;                                                               // IP Address
    m_ipAddress = data[2U];
    m_ipAddress = (m_ipAddress << 8) + data[3U];
    m_ipAddress = (m_ipAddress << 8) + data[4U];
    m_ipAddress = (m_ipAddress << 8) + data[5U];

    m_dsut = (uint8_t)((data[6U] >> 4) & 0x0FU);                                    // Data Subscriber Unit Type

    m_mdpco = (uint8_t)(data[9U] & 0x0FU);                                          // MDPCO

    return true;
}

/* Encode a SNDCP context activation request. */

void SNDCPCtxActRequest::encode(uint8_t* data)
{
    assert(data != nullptr);

    /* stub */
}

/* Internal helper to copy the the class. */

void SNDCPCtxActRequest::copy(const SNDCPCtxActRequest& data)
{
    m_nsapi = data.m_nsapi;
    m_nat = data.m_nat;

    m_ipAddress = data.m_ipAddress;

    m_dsut = data.m_dsut;
}
