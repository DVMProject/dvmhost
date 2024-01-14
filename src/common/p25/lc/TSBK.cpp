/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
*
*/
/*
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
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
#include "p25/P25Defines.h"
#include "p25/lc/TSBK.h"
#include "p25/P25Utils.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

bool TSBK::m_verbose = false;
#if FORCE_TSBK_CRC_WARN
bool TSBK::m_warnCRC = true;
#else
bool TSBK::m_warnCRC = false;
#endif

uint8_t* TSBK::m_siteCallsign = nullptr;
SiteData TSBK::m_siteData = SiteData();

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a copy instance of the TSBK class.
/// </summary>
/// <param name="data"></param>
TSBK::TSBK(const TSBK& data) : TSBK()
{
    copy(data);
}

/// <summary>
/// Initializes a new instance of the TSBK class.
/// </summary>
/// <param name="lc"></param>
TSBK::TSBK(LC* lc) : TSBK()
{
    m_protect = lc->m_protect;
    m_lco = lc->m_lco;
    m_mfId = lc->m_mfId;

    m_srcId = lc->m_srcId;
    m_dstId = lc->m_dstId;

    m_emergency = lc->m_emergency;
    m_encrypted = lc->m_encrypted;
    m_priority = lc->m_priority;

    m_group = lc->m_group;
}

/// <summary>
/// Initializes a new instance of the TSBK class.
/// </summary>
/// <remarks>This should never be used.</remarks>
TSBK::TSBK() :
    m_protect(false),
    m_lco(LC_GROUP),
    m_mfId(P25_MFG_STANDARD),
    m_srcId(0U),
    m_dstId(0U),
    m_lastBlock(false),
    m_aivFlag(true),
    m_extendedAddrFlag(false),
    m_service(0U),
    m_response(P25_RSP_ACCEPT),
    m_netId(P25_WACN_STD_DEFAULT),
    m_sysId(P25_SID_STD_DEFAULT),
    m_emergency(false),
    m_encrypted(false),
    m_priority(4U),
    m_group(true),
    m_siteIdenEntry(lookups::IdenTable()),
    m_rs(),
    m_trellis()
{
    if (m_siteCallsign == nullptr) {
        m_siteCallsign = new uint8_t[P25_MOT_CALLSIGN_LENGTH_BYTES];
        ::memset(m_siteCallsign, 0x00U, P25_MOT_CALLSIGN_LENGTH_BYTES);
    }

#if FORCE_TSBK_CRC_WARN
    m_warnCRC = true;
#endif
}

/// <summary>
/// Finalizes a instance of TSBK class.
/// </summary>
TSBK::~TSBK()
{
    /* stub */
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string TSBK::toString(bool isp)
{
    return std::string("TSBK_IOSP_UNKWN (Unknown TSBK)");
}

/// <summary>
/// Sets the callsign.
/// </summary>
/// <param name="callsign">Callsign.</param>
void TSBK::setCallsign(std::string callsign)
{
    if (m_siteCallsign == nullptr) {
        m_siteCallsign = new uint8_t[P25_MOT_CALLSIGN_LENGTH_BYTES];
        ::memset(m_siteCallsign, 0x00U, P25_MOT_CALLSIGN_LENGTH_BYTES);
    }

    uint32_t idLength = callsign.length();
    if (idLength > 0) {
        ::memset(m_siteCallsign, 0x20U, P25_MOT_CALLSIGN_LENGTH_BYTES);

        if (idLength > P25_MOT_CALLSIGN_LENGTH_BYTES)
            idLength = P25_MOT_CALLSIGN_LENGTH_BYTES;
        for (uint32_t i = 0; i < idLength; i++)
            m_siteCallsign[i] = callsign[i];
    }
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to convert payload bytes to a 64-bit long value.
/// </summary>
/// <param name="tsbk"></param>
/// <returns></returns>
ulong64_t TSBK::toValue(const uint8_t* payload)
{
    assert(payload != nullptr);

    ulong64_t value = 0U;

    // combine bytes into ulong64_t (8 byte) value
    value = payload[0U];
    value = (value << 8) + payload[1U];
    value = (value << 8) + payload[2U];
    value = (value << 8) + payload[3U];
    value = (value << 8) + payload[4U];
    value = (value << 8) + payload[5U];
    value = (value << 8) + payload[6U];
    value = (value << 8) + payload[7U];

    return value;
}

/// <summary>
/// Internal helper to convert a 64-bit long value to payload bytes.
/// </summary>
/// <param name="value"></param>
/// <returns></returns>
UInt8Array TSBK::fromValue(const ulong64_t value)
{
    __UNIQUE_UINT8_ARRAY(payload, P25_TSBK_LENGTH_BYTES - 4U);

    // split ulong64_t (8 byte) value into bytes
    payload[0U] = (uint8_t)((value >> 56) & 0xFFU);
    payload[1U] = (uint8_t)((value >> 48) & 0xFFU);
    payload[2U] = (uint8_t)((value >> 40) & 0xFFU);
    payload[3U] = (uint8_t)((value >> 32) & 0xFFU);
    payload[4U] = (uint8_t)((value >> 24) & 0xFFU);
    payload[5U] = (uint8_t)((value >> 16) & 0xFFU);
    payload[6U] = (uint8_t)((value >> 8) & 0xFFU);
    payload[7U] = (uint8_t)((value >> 0) & 0xFFU);

    return payload;
}

/// <summary>
/// Internal helper to decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="payload"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool TSBK::decode(const uint8_t* data, uint8_t* payload, bool rawTSBK)
{
    assert(data != nullptr);
    assert(payload != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);
    if (rawTSBK)
    {
        ::memcpy(tsbk, data, P25_TSBK_LENGTH_BYTES);

        bool ret = edac::CRC::checkCCITT162(tsbk, P25_TSBK_LENGTH_BYTES);
        if (!ret) {
            if (m_warnCRC) {
                // if we're already warning instead of erroring CRC, don't announce invalid CRC in the 
                // case where no CRC is defined
                if ((tsbk[P25_TSBK_LENGTH_BYTES - 2U] != 0x00U) && (tsbk[P25_TSBK_LENGTH_BYTES - 1U] != 0x00U)) {
                    LogWarning(LOG_P25, "TSBK::decode(), failed CRC CCITT-162 check");
                }

                ret = true; // ignore CRC error
            }
            else {
                LogError(LOG_P25, "TSBK::decode(), failed CRC CCITT-162 check");
            }
        }
    }
    else {
        // deinterleave
        uint8_t raw[P25_TSBK_FEC_LENGTH_BYTES];
        P25Utils::decode(data, raw, 114U, 318U);

        // decode 1/2 rate Trellis & check CRC-CCITT 16
        try {
            bool ret = m_trellis.decode12(raw, tsbk);
            if (!ret) {
                LogError(LOG_P25, "TSBK::decode(), failed to decode Trellis 1/2 rate coding");
            }

            if (ret) {
                ret = edac::CRC::checkCCITT162(tsbk, P25_TSBK_LENGTH_BYTES);
                if (!ret) {
                    if (m_warnCRC) {
                        LogWarning(LOG_P25, "TSBK::decode(), failed CRC CCITT-162 check");
                        ret = true; // ignore CRC error
                    }
                    else {
                        LogError(LOG_P25, "TSBK::decode(), failed CRC CCITT-162 check");
                    }
                }
            }

            if (!ret)
                return false;
        }
        catch (...) {
            Utils::dump(2U, "P25, decoding excepted with input data", tsbk, P25_TSBK_LENGTH_BYTES);
            return false;
        }
    }

    if (m_verbose) {
        Utils::dump(2U, "TSBK::decode(), TSBK Value", tsbk, P25_TSBK_LENGTH_BYTES);
    }

    m_lco = tsbk[0U] & 0x3F;                                                        // LCO
    m_lastBlock = (tsbk[0U] & 0x80U) == 0x80U;                                      // Last Block Marker
    m_mfId = tsbk[1U];                                                              // Mfg Id.

    ::memcpy(payload, tsbk + 2U, P25_TSBK_LENGTH_BYTES - 4U);
    return true;
}

/// <summary>
/// Internal helper to eecode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="payload"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void TSBK::encode(uint8_t* data, const uint8_t* payload, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);
    assert(payload != nullptr);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);
    ::memcpy(tsbk + 2U, payload, P25_TSBK_LENGTH_BYTES - 4U);

    tsbk[0U] = m_lco;                                                               // LCO
    tsbk[0U] |= (m_lastBlock) ? 0x80U : 0x00U;                                      // Last Block Marker
    tsbk[1U] = m_mfId;                                                              // Mfg Id.

    // compute CRC-CCITT 16
    edac::CRC::addCCITT162(tsbk, P25_TSBK_LENGTH_BYTES);

    if (m_verbose) {
        Utils::dump(2U, "TSBK::encode(), TSBK Value", tsbk, P25_TSBK_LENGTH_BYTES);
    }

    uint8_t raw[P25_TSBK_FEC_LENGTH_BYTES];
    ::memset(raw, 0x00U, P25_TSBK_FEC_LENGTH_BYTES);

    // encode 1/2 rate Trellis
    m_trellis.encode12(tsbk, raw);

    // are we encoding a raw TSBK?
    if (rawTSBK) {
        if (noTrellis) {
            ::memcpy(data, tsbk, P25_TSBK_LENGTH_BYTES);
        }
        else {
            ::memcpy(data, raw, P25_TSBK_FEC_LENGTH_BYTES);
        }
    }
    else {
        // interleave
        P25Utils::encode(raw, data, 114U, 318U);

#if DEBUG_P25_TSBK
        Utils::dump(2U, "TSBK::encode(), TSBK Interleave", data, P25_TSBK_FEC_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
    }
}

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void TSBK::copy(const TSBK& data)
{
    m_protect = data.m_protect;
    m_lco = data.m_lco;
    m_mfId = data.m_mfId;

    m_srcId = data.m_srcId;
    m_dstId = data.m_dstId;

    m_lastBlock = data.m_lastBlock;
    m_aivFlag = data.m_aivFlag;
    m_extendedAddrFlag = data.m_extendedAddrFlag;

    m_response = data.m_response;
    m_service = data.m_service;

    m_netId = data.m_netId;
    m_sysId = data.m_sysId;

    m_grpVchNo = data.m_grpVchNo;

    m_emergency = data.m_emergency;
    m_encrypted = data.m_encrypted;
    m_priority = data.m_priority;

    m_group = data.m_group;

    m_siteIdenEntry = data.m_siteIdenEntry;
}
