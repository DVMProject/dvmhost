// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 */
#include "Defines.h"
#include "dmr/data/EMB.h"
#include "edac/QR1676.h"

using namespace dmr::data;
using namespace dmr;

#include <cstdio>
#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the EMB class. */
EMB::EMB() :
    m_colorCode(0U),
    m_PI(false),
    m_LCSS(0U)
{
    /* stub */
}

/* Finalizes a instance of the EMB class. */
EMB::~EMB() = default;

/* Decodes DMR embedded signalling data. */
void EMB::decode(const uint8_t* data)
{
    assert(data != nullptr);

    uint8_t DMREMB[2U];
    DMREMB[0U] = (data[13U] << 4) & 0xF0U;
    DMREMB[0U] |= (data[14U] >> 4) & 0x0FU;
    DMREMB[1U] = (data[18U] << 4) & 0xF0U;
    DMREMB[1U] |= (data[19U] >> 4) & 0x0FU;

    // decode QR (16,7,6) FEC
    edac::QR1676::decode(DMREMB);

    m_colorCode = (DMREMB[0U] >> 4) & 0x0FU;
    m_PI = (DMREMB[0U] & 0x08U) == 0x08U;
    m_LCSS = (DMREMB[0U] >> 1) & 0x03U;
}

/* Encodes DMR embedded signalling data. */
void EMB::encode(uint8_t* data) const
{
    assert(data != nullptr);

    uint8_t DMREMB[2U];
    DMREMB[0U] = (m_colorCode << 4) & 0xF0U;
    DMREMB[0U] |= m_PI ? 0x08U : 0x00U;
    DMREMB[0U] |= (m_LCSS << 1) & 0x06U;
    DMREMB[1U] = 0x00U;

    // encode QR (16,7,6) FEC
    edac::QR1676::encode(DMREMB);

    data[13U] = (data[13U] & 0xF0U) | ((DMREMB[0U] >> 4U) & 0x0FU);
    data[14U] = (data[14U] & 0x0FU) | ((DMREMB[0U] << 4U) & 0xF0U);
    data[18U] = (data[18U] & 0xF0U) | ((DMREMB[1U] >> 4U) & 0x0FU);
    data[19U] = (data[19U] & 0x0FU) | ((DMREMB[1U] << 4U) & 0xF0U);
}
