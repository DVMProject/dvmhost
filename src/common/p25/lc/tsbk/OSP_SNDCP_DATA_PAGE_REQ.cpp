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
#include "p25/lc/tsbk/OSP_SNDCP_DATA_PAGE_REQ.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_SNDCP_DATA_PAGE_REQ class. */

OSP_SNDCP_DATA_PAGE_REQ::OSP_SNDCP_DATA_PAGE_REQ() : TSBK(),
    m_dataServiceOptions(0U),
    m_dataAccessControl(1U)
{
    m_lco = TSBKO::OSP_SNDCP_DATA_PAGE_REQ;
}

/* Decode a trunking signalling block. */

bool OSP_SNDCP_DATA_PAGE_REQ::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_dataServiceOptions = (uint8_t)((tsbkValue >> 56) & 0xFFU);                    // Data Service Options
    m_dataAccessControl = (uint16_t)((tsbkValue >> 24) & 0xFFFFU);                  // Data Access Control
    m_dstId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Target Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void OSP_SNDCP_DATA_PAGE_REQ::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = m_dataServiceOptions;                                               // Data Service Options
    tsbkValue = (tsbkValue << 16);                                                  // Reserved
    tsbkValue = (tsbkValue << 16) + m_dataAccessControl;                            // Data Access Control
    tsbkValue = (tsbkValue << 24) + m_dstId;                                        // Target Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string OSP_SNDCP_DATA_PAGE_REQ::toString(bool isp)
{
    return std::string("TSBKO, OSP_SNDCP_DATA_PAGE_REQ (SNDCP Data Page Request)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void OSP_SNDCP_DATA_PAGE_REQ::copy(const OSP_SNDCP_DATA_PAGE_REQ& data)
{
    TSBK::copy(data);

    m_dataServiceOptions = data.m_dataServiceOptions;
    m_dataAccessControl = data.m_dataAccessControl;
}
