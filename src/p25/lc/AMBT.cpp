/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/lc/AMBT.h"
#include "edac/CRC.h"
#include "HostMain.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the AMBT class.
/// </summary>
AMBT::AMBT() : TSBK()
{
    /* stub */
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool AMBT::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    LogError(LOG_P25, "AMBT::decode(), bad call, not implemented");

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void AMBT::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    LogError(LOG_P25, "AMBT::encode(), bad call, not implemented");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to convert TSBK bytes to a 64-bit long value.
/// </summary>
/// <param name="tsbk"></param>
/// <returns></returns>
ulong64_t AMBT::toValue(const data::DataHeader& dataHeader, const uint8_t* pduUserData)
{
    ulong64_t tsbkValue = 0U;

    // combine bytes into ulong64_t (8 byte) value
    tsbkValue = dataHeader.getAMBTField8();
    tsbkValue = (tsbkValue << 8) + dataHeader.getAMBTField9();
    tsbkValue = (tsbkValue << 8) + pduUserData[0U];
    tsbkValue = (tsbkValue << 8) + pduUserData[1U];
    tsbkValue = (tsbkValue << 8) + pduUserData[2U];
    tsbkValue = (tsbkValue << 8) + pduUserData[3U];
    tsbkValue = (tsbkValue << 8) + pduUserData[4U];
    tsbkValue = (tsbkValue << 8) + pduUserData[5U];

    return tsbkValue;
}

/// <summary>
/// Internal helper to decode a trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <param name="pduUserData"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool AMBT::decode(const data::DataHeader& dataHeader, const data::DataBlock* blocks, uint8_t* pduUserData)
{
    assert(blocks != nullptr);
    assert(pduUserData != nullptr);

    if (dataHeader.getFormat() != PDU_FMT_AMBT) {
        LogError(LOG_P25, "TSBK::decodeMBT(), PDU is not a AMBT PDU");
        return false;
    }

    if (dataHeader.getBlocksToFollow() == 0U) {
        LogError(LOG_P25, "TSBK::decodeMBT(), PDU contains no data blocks");
        return false;
    }

    m_lco = dataHeader.getAMBTOpcode();                                             // LCO
    m_lastBlock = true;
    m_mfId = dataHeader.getMFId();                                                  // Mfg Id.

    if (dataHeader.getOutbound()) {
        LogWarning(LOG_P25, "TSBK::decodeMBT(), MBT is an outbound MBT?, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
    }

    // get PDU block data
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    uint32_t dataOffset = 0U;
    for (uint8_t i = 0; i < dataHeader.getBlocksToFollow(); i++) {
        uint32_t len = blocks[i].getData(pduUserData + dataOffset);
        if (len != P25_PDU_UNCONFIRMED_LENGTH_BYTES) {
            LogError(LOG_P25, "TSBK::decodeMBT(), failed to read PDU data block");
            return false;
        }

        dataOffset += P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    return true;
}

/// <summary>
/// Internal helper to encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="pduUserData"></param>
void AMBT::encode(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    dataHeader.setFormat(PDU_FMT_AMBT);
    dataHeader.setMFId(m_mfId);
    dataHeader.setAckNeeded(false);
    dataHeader.setOutbound(true);
    dataHeader.setSAP(PDU_SAP_TRUNK_CTRL);
    dataHeader.setLLId(m_srcId);
    dataHeader.setFullMessage(true);

    if (dataHeader.getBlocksToFollow() == 0U) {
        dataHeader.setBlocksToFollow(1U);
    }

    dataHeader.setAMBTOpcode(m_lco);

    // generate packet CRC-32 and set data blocks
    if (dataHeader.getBlocksToFollow() > 1U) {
        edac::CRC::addCRC32(pduUserData, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());
    } else {
        edac::CRC::addCRC32(pduUserData, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
    }
}
