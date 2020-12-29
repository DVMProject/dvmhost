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
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2019 by Bryan Biedenkapp N2PLL
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
#include "dmr/lc/CSBK.h"
#include "edac/BPTC19696.h"
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
/// Initializes a new instance of the CSBK class.
/// </summary>
CSBK::CSBK() :
    m_verbose(false),
    m_CSBKO(CSBKO_NONE),
    m_FID(0x00U),
    m_bsId(0U),
    m_GI(false),
    m_srcId(0U),
    m_dstId(0U),
    m_dataContent(false),
    m_CBF(0U),
    m_data(NULL)
{
    m_data = new uint8_t[12U];
}

/// <summary>
/// Finalizes a instance of the CSBK class.
/// </summary>
CSBK::~CSBK()
{
    delete[] m_data;
}

/// <summary>
/// Decodes a DMR CSBK.
/// </summary>
/// <param name="bytes"></param>
/// <returns>True, if DMR CSBK was decoded, otherwise false.</returns>
bool CSBK::decode(const uint8_t* bytes)
{
    assert(bytes != NULL);

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(bytes, m_data);

    // validate the CRC-CCITT 16
    m_data[10U] ^= CSBK_CRC_MASK[0U];
    m_data[11U] ^= CSBK_CRC_MASK[1U];

    bool valid = edac::CRC::checkCCITT162(m_data, DMR_LC_HEADER_LENGTH_BYTES);
    if (!valid)
        return false;

    // restore the checksum
    m_data[10U] ^= CSBK_CRC_MASK[0U];
    m_data[11U] ^= CSBK_CRC_MASK[1U];

    if (m_verbose) {
        Utils::dump(2U, "Decoded CSBK", m_data, DMR_LC_HEADER_LENGTH_BYTES);
    }

    m_CSBKO = m_data[0U] & 0x3FU;
    m_FID = m_data[1U];

    switch (m_CSBKO) {
    case CSBKO_BSDWNACT:
        m_GI = false;
        m_bsId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];
        m_dataContent = false;
        m_CBF = 0U;
        break;

    case CSBKO_UU_V_REQ:
        m_GI = false;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];
        m_dataContent = false;
        m_CBF = 0U;
        break;

    case CSBKO_UU_ANS_RSP:
        m_GI = false;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];
        m_dataContent = false;
        m_CBF = 0U;
        break;

    case CSBKO_PRECCSBK:
        m_GI = (m_data[2U] & 0x40U) == 0x40U;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];
        m_dataContent = (m_data[2U] & 0x80U) == 0x80U;
        m_CBF = m_data[3U];
        break;

    case CSBKO_CALL_ALRT:
        m_GI = (m_data[2U] & 0x40U) == 0x40U;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];
        m_dataContent = (m_data[2U] & 0x80U) == 0x80U;
        m_CBF = m_data[3U];
        break;

    case CSBKO_ACK_RSP:
        m_GI = (m_data[2U] & 0x40U) == 0x40U;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];
        m_dataContent = (m_data[2U] & 0x80U) == 0x80U;
        m_CBF = m_data[3U];
        break;

    case CSBKO_EXT_FNCT:
        m_GI = false;
        m_dstId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];
        m_srcId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];
        m_dataContent = (m_data[2U] & 0x80U) == 0x80U;
        m_CBF = m_data[3U];
        break;

    case CSBKO_NACK_RSP:
        m_GI = false;
        m_srcId = m_data[4U] << 16 | m_data[5U] << 8 | m_data[6U];
        m_dstId = m_data[7U] << 16 | m_data[8U] << 8 | m_data[9U];
        m_dataContent = false;
        m_CBF = 0U;
        break;

    default:
        m_GI = false;
        m_srcId = 0U;
        m_dstId = 0U;
        m_dataContent = false;
        m_CBF = 0U;
        LogError(LOG_DMR, "unknown CSBK type, csbko = $%02X", m_CSBKO);
        return true;
    }

    return true;
}

/// <summary>
/// Encodes a DMR CSBK.
/// </summary>
/// <param name="bytes"></param>
void CSBK::encode(uint8_t* bytes) const
{
    assert(bytes != NULL);

    m_data[0U] = m_CSBKO;
    m_data[1U] = m_FID;

    if (m_GI) {
        m_data[2U] |= 0x40U;
    }

    if (m_dataContent) {
        m_data[2U] |= 0x80U;
    }

    m_data[3U] = m_CBF;

    if (m_CSBKO == CSBKO_EXT_FNCT) {
        m_data[4U] = (m_srcId >> 16) & 0xFFU;
        m_data[5U] = (m_srcId >> 8) & 0xFFU;
        m_data[6U] = (m_srcId >> 0) & 0xFFU;

        m_data[7U] = (m_dstId >> 16) & 0xFFU;
        m_data[8U] = (m_dstId >> 8) & 0xFFU;
        m_data[9U] = (m_dstId >> 0) & 0xFFU;
    }
    else {
        m_data[4U] = (m_dstId >> 16) & 0xFFU;
        m_data[5U] = (m_dstId >> 8) & 0xFFU;
        m_data[6U] = (m_dstId >> 0) & 0xFFU;

        m_data[7U] = (m_srcId >> 16) & 0xFFU;
        m_data[8U] = (m_srcId >> 8) & 0xFFU;
        m_data[9U] = (m_srcId >> 0) & 0xFFU;
    }

    m_data[10U] ^= CSBK_CRC_MASK[0U];
    m_data[11U] ^= CSBK_CRC_MASK[1U];

    edac::CRC::addCCITT162(m_data, 12U);

    m_data[10U] ^= CSBK_CRC_MASK[0U];
    m_data[11U] ^= CSBK_CRC_MASK[1U];

    if (m_verbose) {
        Utils::dump(2U, "Encoded CSBK", m_data, DMR_LC_HEADER_LENGTH_BYTES);
    }

    // encode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.encode(m_data, bytes);
}
