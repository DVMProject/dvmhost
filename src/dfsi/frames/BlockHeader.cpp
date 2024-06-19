// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - DFSI Peer Application
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI Peer Application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/

#include "frames/BlockHeader.h"
#include "common/p25/dfsi/DFSIDefines.h"
#include "common/Utils.h"
#include "common/Log.h"

#include <cassert>
#include <cstring>

using namespace p25;
using namespace p25::dfsi;

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a instance of the BlockHeader class.
/// </summary>
BlockHeader::BlockHeader() :
    m_payloadType(false),
    m_blockLength(UNDEFINED)
{
    /* stub */
}

/// <summary>
/// Initializes a instance of the BlockHeader class.
/// </summary>
/// <param name="data"></param>
/// <param name="verbose"></param>
BlockHeader::BlockHeader(uint8_t* data, bool verbose) :
    m_payloadType(false),
    m_blockLength(UNDEFINED)
{
    decode(data, verbose);
}

/// <summary>
/// Decode a block header frame.
/// </summary>
/// <param name="data"></param>
/// <param name="verbose"></param>
/// <returns></returns>
bool BlockHeader::decode(const uint8_t* data, bool verbose)
{
    assert(data != nullptr);

    uint64_t value = 0U;

    // combine bytes into ulong (8 byte) value
    value = data[0U];
    value = (value << 8) + data[1U];
    value = (value << 8) + data[2U];
    value = (value << 8) + data[3U];

    m_payloadType = (data[0U] & 0x80U) == 0x80U;                // Payload Type
    m_blockType = (BlockType)(data[0U] & 0x7FU);                // Block Type

    if (verbose) {
        m_timestampOffset = (uint32_t)((value >> 10) & 0x3FFU); // Timestamp Offset
        m_blockLength = (uint32_t)(value & 0x3FFU);             // Block Length
    }

    return true;
}

/// <summary>
/// Encode a block header frame.
/// </summary>
/// <param name="data"></param>
/// <param name="verbose"></param>
void BlockHeader::encode(uint8_t* data, bool verbose)
{
    assert(data != nullptr);

    if (!verbose) {
        data[0U] = (uint8_t)((m_payloadType ? 0x80U : 0x00U) +  // Payload Type
            ((uint8_t)m_blockType & 0x7FU));                    // Block Type
    }
    else {
        uint64_t value = 0;

        value = (uint8_t)((m_payloadType ? 0x80U : 0x00U) +     // Payload Type
            ((uint8_t)m_blockType & 0x7FU));                    // Block Type
        value = (value << 24) + (m_timestampOffset & 0x3FFU);   // Timestamp Offset
        value = (value << 10) + (m_blockLength & 0x3FFU);       // Block Length

        // split ulong (8 byte) value into bytes
        data[0U] = (uint8_t)((value >> 24) & 0xFFU);
        data[1U] = (uint8_t)((value >> 16) & 0xFFU);
        data[2U] = (uint8_t)((value >> 8) & 0xFFU);
        data[3U] = (uint8_t)((value >> 0) & 0xFFU);
    }
}
