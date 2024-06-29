// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * @package DVM / Common Library
 * @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/OSP_U_REG_CMD.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the OSP_U_REG_CMD class. */
OSP_U_REG_CMD::OSP_U_REG_CMD() : TSBK()
{
    m_lco = TSBKO::OSP_U_REG_CMD;
}

/* Decode a trunking signalling block. */
bool OSP_U_REG_CMD::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    /* stub */

    return true;
}

/* Encode a trunking signalling block. */
void OSP_U_REG_CMD::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    ulong64_t tsbkValue = 0U;

    tsbkValue = (tsbkValue << 24) + m_dstId;                                        // Target Radio Address
    tsbkValue = (tsbkValue << 24) + m_srcId;                                        // Source Radio Address

    std::unique_ptr<uint8_t[]> tsbk = TSBK::fromValue(tsbkValue);
    TSBK::encode(data, tsbk.get(), rawTSBK, noTrellis);
}

/* Returns a string that represents the current TSBK. */
std::string OSP_U_REG_CMD::toString(bool isp)
{
    return std::string("TSBKO, OSP_U_REG_CMD (Unit Registration Command)");
}
