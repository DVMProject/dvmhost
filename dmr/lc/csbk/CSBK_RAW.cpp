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
#include "dmr/lc/csbk/CSBK_RAW.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr::lc::csbk;
using namespace dmr::lc;
using namespace dmr;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBK_RAW class.
/// </summary>
CSBK_RAW::CSBK_RAW() : CSBK(),
    m_csbk(nullptr)
{
    m_CSBKO = CSBKO_RAND;
}

/// <summary>
/// Finalizes a new instance of the CSBK_RAW class.
/// </summary>
CSBK_RAW::~CSBK_RAW()
{
    if (m_csbk != nullptr) {
        delete m_csbk;
    }
}

/// <summary>
/// Decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
bool CSBK_RAW::decode(const uint8_t* data)
{
    assert(data != NULL);

    /* stub */

    return true;
}

/// <summary>
/// Encode a control signalling block.
/// </summary>
/// <param name="data"></param>
void CSBK_RAW::encode(uint8_t* data)
{
    assert(data != NULL);
    assert(m_csbk != NULL);

    /* stub */

    CSBK::encode(data, m_csbk);
}

/// <summary>
/// Sets the CSBK to encode.
/// </summary>
/// <param name="csbk"></param>
void CSBK_RAW::setCSBK(const uint8_t* csbk)
{
    assert(csbk != NULL);

    m_CSBKO = csbk[0U] & 0x3FU;                                                     // CSBKO
    m_lastBlock = (csbk[0U] & 0x80U) == 0x80U;                                      // Last Block Marker
    m_FID = csbk[1U];                                                               // Feature ID

    m_csbk = new uint8_t[DMR_CSBK_LENGTH_BYTES];
    ::memset(m_csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    ::memcpy(m_csbk, csbk, DMR_CSBK_LENGTH_BYTES);
}
