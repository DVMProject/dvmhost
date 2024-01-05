/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
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
#include "p25/lc/tdulc/TDULCFactory.h"
#include "edac/Golay24128.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tdulc;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

::edac::RS634717 TDULCFactory::m_rs = ::edac::RS634717();

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the TDULCFactory class.
/// </summary>
TDULCFactory::TDULCFactory()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of TDULCFactory class.
/// </summary>
TDULCFactory::~TDULCFactory()
{
    /* stub */
}

/// <summary>
/// Create an instance of a TDULC.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TDULC was decoded, otherwise false.</returns>
std::unique_ptr<TDULC> TDULCFactory::createTDULC(const uint8_t* data)
{
    assert(data != nullptr);

    // deinterleave
    uint8_t rs[P25_TDULC_LENGTH_BYTES + 1U];
    uint8_t raw[P25_TDULC_FEC_LENGTH_BYTES + 1U];
    P25Utils::decode(data, raw, 114U, 410U);

    // decode Golay (24,12,8) FEC
    edac::Golay24128::decode24128(rs, raw, P25_TDULC_LENGTH_BYTES);

#if DEBUG_P25_TDULC
    Utils::dump(2U, "TDULCFactory::decode(), TDULC RS", rs, P25_TDULC_LENGTH_BYTES);
#endif

    // decode RS (24,12,13) FEC
    try {
        bool ret = m_rs.decode241213(rs);
        if (!ret) {
            LogError(LOG_P25, "TDULCFactory::decode(), failed to decode RS (24,12,13) FEC");
            return nullptr;
        }
    }
    catch (...) {
        Utils::dump(2U, "P25, RS excepted with input data", rs, P25_TDULC_LENGTH_BYTES);
        return nullptr;
    }

    uint8_t lco = rs[0U] & 0x3FU;                                                   // LCO

    // standard P25 reference opcodes
    switch (lco) {
    case p25::LC_GROUP:
        return decode(new LC_GROUP(), data);
    case p25::LC_PRIVATE:
        return decode(new LC_PRIVATE(), data);
    case p25::LC_TEL_INT_VCH_USER:
        return decode(new LC_TEL_INT_VCH_USER(), data);
    default:
        LogError(LOG_P25, "TDULCFactory::create(), unknown TDULC LCO value, lco = $%02X", lco);
        break;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="tdulc"></param>
/// <param name="data"></param>
/// <returns></returns>
std::unique_ptr<TDULC> TDULCFactory::decode(TDULC* tdulc, const uint8_t* data)
{
    assert(tdulc != nullptr);
    assert(data != nullptr);

    if (!tdulc->decode(data)) {
        return nullptr;
    }

    return std::unique_ptr<TDULC>(tdulc);
}
