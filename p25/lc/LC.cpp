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
*   Copyright (C) 2016,2017 by Jonathan Naylor G4KLX
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
#include "p25/lc/LC.h"
#include "p25/P25Utils.h"
#include "edac/CRC.h"
#include "edac/Golay24128.h"
#include "edac/Hamming.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc;
using namespace p25;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
/// <param name="siteData"></param>
LC::LC() :
    m_protect(false),
    m_lco(LC_GROUP),
    m_mfId(P25_MFG_STANDARD),
    m_srcId(0U),
    m_dstId(0U),
    m_grpVchNo(0U),
    m_emergency(false),
    m_encrypted(false),
    m_priority(4U),
    m_group(true),
    m_algId(P25_ALGO_UNENCRYPT),
    m_kId(0U),
    m_siteData(SiteData()),
    m_rs(),
    m_encryptOverride(false),
    m_tsbkVendorSkip(false),
    m_callTimer(0U),
    m_mi(NULL)
{
    m_mi = new uint8_t[P25_MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, P25_MI_LENGTH_BYTES);
}

/// <summary>
/// Initializes a copy instance of the LC class.
/// </summary>
/// <param name="data"></param>
LC::LC(const LC& data) : LC()
{
    copy(data);
}

/// <summary>
/// Initializes a new instance of the LC class.
/// </summary>
/// <param name="siteData"></param>
LC::LC(SiteData siteData) : LC()
{
    m_siteData = siteData;
    m_grpVchNo = m_siteData.channelNo();
}

/// <summary>
/// Finalizes a instance of LC class.
/// </summary>
LC::~LC()
{
    if (m_mi != NULL) {
        delete[] m_mi;
        m_mi = NULL;
    }
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
LC& LC::operator=(const LC& data)
{
    if (this != &data) {
        copy(data);
    }

    return *this;
}

/// <summary>
/// Decode a header data unit.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if HDU was decoded, otherwise false.</returns>
bool LC::decodeHDU(const uint8_t* data)
{
    assert(data != NULL);

    // deinterleave
    uint8_t rs[P25_HDU_LENGTH_BYTES + 1U];
    uint8_t raw[P25_HDU_LENGTH_BYTES + 1U];
    P25Utils::decode(data, raw, 114U, 780U);

    // decode Golay (18,6,8) FEC
    decodeHDUGolay(raw, rs);

#if DEBUG_P25_HDU
    Utils::dump(2U, "LC::decodeHDU(), HDU RS", rs, P25_HDU_LENGTH_BYTES);
#endif

    // decode RS (36,20,17) FEC
    try {
        bool ret = m_rs.decode362017(rs);
        if (!ret) {
            LogError(LOG_P25, "LC::decodeHDU(), failed to decode RS (36,20,17) FEC");
            return false;
        }
    }
    catch (...) {
        Utils::dump(2U, "P25, RS excepted with input data", rs, P25_HDU_LENGTH_BYTES);
        return false;
    }

#if DEBUG_P25_HDU
    Utils::dump(2U, "LC::decodeHDU(), HDU", rs, P25_HDU_LENGTH_BYTES);
#endif

    m_mfId = rs[9U];                                                                // Mfg Id.
    m_algId = rs[10U];                                                              // Algorithm ID
    if (m_algId != P25_ALGO_UNENCRYPT) {
        m_mi = new uint8_t[P25_MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, P25_MI_LENGTH_BYTES);
        ::memcpy(m_mi, rs, P25_MI_LENGTH_BYTES);                                    // Message Indicator

        m_kId = (rs[11U] << 8) + rs[12U];                                           // Key ID
        if (!m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = true;
        }
    }
    else {
        m_mi = new uint8_t[P25_MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, P25_MI_LENGTH_BYTES);

        m_kId = 0x0000U;
        if (m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = false;
        }
    }

    ulong64_t rsValue = 0U;

    // combine bytes into rs value
    rsValue = rs[12U];
    rsValue = (rsValue << 8) + rs[13U];
    rsValue = (rsValue << 8) + rs[14U];

    m_dstId = (uint32_t)(rsValue & 0xFFFFU);                                        // Talkgroup Address

    return true;
}

/// <summary>
/// Encode a header data unit.
/// </summary>
/// <param name="data"></param>
void LC::encodeHDU(uint8_t * data)
{
    assert(data != NULL);
    assert(m_mi != NULL);

    uint8_t rs[P25_HDU_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_HDU_LENGTH_BYTES);

    for (uint32_t i = 0; i < P25_MI_LENGTH_BYTES; i++)
        rs[i] = m_mi[i];                                                            // Message Indicator

    rs[9U] = m_mfId;                                                                // Mfg Id.
    rs[10U] = m_algId;                                                              // Algorithm ID
    rs[11U] = (m_kId >> 8) & 0xFFU;                                                 // Key ID
    rs[12U] = (m_kId >> 0) & 0xFFU;                                                 // ...
    rs[13U] = (m_dstId >> 8) & 0xFFU;                                               // Talkgroup Address
    rs[14U] = (m_dstId >> 0) & 0xFFU;                                               // ...

#if DEBUG_P25_HDU
    Utils::dump(2U, "LC::encodeHDU(), HDU", rs, P25_HDU_LENGTH_BYTES);
#endif

    // encode RS (36,20,17) FEC
    m_rs.encode362017(rs);

#if DEBUG_P25_HDU
    Utils::dump(2U, "LC::encodeHDU(), HDU RS", rs, P25_HDU_LENGTH_BYTES);
#endif

    uint8_t raw[P25_HDU_LENGTH_BYTES + 1U];
    ::memset(raw, 0x00U, P25_HDU_LENGTH_BYTES + 1U);

    // encode Golay (18,6,8) FEC
    encodeHDUGolay(raw, rs);

    // interleave
    P25Utils::encode(raw, data, 114U, 780U);

#if DEBUG_P25_HDU
    Utils::dump(2U, "LC::encodeHDU(), HDU Interleave", data, P25_HDU_FRAME_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
}

/// <summary>
/// Decode a logical link data unit 1.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if LDU1 was decoded, otherwise false.</returns>
bool LC::decodeLDU1(const uint8_t * data)
{
    assert(data != NULL);

    uint8_t rs[P25_LDU_LC_LENGTH_BYTES + 1U];

    // deinterleave and decode Hamming (10,6,3) for LC data
    uint8_t raw[5U];
    P25Utils::decode(data, raw, 410U, 452U);
    decodeLDUHamming(raw, rs + 0U);

    P25Utils::decode(data, raw, 600U, 640U);
    decodeLDUHamming(raw, rs + 3U);

    P25Utils::decode(data, raw, 788U, 830U);
    decodeLDUHamming(raw, rs + 6U);

    P25Utils::decode(data, raw, 978U, 1020U);
    decodeLDUHamming(raw, rs + 9U);

    P25Utils::decode(data, raw, 1168U, 1208U);
    decodeLDUHamming(raw, rs + 12U);

    P25Utils::decode(data, raw, 1356U, 1398U);
    decodeLDUHamming(raw, rs + 15U);

#if DEBUG_P25_LDU1
    Utils::dump(2U, "LC::decodeLDU1(), LDU1 RS", rs, P25_LDU_LC_LENGTH_BYTES);
#endif

    // decode RS (24,12,13) FEC
    try {
        bool ret = m_rs.decode241213(rs);
        if (!ret) {
            LogError(LOG_P25, "LC::decodeLDU1(), failed to decode RS (24,12,13) FEC");
            return false;
        }
    }
    catch (...) {
        Utils::dump(2U, "P25, RS excepted with input data", rs, P25_LDU_LC_LENGTH_BYTES);
        return false;
    }

#if DEBUG_P25_LDU1
    Utils::dump(2U, "LC::decodeLDU1(), LDU1 LC", rs, P25_LDU_LC_LENGTH_BYTES);
#endif

    return decodeLC(rs);
}

/// <summary>
/// Encode a logical link data unit 1.
/// </summary>
/// <param name="data"></param>
void LC::encodeLDU1(uint8_t * data)
{
    assert(data != NULL);

    uint8_t rs[P25_LDU_LC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_LDU_LC_LENGTH_BYTES);

    encodeLC(rs);

#if DEBUG_P25_LDU1
    Utils::dump(2U, "LC::encodeLDU1(), LDU1 LC", rs, P25_LDU_LC_LENGTH_BYTES);
#endif

    // encode RS (24,12,13) FEC
    m_rs.encode241213(rs);

#if DEBUG_P25_LDU1
    Utils::dump(2U, "LC::encodeLDU1(), LDU1 RS", rs, P25_LDU_LC_LENGTH_BYTES);
#endif

    // encode Hamming (10,6,3) FEC and interleave for LC data
    uint8_t raw[5U];
    encodeLDUHamming(raw, rs + 0U);
    P25Utils::encode(raw, data, 410U, 452U);

    encodeLDUHamming(raw, rs + 3U);
    P25Utils::encode(raw, data, 600U, 640U);

    encodeLDUHamming(raw, rs + 6U);
    P25Utils::encode(raw, data, 788U, 830U);

    encodeLDUHamming(raw, rs + 9U);
    P25Utils::encode(raw, data, 978U, 1020U);

    encodeLDUHamming(raw, rs + 12U);
    P25Utils::encode(raw, data, 1168U, 1208U);

    encodeLDUHamming(raw, rs + 15U);
    P25Utils::encode(raw, data, 1356U, 1398U);

#if DEBUG_P25_LDU1
    Utils::dump(2U, "LC::encodeLDU1(), LDU1 Interleave", data, P25_LDU_FRAME_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
}

/// <summary>
/// Decode a logical link data unit 2.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if LDU2 was decoded, otherwise false.</returns>
bool LC::decodeLDU2(const uint8_t * data)
{
    assert(data != NULL);

    uint8_t rs[P25_LDU_LC_LENGTH_BYTES + 1U];

    // deinterleave and decode Hamming (10,6,3) for LC data
    uint8_t raw[5U];
    P25Utils::decode(data, raw, 410U, 452U);
    decodeLDUHamming(raw, rs + 0U);

    P25Utils::decode(data, raw, 600U, 640U);
    decodeLDUHamming(raw, rs + 3U);

    P25Utils::decode(data, raw, 788U, 830U);
    decodeLDUHamming(raw, rs + 6U);

    P25Utils::decode(data, raw, 978U, 1020U);
    decodeLDUHamming(raw, rs + 9U);

    P25Utils::decode(data, raw, 1168U, 1208U);
    decodeLDUHamming(raw, rs + 12U);

    P25Utils::decode(data, raw, 1356U, 1398U);
    decodeLDUHamming(raw, rs + 15U);

#if DEBUG_P25_LDU2
    Utils::dump(2U, "LC::decodeLDU2(), LDU2 RS", rs, P25_LDU_LC_LENGTH_BYTES);
#endif

    // decode RS (24,16,9) FEC
    try {
        bool ret = m_rs.decode24169(rs);
        if (!ret) {
            LogError(LOG_P25, "LC::decodeLDU2(), failed to decode RS (24,16,9) FEC");
            return false;
        }
    }
    catch (...) {
        Utils::dump(2U, "P25, RS excepted with input data", rs, P25_LDU_LC_LENGTH_BYTES);
        return false;
    }

#if DEBUG_P25_LDU2
    Utils::dump(2U, "LC::decodeLDU2(), LDU2 LC", rs, P25_LDU_LC_LENGTH_BYTES);
#endif

    m_algId = rs[9U];                                                               // Algorithm ID
    if (m_algId != P25_ALGO_UNENCRYPT) {
        m_mi = new uint8_t[P25_MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, P25_MI_LENGTH_BYTES);
        ::memcpy(m_mi, rs, P25_MI_LENGTH_BYTES);                                    // Message Indicator

        m_kId = (rs[10U] << 8) + rs[11U];                                           // Key ID
        if (!m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = true;
        }
    }
    else {
        m_mi = new uint8_t[P25_MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, P25_MI_LENGTH_BYTES);

        m_kId = 0x0000U;
        if (m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = false;
        }
    }

    return true;
}

/// <summary>
/// Encode a logical link data unit 2.
/// </summary>
/// <param name="data"></param>
void LC::encodeLDU2(uint8_t * data)
{
    assert(data != NULL);
    assert(m_mi != NULL);

    uint8_t rs[P25_LDU_LC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_LDU_LC_LENGTH_BYTES);

    for (uint32_t i = 0; i < P25_MI_LENGTH_BYTES; i++)
        rs[i] = m_mi[i];                                                            // Message Indicator

    rs[9U] = m_algId;                                                               // Algorithm ID
    rs[10U] = (m_kId >> 8) & 0xFFU;                                                 // Key ID
    rs[11U] = (m_kId >> 0) & 0xFFU;                                                 // ...

#if DEBUG_P25_LDU2
    Utils::dump(2U, "LC::encodeLDU2(), LDU2 LC", rs, P25_LDU_LC_LENGTH_BYTES);
#endif

    // encode RS (24,16,9) FEC
    m_rs.encode24169(rs);
    
#if DEBUG_P25_LDU2
    Utils::dump(2U, "LC::encodeLDU2(), LDU2 RS", rs, P25_LDU_LC_LENGTH_BYTES);
#endif

    // encode Hamming (10,6,3) FEC and interleave for LC data
    uint8_t raw[5U];
    encodeLDUHamming(raw, rs + 0U);
    P25Utils::encode(raw, data, 410U, 452U);

    encodeLDUHamming(raw, rs + 3U);
    P25Utils::encode(raw, data, 600U, 640U);

    encodeLDUHamming(raw, rs + 6U);
    P25Utils::encode(raw, data, 788U, 830U);

    encodeLDUHamming(raw, rs + 9U);
    P25Utils::encode(raw, data, 978U, 1020U);

    encodeLDUHamming(raw, rs + 12U);
    P25Utils::encode(raw, data, 1168U, 1208U);

    encodeLDUHamming(raw, rs + 15U);
    P25Utils::encode(raw, data, 1356U, 1398U);

#if DEBUG_P25_LDU2
    Utils::dump(2U, "LC::encodeLDU2(), LDU2 Interleave", data, P25_LDU_FRAME_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
}

/** Encryption data */
/// <summary>Sets the encryption message indicator.</summary>
/// <param name="mi"></param>
void LC::setMI(const uint8_t* mi)
{
    assert(mi != NULL);

    ::memcpy(m_mi, mi, P25_MI_LENGTH_BYTES);
}

/// <summary>Gets the encryption message indicator.</summary>
/// <returns></returns>
void LC::getMI(uint8_t* mi) const
{
    assert(mi != NULL);

    ::memcpy(mi, m_mi, P25_MI_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void LC::copy(const LC& data)
{
    m_protect = data.m_protect;
    m_mfId = data.m_mfId;

    m_srcId = data.m_srcId;
    m_dstId = data.m_dstId;

    m_grpVchNo = data.m_grpVchNo;

    m_emergency = data.m_emergency;
    m_encrypted = data.m_encrypted;
    m_priority = data.m_priority;

    m_group = data.m_group;

    m_callTimer = data.m_callTimer;

    m_algId = data.m_algId;
    if (m_algId != P25_ALGO_UNENCRYPT) {
        delete[] m_mi;

        m_mi = new uint8_t[P25_MI_LENGTH_BYTES];
        ::memcpy(m_mi, data.m_mi, P25_MI_LENGTH_BYTES);

        m_kId = data.m_kId;
        if (!m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = true;
        }
    }
    else {
        delete[] m_mi;

        m_mi = new uint8_t[P25_MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, P25_MI_LENGTH_BYTES);

        m_kId = 0x0000U;
        if (m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = false;
        }
    }

    m_siteData = data.m_siteData;
}

/// <summary>
/// Decode link control.
/// </summary>
/// <param name="rs"></param>
/// <returns></returns>
bool LC::decodeLC(const uint8_t * rs)
{
    ulong64_t rsValue = 0U;

    // combine bytes into rs value
    rsValue = rs[1U];
    rsValue = (rsValue << 8) + rs[2U];
    rsValue = (rsValue << 8) + rs[3U];
    rsValue = (rsValue << 8) + rs[4U];
    rsValue = (rsValue << 8) + rs[5U];
    rsValue = (rsValue << 8) + rs[6U];
    rsValue = (rsValue << 8) + rs[7U];
    rsValue = (rsValue << 8) + rs[8U];

    m_protect = (rs[0U] & 0x80U) == 0x80U;                                          // Protect Flag
    m_lco = rs[0U] & 0x3FU;                                                         // LCO

    // standard P25 reference opcodes
    switch (m_lco) {
    case LC_GROUP:
        m_mfId = rs[1U];                                                            // Mfg Id.
        m_group = true;
        m_emergency = (rs[2U] & 0x80U) == 0x80U;                                    // Emergency Flag
        if (!m_encryptOverride) {
            m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                // Encryption Flag
        }
        m_priority = (rs[2U] & 0x07U);                                              // Priority
        m_dstId = (uint32_t)((rsValue >> 24) & 0xFFFFU);                            // Talkgroup Address
        m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source Radio Address
        break;
    case LC_PRIVATE:
        m_mfId = rs[1U];                                                            // Mfg Id.
        m_group = false;
        m_emergency = (rs[2U] & 0x80U) == 0x80U;                                    // Emergency Flag
        if (!m_encryptOverride) {
            m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                // Encryption Flag
        }
        m_priority = (rs[2U] & 0x07U);                                              // Priority
        m_dstId = (uint32_t)((rsValue >> 24) & 0xFFFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source Radio Address
        break;
    case LC_TEL_INT_VCH_USER:
        m_emergency = (rs[2U] & 0x80U) == 0x80U;                                    // Emergency Flag
        if (!m_encryptOverride) {
            m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                // Encryption Flag
        }
        m_priority = (rs[2U] & 0x07U);                                              // Priority
        m_callTimer = (uint32_t)((rsValue >> 24) & 0xFFFFU);                        // Call Timer
        if (m_srcId == 0U) {
            m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                              // Source/Target Address
        }
        break;
    default:
        LogError(LOG_P25, "LC::decodeLC(), unknown LC value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
        return false;
    }

    // sanity check priority (per TIA-102.AABC-B) it should never be 0, if its 0, default to 4
    if (m_priority == 0) {
        m_priority = 4U;
    }

    return true;
}

/// <summary>
/// Encode link control.
/// </summary>
/// <param name="rs"></param>
void LC::encodeLC(uint8_t * rs)
{
    ulong64_t rsValue = 0U;
    rs[0U] = m_lco;                                                                 // LCO

    // standard P25 reference opcodes
    switch (m_lco) {
    case LC_GROUP:
        rsValue = m_mfId;
        rsValue = (rsValue << 8) +
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        rsValue = (rsValue << 24) + m_dstId;                                        // Talkgroup Address
        rsValue = (rsValue << 24) + m_srcId;                                        // Source Radio Address
        break;
    case LC_GROUP_UPDT:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = m_siteData.channelId();                                           // Group A - Channel ID
        rsValue = (rsValue << 12) + m_grpVchNo;                                     // Group A - Channel Number
        rsValue = (rsValue << 16) + m_dstId;                                        // Group A - Talkgroup Address
        rsValue = (rsValue << 4) + m_siteData.channelId();                          // Group B - Channel ID
        rsValue = (rsValue << 12) + m_grpVchNo;                                     // Group B - Channel Number
        rsValue = (rsValue << 16) + m_dstId;                                        // Group B - Talkgroup Address
        break;
    case LC_PRIVATE:
        rsValue = m_mfId;
        rsValue = (rsValue << 8) +
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        rsValue = (rsValue << 24) + m_dstId;                                        // Target Radio Address
        rsValue = (rsValue << 24) + m_srcId;                                        // Source Radio Address
        break;
    case LC_TEL_INT_VCH_USER:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = (rsValue << 8) +
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        rsValue = (rsValue << 16) + m_callTimer;                                    // Call Timer
        rsValue = (rsValue << 24) + m_srcId;                                        // Source/Target Radio Address
        break;
    case LC_RFSS_STS_BCAST:
        rs[0U] |= 0x40U;                                                            // Implicit Operation
        rsValue = m_siteData.lra();                                                 // Location Registration Area
        rsValue = (rsValue << 12) + m_siteData.sysId();                             // System ID
        rsValue = (rsValue << 8) + m_siteData.rfssId();                             // RF Sub-System ID
        rsValue = (rsValue << 8) + m_siteData.siteId();                             // Site ID
        rsValue = (rsValue << 4) + m_siteData.channelId();                          // Channel ID
        rsValue = (rsValue << 12) + m_siteData.channelNo();                         // Channel Number
        rsValue = (rsValue << 8) + m_siteData.serviceClass();                       // System Service Class
        break;
    default:
        LogError(LOG_P25, "LC::encodeLC(), unknown LC value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
        break;
    }

    // split rs value into bytes
    rs[1U] = (uint8_t)((rsValue >> 56) & 0xFFU);
    rs[2U] = (uint8_t)((rsValue >> 48) & 0xFFU);
    rs[3U] = (uint8_t)((rsValue >> 40) & 0xFFU);
    rs[4U] = (uint8_t)((rsValue >> 32) & 0xFFU);
    rs[5U] = (uint8_t)((rsValue >> 24) & 0xFFU);
    rs[6U] = (uint8_t)((rsValue >> 16) & 0xFFU);
    rs[7U] = (uint8_t)((rsValue >> 8) & 0xFFU);
    rs[8U] = (uint8_t)((rsValue >> 0) & 0xFFU);
}

/// <summary>
/// Decode LDU hamming FEC.
/// </summary>
/// <param name="data"></param>
/// <param name="raw"></param>
void LC::decodeLDUHamming(const uint8_t * data, uint8_t * raw)
{
    uint32_t n = 0U;
    uint32_t m = 0U;
    for (uint32_t i = 0U; i < 4U; i++) {
        bool hamming[10U];

        for (uint32_t j = 0U; j < 10U; j++) {
            hamming[j] = READ_BIT(data, n);
            n++;
        }

        edac::Hamming::decode1063(hamming);

        for (uint32_t j = 0U; j < 6U; j++) {
            WRITE_BIT(raw, m, hamming[j]);
            m++;
        }
    }
}

/// <summary>
/// Encode LDU hamming FEC.
/// </summary>
/// <param name="data"></param>
/// <param name="raw"></param>
void LC::encodeLDUHamming(uint8_t * data, const uint8_t * raw)
{
    uint32_t n = 0U;
    uint32_t m = 0U;
    for (uint32_t i = 0U; i < 4U; i++) {
        bool hamming[10U];

        for (uint32_t j = 0U; j < 6U; j++) {
            hamming[j] = READ_BIT(raw, m);
            m++;
        }

        edac::Hamming::encode1063(hamming);

        for (uint32_t j = 0U; j < 10U; j++) {
            WRITE_BIT(data, n, hamming[j]);
            n++;
        }
    }
}

/// <summary>
/// Decode HDU Golay FEC.
/// </summary>
/// <param name="data"></param>
/// <param name="raw"></param>
void LC::decodeHDUGolay(const uint8_t * data, uint8_t * raw)
{
    // shortened Golay (18,6,8) decode
    uint32_t n = 0U;
    uint32_t m = 0U;
    for (uint32_t i = 0U; i < 36U; i++) {
        bool golay[18U];
        for (uint32_t j = 0U; j < 18U; j++) {
            golay[j] = READ_BIT(data, n);
            n++;
        }

        uint32_t g0 = 0U;
        for (uint32_t j = 0U; j < 18U; j++)
            g0 = (g0 << 1) | (golay[j] ? 0x01U : 0x00U);
        uint32_t c0data = edac::Golay24128::decode24128(g0);
        for (int j = 5; j >= 0; j--) {
            golay[j] = (c0data & 0x01U) == 0x01U;
            c0data >>= 1;
        }

        for (uint32_t j = 0U; j < 6U; j++) {
            WRITE_BIT(raw, m, golay[j]);
            m++;
        }
    }
}

/// <summary>
/// Encode HDU Golay FEC.
/// </summary>
/// <param name="data"></param>
/// <param name="raw"></param>
void LC::encodeHDUGolay(uint8_t * data, const uint8_t * raw)
{
    // shortened Golay (18,6,8) encode
    uint32_t n = 0U;
    uint32_t m = 0U;
    for (uint32_t i = 0U; i < 36U; i++) {
        bool golay[18U];
        for (uint32_t j = 0U; j < 6U; j++) {
            golay[j] = READ_BIT(raw, m);
            m++;
        }

        uint32_t c0data = 0U;
        for (uint32_t j = 0U; j < 6U; j++)
            c0data = (c0data << 1) | (golay[j] ? 0x01U : 0x00U);
        uint32_t g0 = edac::Golay24128::encode24128(c0data);
        for (int j = 17; j >= 0; j--) {
            golay[j] = (g0 & 0x01U) == 0x01U;
            g0 >>= 1;
        }

        for (uint32_t j = 0U; j < 18U; j++) {
            WRITE_BIT(data, n, golay[j]);
            n++;
        }
    }
}
