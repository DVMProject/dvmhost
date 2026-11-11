// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 David Andrzejewski
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/ISP_SNDCP_DATA_PAGE_RES.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ISP_SNDCP_DATA_PAGE_RES class. */

ISP_SNDCP_DATA_PAGE_RES::ISP_SNDCP_DATA_PAGE_RES() : TSBK(),
    m_dataServiceOptions(0U),
    m_answerResponse(0U),
    m_dataAccessControl(0U)
{
    m_lco = TSBKO::ISP_SNDCP_DATA_PAGE_RES;
}

/* Decode a trunking signalling block. */

bool ISP_SNDCP_DATA_PAGE_RES::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_dataServiceOptions = (uint8_t)((tsbkValue >> 56) & 0xFFU);                    // Data Service Options
    m_answerResponse = (uint8_t)((tsbkValue >> 48) & 0xFFU);                        // Answer Response
    m_dataAccessControl = (uint16_t)((tsbkValue >> 32) & 0xFFFFU);                  // Data Access Control
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void ISP_SNDCP_DATA_PAGE_RES::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    /* stub -- inbound only; an SU sends this, the FNE and hosts never encode it */
}

/* Returns a string that represents the current TSBK. */

std::string ISP_SNDCP_DATA_PAGE_RES::toString(bool isp)
{
    return std::string("TSBKO, ISP_SNDCP_DATA_PAGE_RES (SNDCP Data Page Response)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void ISP_SNDCP_DATA_PAGE_RES::copy(const ISP_SNDCP_DATA_PAGE_RES& data)
{
    TSBK::copy(data);

    m_dataServiceOptions = data.m_dataServiceOptions;
    m_answerResponse = data.m_answerResponse;
    m_dataAccessControl = data.m_dataAccessControl;
}
