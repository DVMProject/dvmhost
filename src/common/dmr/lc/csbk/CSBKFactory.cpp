// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "dmr/lc/csbk/CSBKFactory.h"
#include "edac/BPTC19696.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;
using namespace dmr::lc::csbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the CSBKFactory class.
/// </summary>
CSBKFactory::CSBKFactory() = default;

/// <summary>
/// Finalizes a instance of CSBKFactory class.
/// </summary>
CSBKFactory::~CSBKFactory() = default;

/// <summary>
/// Create an instance of a CSBK.
/// </summary>
/// <param name="data"></param>
/// <param name="dataType"></param>
/// <returns>True, if CSBK was decoded, otherwise false.</returns>
std::unique_ptr<CSBK> CSBKFactory::createCSBK(const uint8_t* data, DataType::E dataType)
{
    assert(data != nullptr);

    uint8_t csbk[DMR_CSBK_LENGTH_BYTES];

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
            LogError(LOG_DMR, "CSBKFactory::createCSBK(), unhandled dataType = $%02X", dataType);
            break;
    }

    bool valid = edac::CRC::checkCCITT162(csbk, DMR_CSBK_LENGTH_BYTES);
    if (!valid) {
        LogError(LOG_DMR, "CSBKFactory::createCSBK(), failed CRC CCITT-162 check");
        return nullptr;
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
            LogError(LOG_DMR, "CSBKFactory::createCSBK(), unhandled dataType = $%02X", dataType);
            break;
    }

    uint8_t CSBKO = csbk[0U] & 0x3FU;                                               // CSBKO
    uint8_t FID = csbk[1U];                                                         // Feature ID

    switch (CSBKO) {
    case CSBKO::BSDWNACT:
        return decode(new CSBK_BSDWNACT(), data);
    case CSBKO::UU_V_REQ:
        return decode(new CSBK_UU_V_REQ(), data);
    case CSBKO::UU_ANS_RSP:
        return decode(new CSBK_UU_ANS_RSP(), data);
    case CSBKO::PRECCSBK:
        return decode(new CSBK_PRECCSBK(), data);
    case CSBKO::RAND: // CSBKO::CALL_ALRT when FID == FID_DMRA
        switch (FID)
        {
        case FID_DMRA:
            return decode(new CSBK_CALL_ALRT(), data);
        case FID_ETSI:
        default:
            return decode(new CSBK_RAND(), data);
        }
    case CSBKO::EXT_FNCT:
        return decode(new CSBK_EXT_FNCT(), data);
    case CSBKO::NACK_RSP:
        return decode(new CSBK_NACK_RSP(), data);

    /** Tier 3 */
    case CSBKO::ACK_RSP:
        return decode(new CSBK_ACK_RSP(), data);
    case CSBKO::BROADCAST:
        return decode(new CSBK_BROADCAST(), data);
    case CSBKO::MAINT:
        return decode(new CSBK_MAINT(), data);

    default:
        LogError(LOG_DMR, "CSBKFactory::create(), unknown CSBK type, csbko = $%02X", CSBKO);
        break;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
///
/// </summary>
/// <param name="csbk"></param>
/// <param name="data"></param>
/// <returns></returns>
std::unique_ptr<CSBK> CSBKFactory::decode(CSBK* csbk, const uint8_t* data)
{
    assert(csbk != nullptr);
    assert(data != nullptr);

    if (!csbk->decode(data)) {
        return nullptr;
    }

    return std::unique_ptr<CSBK>(csbk);
}
