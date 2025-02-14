// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "common/p25/dfsi/frames/ControlOctet.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace p25::dfsi;
using namespace p25::dfsi::frames;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a instance of the ControlOctet class. */

ControlOctet::ControlOctet() :
    m_signal(false),
    m_compact(true),
    m_blockHeaderCnt(0U)
{
    /* stub */
}

/* Initializes a instance of the ControlOctet class. */

ControlOctet::ControlOctet(uint8_t* data) :
    m_signal(false),
    m_compact(true),
    m_blockHeaderCnt(0U)
{
    decode(data);
}

/* Decode a control octet frame. */

bool ControlOctet::decode(const uint8_t* data)
{
    assert(data != nullptr);

    m_signal = (data[0U] & 0x80U) == 0x80U;                     // Signal Flag
    m_compact = (data[0U] & 0x40U) == 0x40U;                    // Compact Flag
    m_blockHeaderCnt = (uint8_t)(data[0U] & 0x3FU);             // Block Header Count

    return true;
}

/* Encode a control octet frame. */

void ControlOctet::encode(uint8_t* data)
{
    assert(data != nullptr);

    data[0U] = (uint8_t)((m_signal ? 0x80U : 0x00U) +           // Signal Flag
            (m_compact ? 0x40U : 0x00U) +                       // Control Flag
            (m_blockHeaderCnt & 0x3F));
}
