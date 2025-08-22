// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tdulc/LC_TDULC_RAW.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tdulc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the LC_TDULC_RAW class. */

LC_TDULC_RAW::LC_TDULC_RAW() : TDULC(),
    m_tdulc(nullptr)
{
    m_lco = LCO::GROUP;
}

/* Finalizes a new instance of the LC_TDULC_RAW class. */

LC_TDULC_RAW::~LC_TDULC_RAW()
{
    if (m_tdulc != nullptr) {
        delete m_tdulc;
    }
}

/* Decode a terminator data unit w/ link control. */

bool LC_TDULC_RAW::decode(const uint8_t* data)
{
    assert(data != nullptr);

    return decode(data, false);
}

/* Encode a terminator data unit w/ link control. */

void LC_TDULC_RAW::encode(uint8_t* data)
{
    assert(data != nullptr);
    assert(m_tdulc != nullptr);

    encode(data, false);
}

/* Decode a terminator data unit w/ link control. */

bool LC_TDULC_RAW::decode(const uint8_t* data, bool rawTDULC)
{
    assert(data != nullptr);

    if (m_tdulc != nullptr)
        delete[] m_tdulc;

    m_tdulc = new uint8_t[P25_TDULC_PAYLOAD_LENGTH_BYTES + 1U];
    ::memset(m_tdulc, 0x00U, P25_TDULC_PAYLOAD_LENGTH_BYTES);

    bool ret = TDULC::decode(data, m_tdulc, rawTDULC);
    if (!ret)
        return false;

    return true;
}

/* Encode a terminator data unit w/ link control. */

void LC_TDULC_RAW::encode(uint8_t* data, bool rawTDULC)
{
    assert(data != nullptr);
    assert(m_tdulc != nullptr);

    /* stub */

    TDULC::encode(data, m_tdulc, rawTDULC);
}

/* Sets the TDULC to encode. */

void LC_TDULC_RAW::setTDULC(const uint8_t* tdulc)
{
    assert(tdulc != nullptr);

    m_tdulc = new uint8_t[P25_TDULC_PAYLOAD_LENGTH_BYTES];
    ::memset(m_tdulc, 0x00U, P25_TDULC_PAYLOAD_LENGTH_BYTES);

    ::memcpy(m_tdulc, tdulc, P25_TDULC_PAYLOAD_LENGTH_BYTES);
}
