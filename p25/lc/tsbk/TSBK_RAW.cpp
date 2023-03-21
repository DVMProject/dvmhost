/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "p25/lc/tsbk/TSBK_RAW.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the TSBK_RAW class.
/// </summary>
TSBK_RAW::TSBK_RAW() : TSBK(),
    m_tsbk(nullptr)
{
    m_lco = TSBK_IOSP_ACK_RSP;
}

/// <summary>
/// Finalizes a new instance of the TSBK_RAW class.
/// </summary>
TSBK_RAW::~TSBK_RAW()
{
    if (m_tsbk != nullptr) {
        delete m_tsbk;
    }
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool TSBK_RAW::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

    /* stub */

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void TSBK_RAW::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    /* stub */

    TSBK::encode(data, m_tsbk, rawTSBK, noTrellis);
}

/// <summary>
/// Sets the TSBK to encode.
/// </summary>
/// <param name="tsbk"></param>
void TSBK_RAW::setTSBK(const uint8_t* tsbk)
{
    assert(tsbk != NULL);

    m_lco = tsbk[0U] & 0x3F;                                                        // LCO
    m_lastBlock = (tsbk[0U] & 0x80U) == 0x80U;                                      // Last Block Marker
    m_mfId = tsbk[1U];                                                              // Mfg Id.

    m_tsbk = new uint8_t[P25_TSBK_LENGTH_BYTES];
    ::memset(m_tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    ::memcpy(m_tsbk, tsbk, P25_TSBK_LENGTH_BYTES);
}
