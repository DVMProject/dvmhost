// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "p25/lc/tsbk/mbt/MBT_ISP_CAN_SRV_REQ.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBT_ISP_CAN_SRV_REQ class. */

MBT_ISP_CAN_SRV_REQ::MBT_ISP_CAN_SRV_REQ() : AMBT()
{
    m_lco = TSBKO::ISP_CAN_SRV_REQ;
}

/* Decode a alternate trunking signalling block. */

bool MBT_ISP_CAN_SRV_REQ::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    UInt8Array __pduUserData = std::make_unique<uint8_t[]>(P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());
    uint8_t* pduUserData = __pduUserData.get();
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    ulong64_t tsbkValue = AMBT::toValue(dataHeader, pduUserData);

    m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                     // Additional Info. Flag
    m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                               // Service Type
    m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                              // Reason
    m_netId = (uint32_t)((tsbkValue >> 20) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 8) & 0xFFFU);                                // System ID
    m_dstId = (uint32_t)((((tsbkValue) & 0xFFU) << 16) +                            // Target Radio Address
        (pduUserData[6U] << 8) + (pduUserData[7U]));
    m_srcId = dataHeader.getLLId();                                                 // Source Radio Address

    return true;
}

/* Encode a alternate trunking signalling block. */

void MBT_ISP_CAN_SRV_REQ::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    /* stub */

    return;
}

/* Returns a string that represents the current TSBK. */

std::string MBT_ISP_CAN_SRV_REQ::toString(bool isp)
{
    return std::string("TSBKO, ISP_CAN_SRV_REQ (Cancel Service Request)");
}
