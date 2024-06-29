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
#include "p25/lc/tsbk/ISP_SNDCP_REC_REQ.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ISP_SNDCP_REC_REQ class. */

ISP_SNDCP_REC_REQ::ISP_SNDCP_REC_REQ() : TSBK(),
    m_dataToSend(false),
    m_dataServiceOptions(0U),
    m_dataAccessControl(0U)
{
    m_lco = TSBKO::ISP_SNDCP_REC_REQ;
}

/* Decode a trunking signalling block. */

bool ISP_SNDCP_REC_REQ::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_dataToSend = (tsbk[4U] & 0x80U) == 0x80U;                                     // Data To Send Flag

    m_dataServiceOptions = (uint8_t)((tsbkValue >> 56) & 0xFFU);                    // Data Service Options
    m_dataAccessControl = (uint32_t)((tsbkValue >> 40) & 0xFFFFFFFFU);              // Data Access Control
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void ISP_SNDCP_REC_REQ::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    /* stub */
}

/* Returns a string that represents the current TSBK. */

std::string ISP_SNDCP_REC_REQ::toString(bool isp)
{
    return std::string("TSBKO, ISP_SNDCP_REC_REQ (SNDCP Data Channel Request)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void ISP_SNDCP_REC_REQ::copy(const ISP_SNDCP_REC_REQ& data)
{
    TSBK::copy(data);

    m_dataServiceOptions = data.m_dataServiceOptions;
    m_dataAccessControl = data.m_dataAccessControl;
}
