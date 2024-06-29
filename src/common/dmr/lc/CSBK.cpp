// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2019-2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/lc/CSBK.h"
#include "edac/BPTC19696.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

bool CSBK::m_verbose = false;

SiteData CSBK::m_siteData = SiteData();

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a copy instance of the CSBK class. */

CSBK::CSBK(const CSBK& data) : CSBK()
{
    copy(data);
}

/* Initializes a new instance of the CSBK class. */

CSBK::CSBK() :
    m_colorCode(0U),
    m_lastBlock(true),
    m_Cdef(false),
    m_CSBKO(CSBKO::NONE),
    m_FID(0x00U),
    m_GI(false),
    m_srcId(0U),
    m_dstId(0U),
    m_dataContent(false),
    m_CBF(0U),
    m_dataType(DataType::CSBK),
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
    m_siteIdenEntry(::lookups::IdenTable()),
    m_raw(nullptr)
{
    /* stub */
}

/* Finalizes a instance of the CSBK class. */

CSBK::~CSBK()
{
    if (m_raw != nullptr)
        delete[] m_raw;
}

/* Returns a string that represents the current CSBK. */

std::string CSBK::toString()
{
    return std::string("CSBKO, UNKNOWN (Unknown CSBK)");
}

/* Returns a copy of the raw decoded CSBK bytes. */

uint8_t* CSBK::getDecodedRaw() const
{
    return m_raw;
}

/* Regenerate a DMR CSBK without decoding. */

bool CSBK::regenerate(uint8_t* data, uint8_t dataType)
{
    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(data, csbk);

    // validate the CRC-CCITT 16
    switch (dataType) {
        case DataType::CSBK:
            csbk[10U] ^= CSBK_CRC_MASK[0U];
            csbk[11U] ^= CSBK_CRC_MASK[1U];
            break;
        case DataType::MBC_HEADER:
            csbk[10U] ^= CSBK_MBC_CRC_MASK[0U];
            csbk[11U] ^= CSBK_MBC_CRC_MASK[1U];
            break;
        default:
            LogError(LOG_DMR, "CSBK::regenerate(), unhandled dataType = $%02X", dataType);
            break;
    }

    bool valid = edac::CRC::checkCCITT162(csbk, DMR_CSBK_LENGTH_BYTES);
    if (!valid) {
        LogError(LOG_DMR, "CSBK::regenerate(), failed CRC CCITT-162 check");
        return false;
    }

    // restore the checksum
    switch (dataType) {
        case DataType::CSBK:
            csbk[10U] ^= CSBK_CRC_MASK[0U];
            csbk[11U] ^= CSBK_CRC_MASK[1U];
            break;
        case DataType::MBC_HEADER:
            csbk[10U] ^= CSBK_MBC_CRC_MASK[0U];
            csbk[11U] ^= CSBK_MBC_CRC_MASK[1U];
            break;
        default:
            LogError(LOG_DMR, "CSBK::regenerate(), unhandled dataType = $%02X", dataType);
            break;
    }

    // calculate checksum
    switch (dataType) {
        case DataType::CSBK:
            csbk[10U] ^= CSBK_CRC_MASK[0U];
            csbk[11U] ^= CSBK_CRC_MASK[1U];
            break;
        case DataType::MBC_HEADER:
            csbk[10U] ^= CSBK_MBC_CRC_MASK[0U];
            csbk[11U] ^= CSBK_MBC_CRC_MASK[1U];
            break;
        default:
            LogError(LOG_DMR, "CSBK::regenerate(), unhandled dataType = $%02X", dataType);
            break;
    }

    edac::CRC::addCCITT162(csbk, 12U);

    switch (dataType) {
        case DataType::CSBK:
            csbk[10U] ^= CSBK_CRC_MASK[0U];
            csbk[11U] ^= CSBK_CRC_MASK[1U];
            break;
        case DataType::MBC_HEADER:
            csbk[10U] ^= CSBK_MBC_CRC_MASK[0U];
            csbk[11U] ^= CSBK_MBC_CRC_MASK[1U];
            break;
        default:
            LogError(LOG_DMR, "CSBK::regenerate(), unhandled dataType = $%02X", dataType);
            break;
    }

    // encode BPTC (196,96) FEC
    bptc.encode(csbk, data);

    return true;
}

// ---------------------------------------------------------------------------
//  Protected Class Members
// ---------------------------------------------------------------------------

/* Internal helper to convert payload bytes to a 64-bit long value. */

ulong64_t CSBK::toValue(const uint8_t* payload)
{
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

UInt8Array CSBK::fromValue(const ulong64_t value)
{
    UInt8Array payload = std::unique_ptr<uint8_t[]>(new uint8_t[DMR_CSBK_LENGTH_BYTES - 4U]);
    ::memset(payload.get(), 0x00U, DMR_CSBK_LENGTH_BYTES - 4U);

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

/* Internal helper to decode a control signalling block. */

bool CSBK::decode(const uint8_t* data, uint8_t* payload)
{
    assert(data != nullptr);
    assert(payload != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    // decode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.decode(data, csbk);

    // validate the CRC-CCITT 16
    switch (m_dataType) {
        case DataType::CSBK:
            csbk[10U] ^= CSBK_CRC_MASK[0U];
            csbk[11U] ^= CSBK_CRC_MASK[1U];
            break;
        case DataType::MBC_HEADER:
            csbk[10U] ^= CSBK_MBC_CRC_MASK[0U];
            csbk[11U] ^= CSBK_MBC_CRC_MASK[1U];
            break;
        default:
            LogError(LOG_DMR, "CSBK::decode(), unhandled dataType = $%02X", m_dataType);
            break;
    }

    bool valid = edac::CRC::checkCCITT162(csbk, DMR_CSBK_LENGTH_BYTES);
    if (!valid) {
        LogError(LOG_DMR, "CSBK::decode(), failed CRC CCITT-162 check");
        return false;
    }

    // restore the checksum
    switch (m_dataType) {
        case DataType::CSBK:
            csbk[10U] ^= CSBK_CRC_MASK[0U];
            csbk[11U] ^= CSBK_CRC_MASK[1U];
            break;
        case DataType::MBC_HEADER:
            csbk[10U] ^= CSBK_MBC_CRC_MASK[0U];
            csbk[11U] ^= CSBK_MBC_CRC_MASK[1U];
            break;
        default:
            LogError(LOG_DMR, "CSBK::decode(), unhandled dataType = $%02X", m_dataType);
            break;
    }

    if (m_verbose) {
        Utils::dump(2U, "Decoded CSBK", csbk, DMR_CSBK_LENGTH_BYTES);
    }

    m_raw = new uint8_t[DMR_CSBK_LENGTH_BYTES];
    ::memcpy(m_raw, csbk, DMR_CSBK_LENGTH_BYTES);

    m_CSBKO = csbk[0U] & 0x3FU;                                                     // CSBKO
    m_lastBlock = (csbk[0U] & 0x80U) == 0x80U;                                      // Last Block Marker
    m_FID = csbk[1U];                                                               // Feature ID

    m_dataContent = false;
    m_CBF = 0U;

    ::memcpy(payload, csbk + 2U, DMR_CSBK_LENGTH_BYTES - 4U);
    return true;
}

/* Internal helper to encode a control signalling block. */

void CSBK::encode(uint8_t* data, const uint8_t* payload)
{
    assert(data != nullptr);
    assert(payload != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);
    ::memcpy(csbk + 2U, payload, DMR_CSBK_LENGTH_BYTES - 4U);

    csbk[0U] = m_CSBKO;                                                             // CSBKO
    csbk[0U] |= (m_lastBlock) ? 0x80U : 0x00U;                                      // Last Block Marker
    if (!m_Cdef) {
        csbk[1U] = m_FID;                                                           // Feature ID
    }
    else {
        csbk[1U] = m_colorCode & 0x0FU;                                             // Cdef uses Color Code
    }

    switch (m_dataType) {
        case DataType::CSBK:
            csbk[10U] ^= CSBK_CRC_MASK[0U];
            csbk[11U] ^= CSBK_CRC_MASK[1U];
            break;
        case DataType::MBC_HEADER:
            csbk[10U] ^= CSBK_MBC_CRC_MASK[0U];
            csbk[11U] ^= CSBK_MBC_CRC_MASK[1U];
            break;
        default:
            LogError(LOG_DMR, "CSBK::encoded(), unhandled dataType = $%02X", m_dataType);
            break;
    }

    edac::CRC::addCCITT162(csbk, 12U);

    switch (m_dataType) {
        case DataType::CSBK:
            csbk[10U] ^= CSBK_CRC_MASK[0U];
            csbk[11U] ^= CSBK_CRC_MASK[1U];
            break;
        case DataType::MBC_HEADER:
            csbk[10U] ^= CSBK_MBC_CRC_MASK[0U];
            csbk[11U] ^= CSBK_MBC_CRC_MASK[1U];
            break;
        default:
            LogError(LOG_DMR, "CSBK::encode(), unhandled dataType = $%02X", m_dataType);
            break;
    }

    if (m_verbose) {
        Utils::dump(2U, "Encoded CSBK", csbk, DMR_CSBK_LENGTH_BYTES);
    }

    // encode BPTC (196,96) FEC
    edac::BPTC19696 bptc;
    bptc.encode(csbk, data);
}

/* Internal helper to copy the the class. */

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
