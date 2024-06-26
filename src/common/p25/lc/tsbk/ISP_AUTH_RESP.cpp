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
#include "p25/lc/tsbk/ISP_AUTH_RESP.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the ISP_AUTH_RESP class. */

ISP_AUTH_RESP::ISP_AUTH_RESP() : TSBK(),
    m_authStandalone(false),
    m_authRes(nullptr)
{
    m_lco = TSBKO::ISP_AUTH_RESP;

    m_authRes = new uint8_t[AUTH_RES_LENGTH_BYTES];
    ::memset(m_authRes, 0x00U, AUTH_RES_LENGTH_BYTES);
}

/* Finalizes a instance of ISP_AUTH_RESP class. */

ISP_AUTH_RESP::~ISP_AUTH_RESP()
{
    if (m_authRes != nullptr) {
        delete[] m_authRes;
        m_authRes = nullptr;
    }
}

/* Decode a trunking signalling block. */

bool ISP_AUTH_RESP::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_authStandalone = (((tsbkValue >> 56) & 0xFFU) & 0x01U) == 0x01U;              // Authentication Standalone Flag
    m_authRes[0U] = tsbk[1U];                                                       // RES1(3)
    m_authRes[1U] = tsbk[2U];                                                       // RES1(2)
    m_authRes[2U] = tsbk[3U];                                                       // RES1(1)
    m_authRes[3U] = tsbk[4U];                                                       // RES1(0)

    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void ISP_AUTH_RESP::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    /* stub */
}

/* Returns a string that represents the current TSBK. */

std::string ISP_AUTH_RESP::toString(bool isp)
{
    return std::string("TSBKO, ISP_AUTH_RESP (Authentication Response)");
}

/* Gets the authentication result. */

void ISP_AUTH_RESP::getAuthRes(uint8_t* res) const
{
    assert(res != nullptr);

    ::memcpy(res, m_authRes, AUTH_RES_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void ISP_AUTH_RESP::copy(const ISP_AUTH_RESP& data)
{
    TSBK::copy(data);

    m_authStandalone = data.m_authStandalone;

    if (m_authRes != nullptr) {
        delete[] m_authRes;
    }

    m_authRes = new uint8_t[AUTH_RES_LENGTH_BYTES];
    ::memcpy(m_authRes, data.m_authRes, AUTH_RES_LENGTH_BYTES);
}
