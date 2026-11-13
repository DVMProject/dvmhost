// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024,2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/lc/AMBT.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the AMBT class. */

AMBT::AMBT() : TSBK(),
    m_isAMBT(true)
{
    /* stub */
}

/* Decode a trunking signalling block. */

bool AMBT::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != nullptr);

    LogError(LOG_P25, "AMBT::decode(), bad call, not implemented");

    return true;
}

/* Encode a trunking signalling block. */

void AMBT::encode(uint8_t* data, bool rawTSBK, bool noTrellis)
{
    assert(data != nullptr);

    LogError(LOG_P25, "AMBT::encode(), bad call, not implemented");
}

/* Determines whether a PDU header identifies an MBT packet. */

bool AMBT::isMBT(const data::DataHeader& dataHeader)
{
    return dataHeader.getSAP() == PDUSAP::TRUNK_CTRL &&
        (dataHeader.getFormat() == PDUFormatType::AMBT || dataHeader.getFormat() == PDUFormatType::UNCONFIRMED);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to convert AMBT bytes to a 64-bit long value. */

ulong64_t AMBT::toValue(const data::DataHeader& dataHeader, const uint8_t* pduUserData)
{
    ulong64_t tsbkValue = 0U;

    // combine bytes into ulong64_t (8 byte) value
    if (dataHeader.getFormat() == PDUFormatType::UNCONFIRMED) {
        // Standard MBT: data block octet 0 is the opcode and octets 1.. are message data
        for (uint32_t i = 1U; i <= 8U; i++)
            tsbkValue = (tsbkValue << 8) + pduUserData[i];
    } else {
        // Alternate MBT: data block octets are combined with header fields to form the 64-bit value
        tsbkValue = dataHeader.getAMBTField8();
        tsbkValue = (tsbkValue << 8) + dataHeader.getAMBTField9();
        tsbkValue = (tsbkValue << 8) + pduUserData[0U];
        tsbkValue = (tsbkValue << 8) + pduUserData[1U];
        tsbkValue = (tsbkValue << 8) + pduUserData[2U];
        tsbkValue = (tsbkValue << 8) + pduUserData[3U];
        tsbkValue = (tsbkValue << 8) + pduUserData[4U];
        tsbkValue = (tsbkValue << 8) + pduUserData[5U];
    }

    return tsbkValue;
}

/* Internal helper to decode a trunking signalling block. */

bool AMBT::decode(const data::DataHeader& dataHeader, const data::DataBlock* blocks, uint8_t* pduUserData)
{
    assert(blocks != nullptr);
    assert(pduUserData != nullptr);

    if (!isMBT(dataHeader)) {
        LogError(LOG_P25, "AMBT::decode(), PDU is not a standard or alternate MBT PDU");
        return false;
    }

    if (dataHeader.getBlocksToFollow() == 0U) {
        LogError(LOG_P25, "AMBT::decode(), PDU contains no data blocks");
        return false;
    }

    // determine if the MBT is standard or alternate based on the PDU format
    if (dataHeader.getFormat() == PDUFormatType::UNCONFIRMED) {
        uint8_t firstBlock[P25_PDU_UNCONFIRMED_LENGTH_BYTES];
        if (blocks[0U].getData(firstBlock) != P25_PDU_UNCONFIRMED_LENGTH_BYTES) {
            LogError(LOG_P25, "AMBT::decode(), failed to read first PDU data block");
            return false;
        }

        m_isAMBT = false;
        m_lco = firstBlock[0U] & 0x3FU;                                             // LCO
    } else {
        m_isAMBT = true;
        m_lco = dataHeader.getAMBTOpcode();                                         // LCO
    }

    m_lastBlock = true;                                                             // Last Block Marker
    m_mfId = dataHeader.getMFId();                                                  // Mfg Id.

    if (dataHeader.getOutbound()) {
        LogWarning(LOG_P25, "AMBT::decode(), MBT is an outbound MBT?, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
    }

    // get PDU block data
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    uint32_t dataOffset = 0U;
    for (uint8_t i = 0; i < dataHeader.getBlocksToFollow(); i++) {
        uint32_t len = blocks[i].getData(pduUserData + dataOffset);
        if (len != P25_PDU_UNCONFIRMED_LENGTH_BYTES) {
            LogError(LOG_P25, "AMBT::decode(), failed to read PDU data block");
            return false;
        }

        dataOffset += P25_PDU_UNCONFIRMED_LENGTH_BYTES;
    }

    if (s_verbose) {
        if (m_isAMBT) {
            LogDebugEx(LOG_P25, "AMBT::decode()", "mfId = $%02X, ambt = %u, lco = $%02X, ambt8 = $%02X, ambt9 = $%02X", m_mfId, m_isAMBT, m_lco, dataHeader.getAMBTField8(), dataHeader.getAMBTField9());
        } else {
            LogDebugEx(LOG_P25, "AMBT::decode()", "mfId = $%02X, ambt = %u, lco = $%02X", m_mfId, m_isAMBT, m_lco);
        }
        Utils::dump(2U, "P25, AMBT::decode(), pduUserData", pduUserData, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());
    }

    return true;
}

/* Internal helper to encode a trunking signalling block. */

void AMBT::encode(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    const bool standardMBT = dataHeader.getFormat() == PDUFormatType::UNCONFIRMED;
    dataHeader.setFormat(standardMBT ? PDUFormatType::UNCONFIRMED : PDUFormatType::AMBT);
    dataHeader.setMFId(m_mfId);                                                     // Mfg Id.
    dataHeader.setAckNeeded(false);
    dataHeader.setOutbound(true);
    dataHeader.setSAP(PDUSAP::TRUNK_CTRL);
    dataHeader.setLLId(m_srcId);
    dataHeader.setFullMessage(true);

    if (dataHeader.getBlocksToFollow() == 0U) {
        dataHeader.setBlocksToFollow(1U);
    }

    // determine if we are encoding a standard or alternate MBT based on the PDU format
    if (standardMBT) {
        uint32_t oldLength = P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow();
        uint32_t bytesToMove = oldLength - 7U; // retain four octets for the packet CRC

        // standard MBT moves the opcode and the two message octets from the header
        // into the data stream. Grow by a block if those three octets would displace
        // non-padding message data
        if (pduUserData[oldLength - 7U] != 0U || pduUserData[oldLength - 6U] != 0U ||
            pduUserData[oldLength - 5U] != 0U) {
            // AABB limits an MBT packet to four blocks total: one header plus
            // at most three data blocks
            if (dataHeader.getBlocksToFollow() >= 3U) {
                LogError(LOG_P25, "AMBT::encode(), standard MBT payload exceeds three data blocks");
                return;
            }

            dataHeader.setBlocksToFollow(dataHeader.getBlocksToFollow() + 1U);
            bytesToMove = oldLength - 4U;
        }

        const uint32_t newLength = P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow();
        ::memmove(pduUserData + 3U, pduUserData, bytesToMove);
        ::memset(pduUserData + 3U + bytesToMove, 0x00U, newLength - 3U - bytesToMove);

        pduUserData[0U] = m_lco & 0x3FU;                                            // LCO
        pduUserData[1U] = dataHeader.getAMBTField8();                               // Message octet 0
        pduUserData[2U] = dataHeader.getAMBTField9();                               // Message octet 1

        dataHeader.setPadLength(0U);
        dataHeader.setHeaderOffset(0U);
    } else {
        dataHeader.setAMBTOpcode(m_lco);                                            // LCO
    }

    // generate packet CRC-32 and set data blocks
    if (dataHeader.getBlocksToFollow() > 1U) {
        edac::CRC::addCRC32(pduUserData, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());
    } else {
        edac::CRC::addCRC32(pduUserData, P25_PDU_UNCONFIRMED_LENGTH_BYTES);
    }
}
