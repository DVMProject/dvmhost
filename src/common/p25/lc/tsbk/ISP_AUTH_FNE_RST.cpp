// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/ISP_AUTH_FNE_RST.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ISP_AUTH_FNE_RST class. */
ISP_AUTH_FNE_RST::ISP_AUTH_FNE_RST() : TSBK(),
    m_authSuccess(false),
    m_authStandalone(false)
{
    m_lco = TSBKO::ISP_AUTH_FNE_RST;
}

/* Decode a trunking signalling block. */
bool ISP_AUTH_FNE_RST::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_authSuccess = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 // Authentication Success Flag
    m_authStandalone = (((tsbkValue >> 56) & 0xFFU) & 0x01U) == 0x01U;              // Authentication Standalone Flag
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */
void ISP_AUTH_FNE_RST::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    /* stub */
}

/* Returns a string that represents the current TSBK. */
std::string ISP_AUTH_FNE_RST::toString(bool isp)
{
    return std::string("TSBKO, ISP_AUTH_FNE_RST (Authentication FNE Result)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */
void ISP_AUTH_FNE_RST::copy(const ISP_AUTH_FNE_RST& data)
{
    TSBK::copy(data);

    m_authSuccess = data.m_authSuccess;
    m_authStandalone = data.m_authStandalone;
}
