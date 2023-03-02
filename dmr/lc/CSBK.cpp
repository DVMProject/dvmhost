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
*   Copyright (C) 2019-2021 by Bryan Biedenkapp N2PLL
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
//  Static Class Members
// ---------------------------------------------------------------------------

bool CSBK::m_verbose = false;

SiteData CSBK::m_siteData = SiteData();

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a copy instance of the CSBK class.
/// </summary>
/// <param name="data"></param>
CSBK::CSBK(const CSBK& data) : CSBK()
{
    copy(data);
}

/// <summary>
/// Initializes a new instance of the CSBK class.
/// </summary>
CSBK::CSBK() :
    m_colorCode(0U),
    m_lastBlock(true),
    m_Cdef(false),
    m_CSBKO(CSBKO_NONE),
    m_FID(0x00U),
    m_GI(false),
    m_srcId(0U),
    m_dstId(0U),
    m_dataContent(false),
    m_CBF(0U),
    m_emergency(false),
    m_privacy(false),
    m_supplementData(false),
    m_priority(0U),
    m_broadcast(false),
    m_proxy(false),
    m_response(0U),
    m_reason(0U),
    m_siteOffsetTiming(false),
    m_logicalCh1(DMR_CHNULL),
    m_logicalCh2(DMR_CHNULL),
    m_slotNo(0U),
    m_siteIdenEntry(::lookups::IdenTable())
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the CSBK class.
/// </summary>
CSBK::~CSBK()
{
    /* stub */
}

/// <summary>
/// Regenerate a DMR CSBK without decoding.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool CSBK::regenerate(uint8_t* data)
{
    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(data, csbk);

    // validate the CRC-CCITT 16
    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    bool valid = edac::CRC::checkCCITT162(csbk, DMR_CSBK_LENGTH_BYTES);
    if (!valid) {
        LogError(LOG_DMR, "CSBK::decode(), failed CRC CCITT-162 check");
        return false;
    }

    // restore the checksum
    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    // calculate checksum
    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    edac::CRC::addCCITT162(csbk, 12U);

    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    // encode BPTC (196,96) FEC
    bptc.encode(csbk, data);

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to convert CSBK bytes to a 64-bit long value.
/// </summary>
/// <param name="tsbk"></param>
/// <returns></returns>
ulong64_t CSBK::toValue(const uint8_t* csbk)
{
    ulong64_t csbkValue = 0U;

    // combine bytes into ulong64_t (8 byte) value
    csbkValue = csbk[2U];
    csbkValue = (csbkValue << 8) + csbk[3U];
    csbkValue = (csbkValue << 8) + csbk[4U];
    csbkValue = (csbkValue << 8) + csbk[5U];
    csbkValue = (csbkValue << 8) + csbk[6U];
    csbkValue = (csbkValue << 8) + csbk[7U];
    csbkValue = (csbkValue << 8) + csbk[8U];
    csbkValue = (csbkValue << 8) + csbk[9U];

    return csbkValue;
}

/// <summary>
/// Internal helper to convert a 64-bit long value to CSBK bytes.
/// </summary>
/// <param name="csbkValue"></param>
/// <returns></returns>
std::unique_ptr<uint8_t[]> CSBK::fromValue(const ulong64_t csbkValue)
{
    __UNIQUE_BUFFER(csbk, uint8_t, DMR_CSBK_LENGTH_BYTES);

    // split ulong64_t (8 byte) value into bytes
    csbk[2U] = (uint8_t)((csbkValue >> 56) & 0xFFU);
    csbk[3U] = (uint8_t)((csbkValue >> 48) & 0xFFU);
    csbk[4U] = (uint8_t)((csbkValue >> 40) & 0xFFU);
    csbk[5U] = (uint8_t)((csbkValue >> 32) & 0xFFU);
    csbk[6U] = (uint8_t)((csbkValue >> 24) & 0xFFU);
    csbk[7U] = (uint8_t)((csbkValue >> 16) & 0xFFU);
    csbk[8U] = (uint8_t)((csbkValue >> 8) & 0xFFU);
    csbk[9U] = (uint8_t)((csbkValue >> 0) & 0xFFU);

    return csbk;
}

/// <summary>
/// Internal helper to decode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="tsbk"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool CSBK::decode(const uint8_t* data, uint8_t* csbk)
{
    assert(data != nullptr);
    assert(csbk != nullptr);

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(data, csbk);

    // validate the CRC-CCITT 16
    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    bool valid = edac::CRC::checkCCITT162(csbk, DMR_CSBK_LENGTH_BYTES);
    if (!valid) {
        LogError(LOG_DMR, "CSBK::decode(), failed CRC CCITT-162 check");
        return false;
    }

    // restore the checksum
    csbk[10U] ^= CSBK_CRC_MASK[0U];
    csbk[11U] ^= CSBK_CRC_MASK[1U];

    if (m_verbose) {
        Utils::dump(2U, "Decoded CSBK", csbk, DMR_CSBK_LENGTH_BYTES);
    }

    m_CSBKO = csbk[0U] & 0x3FU;                                                     // CSBKO
    m_lastBlock = (csbk[0U] & 0x80U) == 0x80U;                                      // Last Block Marker
    m_FID = csbk[1U];                                                               // Feature ID

    m_dataContent = false;
    m_CBF = 0U;

    return true;
}

/// <summary>
/// Internal helper to encode a control signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="tsbk"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void CSBK::encode(uint8_t* data, const uint8_t* csbk)
{
    assert(data != nullptr);
    assert(csbk != nullptr);

    uint8_t outCsbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(outCsbk, 0x00U, DMR_CSBK_LENGTH_BYTES);
    ::memcpy(outCsbk, csbk, DMR_CSBK_LENGTH_BYTES);

    outCsbk[0U] = m_CSBKO;                                                          // CSBKO
    outCsbk[0U] |= (m_lastBlock) ? 0x80U : 0x00U;                                   // Last Block Marker
    if (!m_Cdef) {
        outCsbk[1U] = m_FID;                                                        // Feature ID
    }
    else {
        outCsbk[1U] = m_colorCode & 0x0FU;                                          // Cdef uses Color Code
    }

    outCsbk[10U] ^= CSBK_CRC_MASK[0U];
    outCsbk[11U] ^= CSBK_CRC_MASK[1U];

    edac::CRC::addCCITT162(outCsbk, 12U);

    outCsbk[10U] ^= CSBK_CRC_MASK[0U];
    outCsbk[11U] ^= CSBK_CRC_MASK[1U];

    if (m_verbose) {
        Utils::dump(2U, "Encoded CSBK", outCsbk, DMR_CSBK_LENGTH_BYTES);
    }

    // encode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.encode(outCsbk, data);
}

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void CSBK::copy(const CSBK& data)
{
    m_colorCode = data.m_colorCode;

    m_lastBlock = data.m_lastBlock;
    m_Cdef = data.m_Cdef;

    m_CSBKO = data.m_CSBKO;
    m_FID = data.m_FID;

    m_GI = data.m_GI;

    m_srcId = data.m_srcId;
    m_dstId = data.m_dstId;

    m_dataContent = data.m_dataContent;

    m_CBF = data.m_CBF;

    m_emergency = data.m_emergency;
    m_privacy = data.m_privacy;
    m_supplementData = data.m_supplementData;
    m_priority = data.m_priority;
    m_broadcast = data.m_broadcast;
    m_proxy = data.m_proxy;

    m_response = data.m_response;
    m_response = data.m_reason;

    m_siteOffsetTiming = data.m_siteOffsetTiming;

    m_logicalCh1 = data.m_logicalCh1;
    m_logicalCh2 = data.m_logicalCh2;
    m_slotNo = data.m_slotNo;

    m_siteIdenEntry = data.m_siteIdenEntry;
}
