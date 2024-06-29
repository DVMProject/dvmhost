// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/dmr/SlotType.h"
#include "common/edac/Golay2087.h"

using namespace dmr;
using namespace dmr::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the SlotType class. */

SlotType::SlotType() :
    m_colorCode(0U),
    m_dataType(DataType::IDLE)
{
    /* stub */
}

/* Finalizes a instance of the SlotType class. */

SlotType::~SlotType() = default;

/* Decodes DMR slot type. */

void SlotType::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t slotType[3U];
    slotType[0U] = (data[12U] << 2) & 0xFCU;
    slotType[0U] |= (data[13U] >> 6) & 0x03U;

    slotType[1U] = (data[13U] << 2) & 0xC0U;
    slotType[1U] |= (data[19U] << 2) & 0x3CU;
    slotType[1U] |= (data[20U] >> 6) & 0x03U;

    slotType[2U] = (data[20U] << 2) & 0xF0U;

    uint8_t code = edac::Golay2087::decode(slotType);

    m_colorCode = (code >> 4) & 0x0FU;
    m_dataType = (DataType::E)((code >> 0) & 0x0FU);
}

/* Encodes DMR slot type. */

void SlotType::encode(uint8_t* data) const
{
    assert(data != nullptr);

    uint8_t slotType[3U];
    slotType[0U] = (m_colorCode << 4) & 0xF0U;
    slotType[0U] |= (m_dataType << 0) & 0x0FU;
    slotType[1U] = 0x00U;
    slotType[2U] = 0x00U;

    edac::Golay2087::encode(slotType);

    data[12U] = (data[12U] & 0xC0U) | ((slotType[0U] >> 2) & 0x3FU);
    data[13U] = (data[13U] & 0x0FU) | ((slotType[0U] << 6) & 0xC0U) | ((slotType[1U] >> 2) & 0x30U);
    data[19U] = (data[19U] & 0xF0U) | ((slotType[1U] >> 2) & 0x0FU);
    data[20U] = (data[20U] & 0x03U) | ((slotType[1U] << 6) & 0xC0U) | ((slotType[2U] >> 2) & 0x3CU);
}
