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
#include "p25/sndcp/SNDCPPacket.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::sndcp;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a copy instance of the SNDCPPacket class. */

SNDCPPacket::SNDCPPacket(const SNDCPPacket& data) : SNDCPPacket()
{
    copy(data);
}

/* Initializes a new instance of the SNDCPPacket class. */

SNDCPPacket::SNDCPPacket() :
    m_pduType(SNDCP_PDUType::ACT_TDS_CTX),
    m_sndcpVersion(SNDCP_VERSION_1),
    m_nsapi(0U)
{
    /* stub */
}

/* Finalizes a instance of the SNDCPPacket class. */

SNDCPPacket::~SNDCPPacket() = default;

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to decode a SNDCP header. */

bool SNDCPPacket::decodeHeader(const uint8_t* data, bool outbound)
{
    assert(data != nullptr);

    m_pduType = (uint8_t)((data[0U] >> 4) & 0x0FU);                                 // SNDCP PDU Message Type

    if (m_pduType == SNDCP_PDUType::ACT_TDS_CTX && !outbound) {
        m_sndcpVersion = (uint8_t)((data[0U]) & 0x0FU);                             // SNDCP Version
    } else {
        m_nsapi = (uint8_t)((data[0U]) & 0x0FU);                                    // NSAPI
    }

    return true;
}

/* Internal helper to encode a SNDCP header. */

void SNDCPPacket::encodeHeader(uint8_t* data, bool outbound)
{
    assert(data != nullptr);

    data[0U] = ((m_pduType << 4U) & 0xF0U);                                         // SNDCP PDU Message Type

    if (m_pduType == SNDCP_PDUType::ACT_TDS_CTX && !outbound) {
        data[0U] += (m_sndcpVersion & 0x0FU);                                       // SNDCP Version
    } else {
        data[0U] += (m_nsapi & 0x0FU);                                              // NSAPI
    }
}

/* Internal helper to copy the the class. */

void SNDCPPacket::copy(const SNDCPPacket& data)
{
    m_pduType = data.m_pduType;
    m_sndcpVersion = data.m_sndcpVersion;
}
