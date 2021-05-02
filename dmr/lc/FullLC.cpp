/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2012 by Ian Wraith
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2021 Bryan Biedenkapp N2PLL
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
#include "dmr/DMRDefines.h"
#include "dmr/lc/FullLC.h"
#include "edac/RS129.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr::lc;
using namespace dmr;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initialize a new instance of the FullLC class.
/// </summary>
FullLC::FullLC() :
    m_bptc()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the FullLC class.
/// </summary>
FullLC::~FullLC()
{
    /* stub */
}

/// <summary>
/// Decode DMR full-link control data.
/// </summary>
/// <param name="data"></param>
/// <param name="type"></param>
/// <returns></returns>
LC* FullLC::decode(const uint8_t* data, uint8_t type)
{
    assert(data != NULL);

    // decode BPTC (196,96) FEC
    uint8_t lcData[DMR_LC_HEADER_LENGTH_BYTES];
    m_bptc.decode(data, lcData);

    switch (type) {
        case DT_VOICE_LC_HEADER:
            lcData[9U] ^= VOICE_LC_HEADER_CRC_MASK[0U];
            lcData[10U] ^= VOICE_LC_HEADER_CRC_MASK[1U];
            lcData[11U] ^= VOICE_LC_HEADER_CRC_MASK[2U];
            break;

        case DT_TERMINATOR_WITH_LC:
            lcData[9U] ^= TERMINATOR_WITH_LC_CRC_MASK[0U];
            lcData[10U] ^= TERMINATOR_WITH_LC_CRC_MASK[1U];
            lcData[11U] ^= TERMINATOR_WITH_LC_CRC_MASK[2U];
            break;

        default:
            LogError(LOG_DMR, "Unsupported LC type, type = %d", int(type));
            return NULL;
    }

    // check RS (12,9) FEC
    if (!edac::RS129::check(lcData))
        return NULL;

    return new LC(lcData);
}

/// <summary>
/// Encode DMR full-link control data.
/// </summary>
/// <param name="lc"></param>
/// <param name="data"></param>
/// <param name="type"></param>
void FullLC::encode(const LC& lc, uint8_t* data, uint8_t type)
{
    assert(data != NULL);

    uint8_t lcData[DMR_LC_HEADER_LENGTH_BYTES];
    lc.getData(lcData);

    // encode RS (12,9) FEC
    uint8_t parity[4U];
    edac::RS129::encode(lcData, 9U, parity);

    switch (type) {
        case DT_VOICE_LC_HEADER:
            lcData[9U] = parity[2U] ^ VOICE_LC_HEADER_CRC_MASK[0U];
            lcData[10U] = parity[1U] ^ VOICE_LC_HEADER_CRC_MASK[1U];
            lcData[11U] = parity[0U] ^ VOICE_LC_HEADER_CRC_MASK[2U];
            break;

        case DT_TERMINATOR_WITH_LC:
            lcData[9U] = parity[2U] ^ TERMINATOR_WITH_LC_CRC_MASK[0U];
            lcData[10U] = parity[1U] ^ TERMINATOR_WITH_LC_CRC_MASK[1U];
            lcData[11U] = parity[0U] ^ TERMINATOR_WITH_LC_CRC_MASK[2U];
            break;

        default:
            LogError(LOG_DMR, "Unsupported LC type, type = %d", int(type));
            return;
    }

    // encode BPTC (196,96) FEC
    m_bptc.encode(lcData, data);
}

/// <summary>
/// Decode DMR privacy control data.
/// </summary>
/// <param name="data"></param>
/// <param name="type"></param>
/// <returns></returns>
PrivacyLC* FullLC::decodePI(const uint8_t* data)
{
    assert(data != NULL);

    // decode BPTC (196,96) FEC
    uint8_t lcData[DMR_LC_HEADER_LENGTH_BYTES];
    m_bptc.decode(data, lcData);

    // make sure the CRC-CCITT 16 was actually included (the network tends to zero the CRC)
    if (lcData[10U] != 0x00U && lcData[11U] != 0x00U) {
        // validate the CRC-CCITT 16
        lcData[10U] ^= PI_HEADER_CRC_MASK[0U];
        lcData[11U] ^= PI_HEADER_CRC_MASK[1U];

        if (!edac::CRC::checkCCITT162(lcData, DMR_LC_HEADER_LENGTH_BYTES))
            return NULL;

        // restore the checksum
        lcData[10U] ^= PI_HEADER_CRC_MASK[0U];
        lcData[11U] ^= PI_HEADER_CRC_MASK[1U];
    }

    return new PrivacyLC(lcData);
}

/// <summary>
/// Encode DMR privacy control data.
/// </summary>
/// <param name="lc"></param>
/// <param name="data"></param>
/// <param name="type"></param>
void FullLC::encodePI(const PrivacyLC& lc, uint8_t* data)
{
    assert(data != NULL);

    uint8_t lcData[DMR_LC_HEADER_LENGTH_BYTES];
    lc.getData(lcData);

    // compute CRC-CCITT 16
    lcData[10U] ^= PI_HEADER_CRC_MASK[0U];
    lcData[11U] ^= PI_HEADER_CRC_MASK[1U];

    edac::CRC::addCCITT162(lcData, DMR_LC_HEADER_LENGTH_BYTES);

    // restore the checksum
    lcData[10U] ^= PI_HEADER_CRC_MASK[0U];
    lcData[11U] ^= PI_HEADER_CRC_MASK[1U];

    // encode BPTC (196,96) FEC
    m_bptc.encode(lcData, data);
}
