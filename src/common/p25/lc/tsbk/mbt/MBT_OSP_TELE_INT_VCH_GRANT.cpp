// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2026 Charles Bricker, K3NOP
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/mbt/MBT_OSP_TELE_INT_VCH_GRANT.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBT_OSP_TELE_INT_VCH_GRANT class. */

MBT_OSP_TELE_INT_VCH_GRANT::MBT_OSP_TELE_INT_VCH_GRANT() : AMBT(),
    m_forceChannelId(false),
    m_callTimer(0U),
    m_rxGrpVchId(0U),
    m_rxGrpVchNo(0U)
{
    m_lco = TSBKO::IOSP_TELE_INT_DIAL;
}

/* Decode a alternate trunking signalling block. */

bool MBT_OSP_TELE_INT_VCH_GRANT::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    /* stub */

    return true;
}

/* Encode a alternate trunking signalling block. */

void MBT_OSP_TELE_INT_VCH_GRANT::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    dataHeader.setBlocksToFollow(1U);

    uint8_t serviceOptions =
        (m_emergency ? 0x80U : 0x00U) +                                             // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                             // Encrypted Flag
        (m_priority & 0x07U);                                                       // Priority

    dataHeader.setAMBTField8(serviceOptions);
    dataHeader.setAMBTField9(0U);                                                   // Reserved

    uint16_t txFrequency = 0U;
    if ((m_grpVchId != 0U) || m_forceChannelId) {
        txFrequency = (txFrequency << 4) + m_grpVchId;                              // Tx Channel ID
    }
    else {
        txFrequency = (txFrequency << 4) + s_siteData.channelId();                  // Tx Channel ID
    }
    txFrequency = (txFrequency << 12) + m_grpVchNo;                                 // Tx Channel Number

    uint16_t rxFrequency = 0U;
    if ((m_rxGrpVchId != 0U) || m_forceChannelId) {
        rxFrequency = (rxFrequency << 4) + m_rxGrpVchId;                            // Rx Channel ID
    }
    else {
        rxFrequency = (rxFrequency << 4) + s_siteData.channelId();                  // Rx Channel ID
    }
    rxFrequency = (rxFrequency << 12) + m_rxGrpVchNo;                               // Rx Channel Number

    /** Block 1 */
    SET_UINT16(txFrequency, pduUserData, 0U);                                       // Transmit Frequency
    SET_UINT16(rxFrequency, pduUserData, 2U);                                       // Receive Frequency
    SET_UINT16(m_callTimer, pduUserData, 4U);                                       // Call Timer

    // the source/target radio address is carried in the header (LLId)
    AMBT::encode(dataHeader, pduUserData);
}

/* Returns a string that represents the current TSBK. */

std::string MBT_OSP_TELE_INT_VCH_GRANT::toString(bool isp)
{
    return std::string("TSBKO, OSP_TELE_INT_VCH_GRANT (Telephone Interconnect Voice Channel Grant - Explicit)");
}
