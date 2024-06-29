// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/lc/TDULC.h"
#include "p25/P25Utils.h"
#include "edac/Golay24128.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

bool TDULC::m_verbose = false;

SiteData TDULC::m_siteData = SiteData();

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a copy instance of the TDULC class. */
TDULC::TDULC(const TDULC& data) : TDULC()
{
    copy(data);
}

/* Initializes a new instance of the TDULC class. */
TDULC::TDULC(LC* lc) : TDULC()
{
    m_protect = lc->m_protect;
    m_lco = lc->m_lco;
    m_mfId = lc->m_mfId;

    m_srcId = lc->m_srcId;
    m_dstId = lc->m_dstId;

    m_grpVchNo = lc->m_grpVchNo;

    m_emergency = lc->m_emergency;
    m_encrypted = lc->m_encrypted;
    m_priority = lc->m_priority;

    m_group = lc->m_group;

    m_callTimer = lc->m_callTimer;
}

/* Initializes a new instance of the TDULC class. */
TDULC::TDULC() :
    m_protect(false),
    m_lco(LCO::GROUP),
    m_mfId(MFG_STANDARD),
    m_srcId(0U),
    m_dstId(0U),
    m_grpVchNo(0U),
    m_emergency(false),
    m_encrypted(false),
    m_priority(4U),
    m_group(true),
    m_siteIdenEntry(lookups::IdenTable()),
    m_rs(),
    m_implicit(false),
    m_callTimer(0U)
{
    m_grpVchNo = m_siteData.channelNo();
}

/* Finalizes a instance of TDULC class. */
TDULC::~TDULC()
{
    /* stub */
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to convert payload bytes to a 64-bit long value. */
ulong64_t TDULC::toValue(const uint8_t* payload)
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

/* Internal helper to convert a 64-bit long value to payload bytes. */
UInt8Array TDULC::fromValue(const ulong64_t value)
{
    UInt8Array payload = std::unique_ptr<uint8_t[]>(new uint8_t[P25_TDULC_PAYLOAD_LENGTH_BYTES]);
    ::memset(payload.get(), 0x00U, P25_TDULC_PAYLOAD_LENGTH_BYTES);

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

/* Internal helper to decode a terminator data unit w/ link control. */
bool TDULC::decode(const uint8_t* data, uint8_t* payload)
{
    assert(data != nullptr);
    assert(payload != nullptr);

    uint8_t rs[P25_TDULC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_TDULC_LENGTH_BYTES);

    // deinterleave
    uint8_t raw[P25_TDULC_FEC_LENGTH_BYTES + 1U];
    P25Utils::decode(data, raw, 114U, 410U);

    // decode Golay (24,12,8) FEC
    edac::Golay24128::decode24128(rs, raw, P25_TDULC_LENGTH_BYTES);

#if DEBUG_P25_TDULC
    Utils::dump(2U, "TDULC::decode(), TDULC RS", rs, P25_TDULC_LENGTH_BYTES);
#endif

    // decode RS (24,12,13) FEC
    try {
        bool ret = m_rs.decode241213(rs);
        if (!ret) {
            LogError(LOG_P25, "TDULC::decode(), failed to decode RS (24,12,13) FEC");
            return false;
        }
    }
    catch (...) {
        Utils::dump(2U, "P25, RS excepted with input data", rs, P25_TDULC_LENGTH_BYTES);
        return false;
    }

    if (m_verbose) {
        Utils::dump(2U, "TDULC::decode(), TDULC Value", rs, P25_TDULC_LENGTH_BYTES);
    }

    ::memcpy(payload, rs + 1U, P25_TDULC_PAYLOAD_LENGTH_BYTES);
    return true;
}

/* Internal helper to encode a terminator data unit w/ link control. */
void TDULC::encode(uint8_t* data, const uint8_t* payload)
{
    assert(data != nullptr);
    assert(payload != nullptr);

    uint8_t rs[P25_TDULC_LENGTH_BYTES];
    ::memset(rs, 0x00U, P25_TDULC_LENGTH_BYTES);
    ::memcpy(rs + 1U, payload, P25_TDULC_PAYLOAD_LENGTH_BYTES);

    rs[0U] = m_lco;                                                                 // LCO
    if (m_implicit)
        rs[0U] |= 0x40U;                                                            // Implicit Operation

    if (m_verbose) {
        Utils::dump(2U, "TDULC::encode(), TDULC Value", rs, P25_TDULC_LENGTH_BYTES);
    }

    // encode RS (24,12,13) FEC
    m_rs.encode241213(rs);

#if DEBUG_P25_TDULC
    Utils::dump(2U, "TDULC::encode(), TDULC RS", rs, P25_TDULC_LENGTH_BYTES);
#endif

    uint8_t raw[P25_TDULC_FEC_LENGTH_BYTES + 1U];
    ::memset(raw, 0x00U, P25_TDULC_FEC_LENGTH_BYTES + 1U);

    // encode Golay (24,12,8) FEC
    edac::Golay24128::encode24128(raw, rs, P25_TDULC_LENGTH_BYTES);

    // interleave
    P25Utils::encode(raw, data, 114U, 410U);

#if DEBUG_P25_TDULC
    Utils::dump(2U, "TDULC::encode(), TDULC Interleave", data, P25_TDULC_FRAME_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
}

/* Internal helper to copy the the class. */
void TDULC::copy(const TDULC& data)
{
    m_verbose = data.m_verbose;
    m_protect = data.m_protect;
    m_lco = data.m_lco;
    m_mfId = data.m_mfId;

    m_srcId = data.m_srcId;
    m_dstId = data.m_dstId;

    m_grpVchNo = data.m_grpVchNo;

    m_emergency = data.m_emergency;
    m_encrypted = data.m_encrypted;
    m_priority = data.m_priority;

    m_group = data.m_group;

    m_callTimer = data.m_callTimer;

    m_siteData = data.m_siteData;
    m_siteIdenEntry = data.m_siteIdenEntry;
}
