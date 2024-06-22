// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2020 Jonathan Naylor, G4KLX
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "nxdn/NXDNDefines.h"
#include "nxdn/NXDNUtils.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::defines;

#include <cassert>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const uint8_t SCRAMBLER[] = {
    0x00U, 0x00U, 0x00U, 0x82U, 0xA0U, 0x88U, 0x8AU, 0x00U, 0xA2U, 0xA8U, 0x82U, 0x8AU, 0x82U, 0x02U,
    0x20U, 0x08U, 0x8AU, 0x20U, 0xAAU, 0xA2U, 0x82U, 0x08U, 0x22U, 0x8AU, 0xAAU, 0x08U, 0x28U, 0x88U,
    0x28U, 0x28U, 0x00U, 0x0AU, 0x02U, 0x82U, 0x20U, 0x28U, 0x82U, 0x2AU, 0xAAU, 0x20U, 0x22U, 0x80U,
    0xA8U, 0x8AU, 0x08U, 0xA0U, 0xAAU, 0x02U };

// ---------------------------------------------------------------------------
//  Static Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Helper to scramble the NXDN frame data.
/// </summary>
/// <param name="data"></param>
void NXDNUtils::scrambler(uint8_t* data)
{
    assert(data != nullptr);

    for (uint32_t i = 0U; i < NXDN_FRAME_LENGTH_BYTES; i++)
        data[i] ^= SCRAMBLER[i];
}

/// <summary>
/// Helper to add the post field bits on NXDN frame data.
/// </summary>
/// <param name="data"></param>
void NXDNUtils::addPostBits(uint8_t* data)
{
    assert(data != nullptr);

    // post field
    for (uint32_t i = 0U; i < NXDN_CAC_E_POST_FIELD_BITS; i++) {
        uint32_t n = i + NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_CAC_FEC_LENGTH_BITS + NXDN_CAC_E_POST_FIELD_BITS;
        bool b = READ_BIT(NXDN_PREAMBLE, i);
        WRITE_BIT(data, n, b);
    }
}
