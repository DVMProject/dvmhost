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
#include "p25/lc/tsbk/mbt/MBT_IOSP_STS_UPDT.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBT_IOSP_STS_UPDT class. */

MBT_IOSP_STS_UPDT::MBT_IOSP_STS_UPDT() : AMBT(),
    m_statusValue(0U)
{
    m_lco = TSBKO::IOSP_STS_UPDT;
}

/* Decode a alternate trunking signalling block. */

bool MBT_IOSP_STS_UPDT::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    UInt8Array __pduUserData = std::make_unique<uint8_t[]>(P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());
    uint8_t* pduUserData = __pduUserData.get();
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    ulong64_t tsbkValue = AMBT::toValue(dataHeader, pduUserData);

    m_statusValue = (uint8_t)((tsbkValue >> 48) & 0xFFFFU);                         // Message Value
    m_netId = (uint32_t)((tsbkValue >> 28) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 16) & 0xFFFU);                               // System ID
    m_dstId = (uint32_t)(((tsbkValue) & 0xFFFFU) << 8) + pduUserData[6U];           // Target Radio Address
    m_srcId = dataHeader.getLLId();                                                 // Source Radio Address

    return true;
}

/* Encode a alternate trunking signalling block. */

void MBT_IOSP_STS_UPDT::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    /* stub */

    return;
}

/* Returns a string that represents the current TSBK. */

std::string MBT_IOSP_STS_UPDT::toString(bool isp)
{
    return (isp) ? std::string("TSBKO, IOSP_STS_UPDT (Status Update Request)") :
        std::string("TSBKO, IOSP_STS_UPDT (Status Update)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void MBT_IOSP_STS_UPDT::copy(const MBT_IOSP_STS_UPDT& data)
{
    TSBK::copy(data);

    m_statusValue = data.m_statusValue;
}
