// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/mbt/MBT_OSP_GRP_VCH_GRANT.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBT_OSP_GRP_VCH_GRANT class. */

MBT_OSP_GRP_VCH_GRANT::MBT_OSP_GRP_VCH_GRANT() : AMBT(),
    m_forceChannelId(false),
    m_rxGrpVchId(0U),
    m_rxGrpVchNo(0U)
{
    m_lco = TSBKO::IOSP_GRP_VCH;
}

/* Decode a alternate trunking signalling block. */

bool MBT_OSP_GRP_VCH_GRANT::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    /* stub */

    return true;
}

/* Encode a alternate trunking signalling block. */

void MBT_OSP_GRP_VCH_GRANT::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    uint8_t serviceOptions =
        (m_emergency ? 0x80U : 0x00U) +                                             // Emergency Flag
        (m_encrypted ? 0x40U : 0x00U) +                                             // Encrypted Flag
        (m_priority & 0x07U);                                                       // Priority

    dataHeader.setAMBTField8(serviceOptions);

    uint16_t txFrequency = 0U;
    if ((m_grpVchId != 0U) || m_forceChannelId) {
        txFrequency = (txFrequency << 4) + m_grpVchId;                              // Tx Channel ID
    }
    else {
        txFrequency = (txFrequency << 4) + m_siteData.channelId();                  // Tx Channel ID
    }
    txFrequency = (txFrequency << 12) + m_grpVchNo;                                 // Tx Channel Number


    uint16_t rxFrequency = 0U;
    if ((m_rxGrpVchId != 0U) || m_forceChannelId) {
        rxFrequency = (rxFrequency << 4) + m_rxGrpVchId;                            // Rx Channel ID
    }
    else {
        rxFrequency = (rxFrequency << 4) + m_siteData.channelId();                  // Rx Channel ID
    }
    rxFrequency = (rxFrequency << 12) + m_rxGrpVchNo;                               // Rx Channel Number

    /** Block 1 */
    SET_UINT16(txFrequency, pduUserData, 2U);                                       // Transmit Frequency
    SET_UINT16(rxFrequency, pduUserData, 4U);                                       // Receive Frequency
    SET_UINT16(m_dstId, pduUserData, 6U);                                           // Talkgroup Address

    AMBT::encode(dataHeader, pduUserData);
}

/* Returns a string that represents the current TSBK. */

std::string MBT_OSP_GRP_VCH_GRANT::toString(bool isp)
{
    return std::string("TSBKO, IOSP_GRP_VCH (Group Voice Channel Grant - Explicit)");
}
