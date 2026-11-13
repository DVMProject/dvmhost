// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "dmr/lc/csbk/CSBK_AHOY.h"

using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the CSBK_AHOY class. */

CSBK_AHOY::CSBK_AHOY() : CSBK(),
    m_serviceOptionsMirror(0U),
    m_serviceKindFlag(false),
    m_ambientListening(false),
    m_appendedBlocks(0U),
    m_serviceKind(0U)
{
    m_CSBKO = CSBKO::AHOY;
}

/* Decode a control signalling block. */

bool CSBK_AHOY::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];
    ::memset(csbk, 0x00U, DMR_CSBK_LENGTH_BYTES);

    bool ret = CSBK::decode(data, csbk);
    if (!ret)
        return false;

    ulong64_t csbkValue = CSBK::toValue(csbk);

    m_serviceOptionsMirror = (uint8_t)((csbkValue >> 57U) & 0x7FU);  // 7-bit Service Options Mirror
    m_serviceKindFlag = ((csbkValue >> 56U) & 0x01U) != 0U;          // Service Kind Flag
    m_ambientListening = ((csbkValue >> 55U) & 0x01U) != 0U;         // Ambient Listening Flag
    m_GI = ((csbkValue >> 54U) & 0x01U) != 0U;                       // GI Flag
    m_appendedBlocks = (uint8_t)((csbkValue >> 52U) & 0x03U);        // Appended Blocks
    m_serviceKind = (uint8_t)((csbkValue >> 48U) & 0x0FU);           // Service Kind
    m_dstId = (uint32_t)((csbkValue >> 24U) & 0xFFFFFFU);            // Destination ID
    m_srcId = (uint32_t)(csbkValue & 0xFFFFFFU);                     // Source ID

    return true;
}

/* Encode a control signalling block. */

void CSBK_AHOY::encode(uint8_t* data)
{
    assert(data != nullptr);

    ulong64_t csbkValue = 0U;

    csbkValue = m_serviceOptionsMirror & 0x7FU;                     // 7-bit Service Options Mirror
    csbkValue = (csbkValue << 1U) | (m_serviceKindFlag ? 1U : 0U);  // Service Kind Flag
    csbkValue = (csbkValue << 1U) | (m_ambientListening ? 1U : 0U); // Ambient Listening Flag
    csbkValue = (csbkValue << 1U) | (m_GI ? 1U : 0U);               // GI Flag
    csbkValue = (csbkValue << 2U) | (m_appendedBlocks & 0x03U);     // Appended Blocks
    csbkValue = (csbkValue << 4U) | (m_serviceKind & 0x0FU);        // Service Kind
    csbkValue = (csbkValue << 24U) | (m_dstId & 0xFFFFFFU);         // Destination ID
    csbkValue = (csbkValue << 24U) | (m_srcId & 0xFFFFFFU);         // Source ID

    std::unique_ptr<uint8_t[]> csbk = CSBK::fromValue(csbkValue);
    CSBK::encode(data, csbk.get());
}

/* Returns a string that represents the current CSBK. */

std::string CSBK_AHOY::toString()
{
    return std::string("CSBKO, AHOY (Enquiry from TSCC)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void CSBK_AHOY::copy(const CSBK_AHOY& data)
{
    CSBK::copy(data);

    m_serviceOptionsMirror = data.m_serviceOptionsMirror;
    m_serviceKindFlag = data.m_serviceKindFlag;
    m_ambientListening = data.m_ambientListening;
    m_appendedBlocks = data.m_appendedBlocks;
    m_serviceKind = data.m_serviceKind;
}
