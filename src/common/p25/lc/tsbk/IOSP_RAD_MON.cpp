// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022 by Jason-UWU
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/IOSP_RAD_MON.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the IOSP_RAD_MON class. */

IOSP_RAD_MON::IOSP_RAD_MON() : TSBK(),
    m_txMult(0U)
{
    m_lco = TSBKO::IOSP_RAD_MON;
}

/* Decode a trunking signalling block. */

bool IOSP_RAD_MON::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    bool ret = TSBK::decode(data, tsbk, rawTSBK);
    if (!ret)
        return false;

    ulong64_t tsbkValue = TSBK::toValue(tsbk);

    m_txMult = (uint8_t)((tsbkValue >> 48) & 0x3U);                                 // TX Multiplier
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                            // Target Radio Address
    m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                    // Source Radio Address

    return true;
}

/* Encode a trunking signalling block. */

void IOSP_RAD_MON::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = (tsbkValue << 48) + (m_txMult & 0x3U);                              // TX Multiplier
    tsbkValue = (tsbkValue << 24) + (m_srcId & 0xFFFFFFU);                          // Source Radio Address
    tsbkValue = tsbkValue + (m_dstId & 0xFFFFFFU);                                  // Target Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */

std::string IOSP_RAD_MON::toString(bool isp)
{
    return (isp) ? std::string("TSBKO, IOSP_RAD_MON (Radio Unit Monitor Request)") :
        std::string("TSBKO, IOSP_RAD_MON (Radio Unit Monitor Command)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void IOSP_RAD_MON::copy(const IOSP_RAD_MON& data)
{
    TSBK::copy(data);

    m_txMult = data.m_txMult;
}
