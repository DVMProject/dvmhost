// SPDX-License-Identifier: GPL-2.0-only
/*
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
*   Copyright (C) 2016,2017 Jonathan Naylor, G4KLX
*   Copyright (C) 2017-2025 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/lc/LC.h"
#include "p25/P25Utils.h"
#include "edac/Golay24128.h"
#include "edac/Hamming.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

SiteData LC::m_siteData = SiteData();

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a copy instance of the LC class. */

LC::LC(const LC& data) : LC()
{
    copy(data);
}

/* Initializes a new instance of the LC class. */

LC::LC() :
    m_protect(false),
    m_lco(LCO::GROUP),
    m_mfId(MFG_STANDARD),
    m_srcId(0U),
    m_dstId(0U),
    m_grpVchNo(0U),
    m_grpVchNoB(0U),
    m_dstIdB(0U),
    m_explicitId(false),
    m_netId(WACN_STD_DEFAULT),
    m_sysId(SID_STD_DEFAULT),
    m_emergency(false),
    m_encrypted(false),
    m_priority(4U),
    m_group(true),
    m_algId(ALGO_UNENCRYPT),
    m_kId(0U),
    m_rsValue(0U),
    m_rs(),
    m_encryptOverride(false),
    m_tsbkVendorSkip(false),
    m_callTimer(0U),
    m_mi(nullptr),
    m_userAlias(nullptr),
    m_gotUserAliasPartA(false),
    m_gotUserAlias(false)
{
    m_mi = new uint8_t[MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);

    m_userAlias = new uint8_t[HARRIS_USER_ALIAS_LENGTH_BYTES];
    ::memset(m_userAlias, 0x00U, HARRIS_USER_ALIAS_LENGTH_BYTES);
}

/* Finalizes a instance of LC class. */

LC::~LC()
{
    if (m_mi != nullptr) {
        delete[] m_mi;
        m_mi = nullptr;
    }

    if (m_userAlias != nullptr) {
        delete[] m_userAlias;
        m_userAlias = nullptr;
    }
}

/* Equals operator. */

LC& LC::operator=(const LC& data)
{
    if (this != &data) {
        copy(data);
    }

    return *this;
}

/* Decode a header data unit. */

bool LC::decodeHDU(const uint8_t* data, bool rawOnly)
{
    assert(data != nullptr);

    // deinterleave
    uint8_t rs[P25_HDU_LENGTH_BYTES + 1U];
    uint8_t raw[P25_HDU_LENGTH_BYTES + 1U];
    if (rawOnly)
        ::memcpy(raw, data, P25_HDU_LENGTH_BYTES);
    else
        P25Utils::decode(data, raw, 114U, 780U);

#if DEBUG_P25_HDU
    Utils::dump(2U, "P25, LC::decodeHDU(), HDU Raw", raw, P25_HDU_LENGTH_BYTES);
#endif

    // decode Golay (18,6,8) FEC
    decodeHDUGolay(raw, rs);

#if DEBUG_P25_HDU
    Utils::dump(2U, "P25, LC::decodeHDU(), HDU RS", rs, P25_HDU_LENGTH_BYTES);
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
        Utils::dump(2U, "P25, LC::decodeHDU(), RS excepted with input data", rs, P25_HDU_LENGTH_BYTES);
        return false;
    }

#if DEBUG_P25_HDU
    Utils::dump(2U, "P25, LC::decodeHDU(), HDU", rs, P25_HDU_LENGTH_BYTES);
#endif

    m_mfId = rs[9U];                                                                // Mfg Id.
    m_algId = rs[10U];                                                              // Algorithm ID
    if (m_algId != ALGO_UNENCRYPT) {
        m_mi = new uint8_t[MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
        ::memcpy(m_mi, rs, MI_LENGTH_BYTES);                                        // Message Indicator

        m_kId = (rs[11U] << 8) + rs[12U];                                           // Key ID
        if (!m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = true;
        }
    }
    else {
        m_mi = new uint8_t[MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);

        m_kId = 0x0000U;
        if (m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = false;
        }
    }

    ulong64_t rsValue = 0U;

    // combine bytes into ulong64_t (8 byte) value
    rsValue = rs[12U];
    rsValue = (rsValue << 8) + rs[13U];
    rsValue = (rsValue << 8) + rs[14U];

    m_dstId = (uint32_t)(rsValue & 0xFFFFU);                                        // Talkgroup Address

    return true;
}

/* Encode a header data unit. */

void LC::encodeHDU(uint8_t* data, bool rawOnly)
{
    assert(data != nullptr);
    assert(m_mi != nullptr);

    uint8_t rs[P25_HDU_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_HDU_LENGTH_BYTES);

    for (uint32_t i = 0; i < MI_LENGTH_BYTES; i++)
        rs[i] = m_mi[i];                                                            // Message Indicator

    rs[9U] = m_mfId;                                                                // Mfg Id.
    rs[10U] = m_algId;                                                              // Algorithm ID
    rs[11U] = (m_kId >> 8) & 0xFFU;                                                 // Key ID
    rs[12U] = (m_kId >> 0) & 0xFFU;                                                 // ...
    rs[13U] = (m_dstId >> 8) & 0xFFU;                                               // Talkgroup Address
    rs[14U] = (m_dstId >> 0) & 0xFFU;                                               // ...

#if DEBUG_P25_HDU
    Utils::dump(2U, "P25, LC::encodeHDU(), HDU", rs, P25_HDU_LENGTH_BYTES);
#endif

    // encode RS (36,20,17) FEC
    m_rs.encode362017(rs);

#if DEBUG_P25_HDU
    Utils::dump(2U, "P25, LC::encodeHDU(), HDU RS", rs, P25_HDU_LENGTH_BYTES);
#endif

    uint8_t raw[P25_HDU_LENGTH_BYTES + 1U];
    ::memset(raw, 0x00U, P25_HDU_LENGTH_BYTES + 1U);

    // encode Golay (18,6,8) FEC
    encodeHDUGolay(raw, rs);

    if (rawOnly) {
        ::memcpy(data, raw, P25_HDU_LENGTH_BYTES);
        return;
    }

    // interleave
    P25Utils::encode(raw, data, 114U, 780U);

#if DEBUG_P25_HDU
    Utils::dump(2U, "P25, LC::encodeHDU(), HDU Interleave", data, P25_HDU_FRAME_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
}

/* Decode a logical link data unit 1. */

bool LC::decodeLDU1(const uint8_t* data, bool rawOnly)
{
    assert(data != nullptr);

    uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES + 1U];

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
    Utils::dump(2U, "P25, LC::decodeLDU1(), LDU1 RS", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
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
        Utils::dump(2U, "P25, LC::decodeLDU1(), RS excepted with input data", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
        return false;
    }

#if DEBUG_P25_LDU1
    Utils::dump(2U, "P25, LC::decodeLDU1(), LDU1 LC", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
#endif

    return decodeLC(rs, rawOnly);
}

/* Encode a logical link data unit 1. */

void LC::encodeLDU1(uint8_t* data)
{
    assert(data != nullptr);

    uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

    encodeLC(rs);

#if DEBUG_P25_LDU1
    Utils::dump(2U, "P25, LC::encodeLDU1(), LDU1 LC", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
#endif

    // encode RS (24,12,13) FEC
    m_rs.encode241213(rs);

#if DEBUG_P25_LDU1
    Utils::dump(2U, "P25, LC::encodeLDU1(), LDU1 RS", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
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
    Utils::dump(2U, "P25, LC::encodeLDU1(), LDU1 Interleave", data, P25_LDU_FRAME_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
}

/* Decode a logical link data unit 2. */

bool LC::decodeLDU2(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES + 1U];

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
    Utils::dump(2U, "P25, LC::decodeLDU2(), LDU2 RS", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
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
        Utils::dump(2U, "P25, LC::decodeLDU2(), RS excepted with input data", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
        return false;
    }

#if DEBUG_P25_LDU2
    Utils::dump(2U, "P25, LC::decodeLDU2(), LDU2 LC", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
#endif

    m_algId = rs[9U];                                                               // Algorithm ID
    if (m_algId != ALGO_UNENCRYPT) {
        if (m_mi != nullptr)
            delete[] m_mi;
        m_mi = new uint8_t[MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);
        ::memcpy(m_mi, rs, MI_LENGTH_BYTES);                                        // Message Indicator

        m_kId = (rs[10U] << 8) + rs[11U];                                           // Key ID
        if (!m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = true;
        }
    }
    else {
        if (m_mi != nullptr)
            delete[] m_mi;
        m_mi = new uint8_t[MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);

        m_kId = 0x0000U;
        if (m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = false;
        }
    }

    return true;
}

/* Encode a logical link data unit 2. */

void LC::encodeLDU2(uint8_t* data)
{
    assert(data != nullptr);
    assert(m_mi != nullptr);

    uint8_t rs[P25_LDU_LC_FEC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_LDU_LC_FEC_LENGTH_BYTES);

    for (uint32_t i = 0; i < MI_LENGTH_BYTES; i++)
        rs[i] = m_mi[i];                                                            // Message Indicator

    rs[9U] = m_algId;                                                               // Algorithm ID
    rs[10U] = (m_kId >> 8) & 0xFFU;                                                 // Key ID
    rs[11U] = (m_kId >> 0) & 0xFFU;                                                 // ...

#if DEBUG_P25_LDU2
    Utils::dump(2U, "P25, LC::encodeLDU2(), LDU2 LC", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
#endif

    // encode RS (24,16,9) FEC
    m_rs.encode24169(rs);

#if DEBUG_P25_LDU2
    Utils::dump(2U, "P25, LC::encodeLDU2(), LDU2 RS", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
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
    Utils::dump(2U, "P25, LC::encodeLDU2(), LDU2 Interleave", data, P25_LDU_FRAME_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
}

/* Helper to determine if the MFId is a standard MFId. */

bool LC::isStandardMFId() const
{
    if ((m_mfId == MFG_STANDARD) || (m_mfId == MFG_STANDARD_ALT))
        return true;
    return false;
}

/* Decode link control. */

bool LC::decodeLC(const uint8_t* rs, bool rawOnly)
{
    assert(rs != nullptr);

    ulong64_t rsValue = 0U;

    // combine bytes into ulong64_t (8 byte) value
    rsValue = rs[1U];
    rsValue = (rsValue << 8) + rs[2U];
    rsValue = (rsValue << 8) + rs[3U];
    rsValue = (rsValue << 8) + rs[4U];
    rsValue = (rsValue << 8) + rs[5U];
    rsValue = (rsValue << 8) + rs[6U];
    rsValue = (rsValue << 8) + rs[7U];
    rsValue = (rsValue << 8) + rs[8U];
    m_rsValue = rsValue;

    m_protect = (rs[0U] & 0x80U) == 0x80U;                                          // Protect Flag
    m_lco = rs[0U] & 0x3FU;                                                         // LCO

    bool sf = (rs[0U] & 0x40U) == 0x40U;                                            // Implicit/Explicit Operation
    if (sf)
        m_mfId = MFG_STANDARD;
    else
        m_mfId = rs[1U];                                                            // Mfg Id.

    if (rawOnly)
        return true;

    // non-standard P25 vendor opcodes (these are just detected for passthru, and stored
    // as the packed RS value)
    if ((m_mfId != MFG_STANDARD) && (m_mfId != MFG_STANDARD_ALT)) {
        //Utils::dump(1U, "P25, LC::decodeLC(), Decoded P25 Non-Standard RS", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
        // Harris
        if (m_mfId == MFG_HARRIS) {
            // Harris P25 opcodes
            switch (m_lco) {
            case LCO::HARRIS_USER_ALIAS_PA_ODD:
            case LCO::HARRIS_USER_ALIAS_PA_EVEN:
                m_gotUserAliasPartA = true;
                m_gotUserAlias = false;

                if (m_userAlias != nullptr) {
                    ::memset(m_userAlias, 0x00U, HARRIS_USER_ALIAS_LENGTH_BYTES);
                    ::memcpy(m_userAlias, rs + 2U, 7U);
                    m_gotUserAlias = true;
                }
                break;

            case LCO::HARRIS_USER_ALIAS_PB_ODD:
            case LCO::HARRIS_USER_ALIAS_PB_EVEN:
                if (m_gotUserAliasPartA && (m_userAlias != nullptr)) {
                    ::memcpy(m_userAlias + 7U, rs + 2U, 7U);
                    m_gotUserAlias = true;
                }
                break;

            default:
                break;
            }
        }

        return true;
    }

    // standard P25 reference opcodes
    switch (m_lco) {
    case LCO::GROUP:
        m_mfId = rs[1U];                                                            // Mfg Id.
        m_group = true;
        m_emergency = (rs[2U] & 0x80U) == 0x80U;                                    // Emergency Flag
        if (!m_encryptOverride) {
            m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                // Encryption Flag
        }
        m_priority = (rs[2U] & 0x07U);                                              // Priority
        m_explicitId = (rs[3U] & 0x01U) == 0x01U;                                   // Explicit Source ID Flag
        m_dstId = (uint32_t)((rsValue >> 24) & 0xFFFFU);                            // Talkgroup Address
        m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source Radio Address
        break;
    case LCO::PRIVATE:
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
    case LCO::TEL_INT_VCH_USER:
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
    case LCO::EXPLICIT_SOURCE_ID:
        m_netId = (uint32_t)((rsValue >> 36) & 0xFFFFFU);                           // Network ID
        m_sysId = (uint32_t)((rsValue >> 24) & 0xFFFU);                             // System ID
        m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source Radio Address
        break;
    case LCO::PRIVATE_EXT:
        m_explicitId = (rs[1U] & 0x01U) == 0x01U;                                   // Explicit Source ID Flag
        m_group = false;
        m_emergency = (rs[2U] & 0x80U) == 0x80U;                                    // Emergency Flag
        if (!m_encryptOverride) {
            m_encrypted = (rs[2U] & 0x40U) == 0x40U;                                // Encryption Flag
        }
        m_priority = (rs[2U] & 0x07U);                                              // Priority
        m_explicitId = (rs[3U] & 0x01U) == 0x01U;                                   // Explicit Source ID Flag
        m_dstId = (uint32_t)((rsValue >> 24) & 0xFFFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(rsValue & 0xFFFFFFU);                                  // Source Radio Address
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

/* Encode link control. */

void LC::encodeLC(uint8_t* rs)
{
    assert(rs != nullptr);

    ulong64_t rsValue = 0U;
    rs[0U] = m_lco;                                                                 // LCO

    // non-standard P25 vendor opcodes (these are just detected for passthru, and stored
    // as the packed RS value)
    if ((m_mfId != MFG_STANDARD) && (m_mfId != MFG_STANDARD_ALT)) {
        //Utils::dump(1U, "P25, LC::decodeLC(), Decoded P25 Non-Standard RS", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
        if (m_mfId == MFG_HARRIS) {
            // Harris P25 opcodes
            switch (m_lco) {
            case LCO::HARRIS_USER_ALIAS_PA_ODD:
            case LCO::HARRIS_USER_ALIAS_PA_EVEN:
                if (m_userAlias != nullptr) {
                    // split ulong64_t (8 byte) value into bytes
                    rs[1U] = m_mfId;                                                // Manufacturer ID
                    rs[2U] = m_userAlias[0U];
                    rs[3U] = m_userAlias[1U];
                    rs[4U] = m_userAlias[2U];
                    rs[5U] = m_userAlias[3U];
                    rs[6U] = m_userAlias[4U];
                    rs[7U] = m_userAlias[5U];
                    rs[8U] = m_userAlias[6U];
                }
                return;

            case LCO::HARRIS_USER_ALIAS_PB_ODD:
            case LCO::HARRIS_USER_ALIAS_PB_EVEN:
                if (m_userAlias != nullptr) {
                    // split ulong64_t (8 byte) value into bytes
                    rs[1U] = m_mfId;                                                // Manufacturer ID
                    rs[2U] = m_userAlias[7U];
                    rs[3U] = m_userAlias[8U];
                    rs[4U] = m_userAlias[9U];
                    rs[5U] = m_userAlias[10U];
                    rs[6U] = m_userAlias[11U];
                    rs[7U] = m_userAlias[12U];
                    rs[8U] = m_userAlias[13U];
                }
                return;

            default:
                break;
            }
        }
    }

    if ((m_mfId == MFG_STANDARD) || (m_mfId == MFG_STANDARD_ALT)) {
        // standard P25 reference opcodes
        switch (m_lco) {
        case LCO::GROUP:
            rsValue = m_mfId;                                                       // Manufacturer ID
            rsValue = (rsValue << 8) +
                (m_emergency ? 0x80U : 0x00U) +                                     // Emergency Flag
                (m_encrypted ? 0x40U : 0x00U) +                                     // Encrypted Flag
                (m_priority & 0x07U);                                               // Priority
            rsValue = (rsValue << 8) + (m_explicitId ? 0x01U : 0x00U);              // Explicit Source ID Flag
            rsValue = (rsValue << 16) + m_dstId;                                    // Talkgroup Address
            rsValue = (rsValue << 24) + m_srcId;                                    // Source Radio Address
            break;
        case LCO::GROUP_UPDT:
            rs[0U] |= 0x40U;                                                        // Implicit Operation
            rsValue = m_siteData.channelId();                                       // Group A - Channel ID
            rsValue = (rsValue << 12) + m_grpVchNo;                                 // Group A - Channel Number
            rsValue = (rsValue << 16) + m_dstId;                                    // Group A - Talkgroup Address
            rsValue = (rsValue << 4) + m_siteData.channelId();                      // Group B - Channel ID
            rsValue = (rsValue << 12) + m_grpVchNoB;                                // Group B - Channel Number
            rsValue = (rsValue << 16) + m_dstIdB;                                   // Group B - Talkgroup Address
            break;
        case LCO::PRIVATE:
            rsValue = m_mfId;                                                       // Manufacturer ID
            rsValue = (rsValue << 8) +
                (m_emergency ? 0x80U : 0x00U) +                                     // Emergency Flag
                (m_encrypted ? 0x40U : 0x00U) +                                     // Encrypted Flag
                (m_priority & 0x07U);                                               // Priority
            rsValue = (rsValue << 24) + m_dstId;                                    // Target Radio Address
            rsValue = (rsValue << 24) + m_srcId;                                    // Source Radio Address
            break;
        case LCO::TEL_INT_VCH_USER:
            rs[0U] |= 0x40U;                                                        // Implicit Operation
            rsValue = (rsValue << 8) +
                (m_emergency ? 0x80U : 0x00U) +                                     // Emergency Flag
                (m_encrypted ? 0x40U : 0x00U) +                                     // Encrypted Flag
                (m_priority & 0x07U);                                               // Priority
            rsValue = (rsValue << 16) + m_callTimer;                                // Call Timer
            rsValue = (rsValue << 24) + m_srcId;                                    // Source/Target Radio Address
            break;
        case LCO::EXPLICIT_SOURCE_ID:
            rs[0U] |= 0x40U;                                                        // Implicit Operation
            rsValue = (rsValue << 8) + m_netId;                                     // Network ID
            rsValue = (rsValue << 12) + (m_sysId & 0xFFFU);                         // System ID
            rsValue = (rsValue << 24) + m_srcId;                                    // Source Radio Address
            break;
        case LCO::PRIVATE_EXT:
            rs[0U] |= 0x40U;                                                        // Implicit Operation
            rsValue = (m_explicitId ? 0x01U : 0x00U);                               // Explicit Source ID Flag
            rsValue = (rsValue << 8) +
                (m_emergency ? 0x80U : 0x00U) +                                     // Emergency Flag
                (m_encrypted ? 0x40U : 0x00U) +                                     // Encrypted Flag
                (m_priority & 0x07U);                                               // Priority
            rsValue = (rsValue << 24) + m_dstId;                                    // Target Radio Address
            rsValue = (rsValue << 24) + m_srcId;                                    // Source Radio Address
            break;
        case LCO::RFSS_STS_BCAST:
            rs[0U] |= 0x40U;                                                        // Implicit Operation
            rsValue = m_siteData.lra();                                             // Location Registration Area
            rsValue = (rsValue << 12) + m_siteData.sysId();                         // System ID
            rsValue = (rsValue << 8) + m_siteData.rfssId();                         // RF Sub-System ID
            rsValue = (rsValue << 8) + m_siteData.siteId();                         // Site ID
            rsValue = (rsValue << 4) + m_siteData.channelId();                      // Channel ID
            rsValue = (rsValue << 12) + m_siteData.channelNo();                     // Channel Number
            rsValue = (rsValue << 8) + m_siteData.serviceClass();                   // System Service Class
            break;
        default:
            LogError(LOG_P25, "LC::encodeLC(), unknown LC value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
            break;
        }
    } else {
        if (m_rsValue == 0U) {
            LogError(LOG_P25, "LC::encodeLC(), zero packed value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
        }

        // non-standard P25 vendor opcodes (these are just passed from the packed RS)
        rsValue = m_rsValue;
    }

    // split ulong64_t (8 byte) value into bytes
    rs[1U] = (uint8_t)((rsValue >> 56) & 0xFFU);
    rs[2U] = (uint8_t)((rsValue >> 48) & 0xFFU);
    rs[3U] = (uint8_t)((rsValue >> 40) & 0xFFU);
    rs[4U] = (uint8_t)((rsValue >> 32) & 0xFFU);
    rs[5U] = (uint8_t)((rsValue >> 24) & 0xFFU);
    rs[6U] = (uint8_t)((rsValue >> 16) & 0xFFU);
    rs[7U] = (uint8_t)((rsValue >> 8) & 0xFFU);
    rs[8U] = (uint8_t)((rsValue >> 0) & 0xFFU);
/*
    if ((m_mfId != MFG_STANDARD) && (m_mfId != MFG_STANDARD_ALT)) {
        Utils::dump(1U, "P25, LC::encodeLC(), Encoded P25 Non-Standard RS", rs, P25_LDU_LC_FEC_LENGTH_BYTES);
    }
*/
}

/*
** Encryption data 
*/

/* Sets the encryption message indicator. */

void LC::setMI(const uint8_t* mi)
{
    assert(mi != nullptr);

    ::memcpy(m_mi, mi, MI_LENGTH_BYTES);
}

/* Gets the encryption message indicator. */

void LC::getMI(uint8_t* mi) const
{
    assert(mi != nullptr);

    ::memcpy(mi, m_mi, MI_LENGTH_BYTES);
}

/*
** User Alias data
*/

/* Gets the user alias. */

std::string LC::getUserAlias() const
{
    std::string alias;
    if (m_gotUserAlias) {
        for (uint32_t i = 0; i < HARRIS_USER_ALIAS_LENGTH_BYTES; i++)
            alias[i] = m_userAlias[i];
    }

    return alias;
}

/* Sets the user alias. */

void LC::setUserAlias(std::string alias)
{
    if (m_userAlias == nullptr)
        m_userAlias = new uint8_t[HARRIS_USER_ALIAS_LENGTH_BYTES];

    ::memset(m_userAlias, 0x00U, HARRIS_USER_ALIAS_LENGTH_BYTES);
    for (uint32_t i = 0; i < HARRIS_USER_ALIAS_LENGTH_BYTES; i++)
        m_userAlias[i] = alias[i];
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void LC::copy(const LC& data)
{
    m_lco = data.m_lco;

    m_protect = data.m_protect;
    m_mfId = data.m_mfId;

    m_srcId = data.m_srcId;
    m_dstId = data.m_dstId;

    m_grpVchNo = data.m_grpVchNo;

    m_grpVchNoB = data.m_grpVchNoB;
    m_dstIdB = data.m_dstIdB;

    m_explicitId = data.m_explicitId;

    m_netId = data.m_netId;
    m_sysId = data.m_sysId;

    m_emergency = data.m_emergency;
    m_encrypted = data.m_encrypted;
    m_priority = data.m_priority;

    m_group = data.m_group;

    m_callTimer = data.m_callTimer;

    m_rsValue = data.m_rsValue;

    m_algId = data.m_algId;
    if (m_algId != ALGO_UNENCRYPT) {
        delete[] m_mi;

        m_mi = new uint8_t[MI_LENGTH_BYTES];
        ::memcpy(m_mi, data.m_mi, MI_LENGTH_BYTES);

        m_kId = data.m_kId;
        if (!m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = true;
        }
    }
    else {
        delete[] m_mi;

        m_mi = new uint8_t[MI_LENGTH_BYTES];
        ::memset(m_mi, 0x00U, MI_LENGTH_BYTES);

        m_kId = 0x0000U;
        if (m_encrypted) {
            m_encryptOverride = true;
            m_encrypted = false;
        }
    }

    // do we have user alias data to copy?
    if (data.m_gotUserAlias && data.m_userAlias != nullptr) {
        delete[] m_userAlias;

        m_userAlias = new uint8_t[HARRIS_USER_ALIAS_LENGTH_BYTES];
        ::memcpy(m_userAlias, data.m_userAlias, HARRIS_USER_ALIAS_LENGTH_BYTES);
        m_gotUserAlias = data.m_gotUserAlias;
    } else {
        delete[] m_userAlias;

        m_userAlias = new uint8_t[HARRIS_USER_ALIAS_LENGTH_BYTES];
        ::memset(m_userAlias, 0x00U, HARRIS_USER_ALIAS_LENGTH_BYTES);
        m_gotUserAlias = false;
    }

    m_siteData = data.m_siteData;
}

/* Decode LDU hamming FEC. */

void LC::decodeLDUHamming(const uint8_t* data, uint8_t* raw)
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

/* Encode LDU hamming FEC. */

void LC::encodeLDUHamming(uint8_t* data, const uint8_t* raw)
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

/* Decode HDU Golay FEC. */

void LC::decodeHDUGolay(const uint8_t* data, uint8_t* raw)
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

        uint32_t c0data = 0U;
        edac::Golay24128::decode24128(g0, c0data);

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

/* Encode HDU Golay FEC. */

void LC::encodeHDUGolay(uint8_t* data, const uint8_t* raw)
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
