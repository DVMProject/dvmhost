// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/P25Defines.h"
#include "p25/lc/tsbk/mbt/MBT_ISP_TELE_INT_DIAL_REQ.h"
#include "edac/CRC.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBT_ISP_TELE_INT_DIAL_REQ class. */

MBT_ISP_TELE_INT_DIAL_REQ::MBT_ISP_TELE_INT_DIAL_REQ() : AMBT(),
    m_digitCount(0U),
    m_duplex(false),
    m_packetMode(false)
{
    m_lco = TSBKO::IOSP_TELE_INT_DIAL;
    ::memset(m_digits, 0x0FU, MAX_DIGITS);
}

/* Decode a standard multi-block trunking signalling block. */

bool MBT_ISP_TELE_INT_DIAL_REQ::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    if (dataHeader.getFormat() != PDUFormatType::UNCONFIRMED || dataHeader.getSAP() != PDUSAP::TRUNK_CTRL || dataHeader.getOutbound()) {
        LogError(LOG_P25, "MBT_ISP_TELE_INT_DIAL_REQ::decodeMBT(), invalid PDU header");
        return false;
    }

    DECLARE_UINT8_ARRAY(pduUserData,
        P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    if (m_lco != TSBKO::IOSP_TELE_INT_DIAL) {
        LogError(LOG_P25, "MBT_ISP_TELE_INT_DIAL_REQ::decodeMBT(), invalid opcode, lco = $%02X", m_lco);
        return false;
    }

    m_digitCount = pduUserData[1U];
    const uint8_t expectedBlocks = (m_digitCount <= 10U) ? 1U : 2U;
    if (m_digitCount == 0U || m_digitCount > MAX_DIGITS ||
        dataHeader.getBlocksToFollow() != expectedBlocks) {
        LogError(LOG_P25, "MBT_ISP_TELE_INT_DIAL_REQ::decodeMBT(), invalid digit/block count, digits = %u, blocks = %u",
            m_digitCount, dataHeader.getBlocksToFollow());
        return false;
    }

    const uint8_t serviceOptions = pduUserData[2U];
    m_emergency = (serviceOptions & 0x80U) == 0x80U;
    m_encrypted = (serviceOptions & 0x40U) == 0x40U;
    m_duplex = (serviceOptions & 0x20U) == 0x20U;
    m_packetMode = (serviceOptions & 0x10U) == 0x10U;
    m_priority = serviceOptions & 0x07U;
    m_srcId = dataHeader.getLLId();

    ::memset(m_digits, 0x0FU, MAX_DIGITS);
    for (uint8_t i = 0U; i < m_digitCount; i++) {
        const uint32_t octet = 3U + (i / 2U);
        m_digits[i] = ((i & 1U) == 0U) ? ((pduUserData[octet] >> 4) & 0x0FU) : (pduUserData[octet] & 0x0FU);
    }

    return true;
}

/* Encode a standard multi-block trunking signalling block. */

void MBT_ISP_TELE_INT_DIAL_REQ::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    if (m_digitCount == 0U || m_digitCount > MAX_DIGITS) {
        LogError(LOG_P25, "MBT_ISP_TELE_INT_DIAL_REQ::encodeMBT(), invalid digit count, digits = %u", m_digitCount);
        return;
    }

    const uint8_t blocksToFollow = (m_digitCount <= 10U) ? 1U : 2U;
    const uint32_t length = P25_PDU_UNCONFIRMED_LENGTH_BYTES * blocksToFollow;
    ::memset(pduUserData, 0x00U, length);

    dataHeader.setFormat(PDUFormatType::UNCONFIRMED);
    dataHeader.setMFId(m_mfId);
    dataHeader.setAckNeeded(false);
    dataHeader.setOutbound(false);
    dataHeader.setSAP(PDUSAP::TRUNK_CTRL);
    dataHeader.setLLId(m_srcId);
    dataHeader.setFullMessage(true);
    dataHeader.setBlocksToFollow(blocksToFollow);
    dataHeader.setPadLength(0U);
    dataHeader.setHeaderOffset(0U);

    pduUserData[0U] = m_lco & 0x3FU;
    pduUserData[1U] = m_digitCount;
    pduUserData[2U] =
        (m_emergency ? 0x80U : 0x00U) |
        (m_encrypted ? 0x40U : 0x00U) |
        (m_duplex ? 0x20U : 0x00U) |
        (m_packetMode ? 0x10U : 0x00U) |
        (m_priority & 0x07U);

    // Unused digit positions use the DTMF null value.
    const uint8_t capacity = (blocksToFollow == 1U) ? 10U : MAX_DIGITS;
    for (uint8_t i = 0U; i < capacity; i++) {
        const uint8_t digit = (i < m_digitCount) ? m_digits[i] : 0x0FU;
        const uint32_t octet = 3U + (i / 2U);
        if ((i & 1U) == 0U)
            pduUserData[octet] = (digit & 0x0FU) << 4;
        else
            pduUserData[octet] |= digit & 0x0FU;
    }

    edac::CRC::addCRC32(pduUserData, length);
}

/* Returns a string that represents the current MBT. */

std::string MBT_ISP_TELE_INT_DIAL_REQ::toString(bool isp)
{
    return std::string("TSBKO, ISP_TELE_INT_DIAL_REQ (Telephone Interconnect Request - Explicit Dialing)");
}

/* Copies the encoded four-bit dial digit values to the supplied buffer. */

uint8_t MBT_ISP_TELE_INT_DIAL_REQ::getDialDigits(uint8_t* digits) const
{
    assert(digits != nullptr);

    ::memcpy(digits, m_digits, m_digitCount);
    return m_digitCount;
}

/* Sets encoded four-bit dial digit values. */

bool MBT_ISP_TELE_INT_DIAL_REQ::setDialDigits(const uint8_t* digits, uint8_t count)
{
    assert(digits != nullptr);

    if (count == 0U || count > MAX_DIGITS)
        return false;

    for (uint8_t i = 0U; i < count; i++) {
        if (digits[i] > 0x0FU)
            return false;
    }

    ::memset(m_digits, 0x0FU, MAX_DIGITS);
    ::memcpy(m_digits, digits, count);
    m_digitCount = count;
    return true;
}
