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
*   Copyright (C) 2012 Ian Wraith
*   Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
*   Copyright (C) 2021,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "dmr/DMRDefines.h"
#include "dmr/lc/FullLC.h"
#include "edac/RS129.h"
#include "edac/CRC.h"
#include "Log.h"

using namespace dmr;
using namespace dmr::defines;
using namespace dmr::lc;

#include <cassert>
#include <memory>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initialize a new instance of the FullLC class.
/// </summary>
FullLC::FullLC() :
    m_bptc()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the FullLC class.
/// </summary>
FullLC::~FullLC() = default;

/// <summary>
/// Decode DMR full-link control data.
/// </summary>
/// <param name="data"></param>
/// <param name="type"></param>
/// <returns></returns>
std::unique_ptr<LC> FullLC::decode(const uint8_t* data, DataType::E type)
{
    assert(data != nullptr);

    // decode BPTC (196,96) FEC
    uint8_t lcData[DMR_LC_HEADER_LENGTH_BYTES];
    m_bptc.decode(data, lcData);

    switch (type) {
        case DataType::VOICE_LC_HEADER:
            lcData[9U] ^= VOICE_LC_HEADER_CRC_MASK[0U];
            lcData[10U] ^= VOICE_LC_HEADER_CRC_MASK[1U];
            lcData[11U] ^= VOICE_LC_HEADER_CRC_MASK[2U];
            break;

        case DataType::TERMINATOR_WITH_LC:
            lcData[9U] ^= TERMINATOR_WITH_LC_CRC_MASK[0U];
            lcData[10U] ^= TERMINATOR_WITH_LC_CRC_MASK[1U];
            lcData[11U] ^= TERMINATOR_WITH_LC_CRC_MASK[2U];
            break;

        default:
            LogError(LOG_DMR, "Unsupported LC type, type = %d", int(type));
            return nullptr;
    }

    // check RS (12,9) FEC
    if (!edac::RS129::check(lcData))
        return nullptr;

    return std::make_unique<LC>(lcData);
}

/// <summary>
/// Encode DMR full-link control data.
/// </summary>
/// <param name="lc"></param>
/// <param name="data"></param>
/// <param name="type"></param>
void FullLC::encode(const LC& lc, uint8_t* data, DataType::E type)
{
    assert(data != nullptr);

    uint8_t lcData[DMR_LC_HEADER_LENGTH_BYTES];
    lc.getData(lcData);

    // encode RS (12,9) FEC
    uint8_t parity[4U];
    edac::RS129::encode(lcData, 9U, parity);

    switch (type) {
        case DataType::VOICE_LC_HEADER:
            lcData[9U] = parity[2U] ^ VOICE_LC_HEADER_CRC_MASK[0U];
            lcData[10U] = parity[1U] ^ VOICE_LC_HEADER_CRC_MASK[1U];
            lcData[11U] = parity[0U] ^ VOICE_LC_HEADER_CRC_MASK[2U];
            break;

        case DataType::TERMINATOR_WITH_LC:
            lcData[9U] = parity[2U] ^ TERMINATOR_WITH_LC_CRC_MASK[0U];
            lcData[10U] = parity[1U] ^ TERMINATOR_WITH_LC_CRC_MASK[1U];
            lcData[11U] = parity[0U] ^ TERMINATOR_WITH_LC_CRC_MASK[2U];
            break;

        default:
            LogError(LOG_DMR, "Unsupported LC type, type = %d", int(type));
            return;
    }

    // encode BPTC (196,96) FEC
    m_bptc.encode(lcData, data);
}

/// <summary>
/// Decode DMR privacy control data.
/// </summary>
/// <param name="data"></param>
/// <param name="type"></param>
/// <returns></returns>
std::unique_ptr<PrivacyLC> FullLC::decodePI(const uint8_t* data)
{
    assert(data != nullptr);

    // decode BPTC (196,96) FEC
    uint8_t lcData[DMR_LC_HEADER_LENGTH_BYTES];
    m_bptc.decode(data, lcData);

    // make sure the CRC-CCITT 16 was actually included (the network tends to zero the CRC)
    if (lcData[10U] != 0x00U && lcData[11U] != 0x00U) {
        // validate the CRC-CCITT 16
        lcData[10U] ^= PI_HEADER_CRC_MASK[0U];
        lcData[11U] ^= PI_HEADER_CRC_MASK[1U];

        if (!edac::CRC::checkCCITT162(lcData, DMR_LC_HEADER_LENGTH_BYTES))
            return nullptr;

        // restore the checksum
        lcData[10U] ^= PI_HEADER_CRC_MASK[0U];
        lcData[11U] ^= PI_HEADER_CRC_MASK[1U];
    }

    return std::make_unique<PrivacyLC>(lcData);
}

/// <summary>
/// Encode DMR privacy control data.
/// </summary>
/// <param name="lc"></param>
/// <param name="data"></param>
/// <param name="type"></param>
void FullLC::encodePI(const PrivacyLC& lc, uint8_t* data)
{
    assert(data != nullptr);

    uint8_t lcData[DMR_LC_HEADER_LENGTH_BYTES];
    lc.getData(lcData);

    // compute CRC-CCITT 16
    lcData[10U] ^= PI_HEADER_CRC_MASK[0U];
    lcData[11U] ^= PI_HEADER_CRC_MASK[1U];

    edac::CRC::addCCITT162(lcData, DMR_LC_HEADER_LENGTH_BYTES);

    // restore the checksum
    lcData[10U] ^= PI_HEADER_CRC_MASK[0U];
    lcData[11U] ^= PI_HEADER_CRC_MASK[1U];

    // encode BPTC (196,96) FEC
    m_bptc.encode(lcData, data);
}
