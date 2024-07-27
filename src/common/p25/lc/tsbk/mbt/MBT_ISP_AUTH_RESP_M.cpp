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
#include "p25/lc/tsbk/mbt/MBT_ISP_AUTH_RESP_M.h"
#include "Log.h"

using namespace p25;
using namespace p25::defines;
using namespace p25::lc;
using namespace p25::lc::tsbk;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBT_ISP_AUTH_RESP_M class. */

MBT_ISP_AUTH_RESP_M::MBT_ISP_AUTH_RESP_M() : AMBT(),
    m_authStandalone(false)
{
    m_lco = TSBKO::ISP_AUTH_RESP_M;

    m_authRes = new uint8_t[AUTH_RES_LENGTH_BYTES];
    ::memset(m_authRes, 0x00U, AUTH_RES_LENGTH_BYTES);
    m_authRC = new uint8_t[AUTH_RAND_CHLNG_LENGTH_BYTES];
    ::memset(m_authRC, 0x00U, AUTH_RAND_CHLNG_LENGTH_BYTES);
}

/* Finalizes a instance of MBT_ISP_AUTH_RESP_M class. */

MBT_ISP_AUTH_RESP_M::~MBT_ISP_AUTH_RESP_M()
{
    if (m_authRes != nullptr) {
        delete[] m_authRes;
        m_authRes = nullptr;
    }

    if (m_authRC != nullptr) {
        delete[] m_authRC;
        m_authRC = nullptr;
    }
}

/* Decode a alternate trunking signalling block. */

bool MBT_ISP_AUTH_RESP_M::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    UInt8Array __pduUserData = std::make_unique<uint8_t[]>(P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());
    uint8_t* pduUserData = __pduUserData.get();
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    ulong64_t tsbkValue = AMBT::toValue(dataHeader, pduUserData);

    if (dataHeader.getBlocksToFollow() != 2) {
        LogError(LOG_P25, "MBT_ISP_AUTH_RESP_M::decodeMBT(), PDU does not contain the appropriate amount of data blocks");
        return false;
    }

    m_netId = (uint32_t)((tsbkValue >> 44) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 32) & 0xFFFU);                               // System ID
    m_srcId = dataHeader.getLLId();                                                 // Source Radio Address

    /** Block 1 */
    m_authRC[0U] = (uint8_t)pduUserData[5U] & 0xFFU;                                // Random Challenge b4
    m_authRC[1U] = (uint8_t)pduUserData[6U] & 0xFFU;                                // Random Challenge b3
    m_authRC[2U] = (uint8_t)pduUserData[7U] & 0xFFU;                                // Random Challenge b2
    m_authRC[3U] = (uint8_t)pduUserData[8U] & 0xFFU;                                // Random Challenge b1
    m_authRC[4U] = (uint8_t)pduUserData[9U] & 0xFFU;                                // Random Challenge b0
    m_authRes[0U] = (uint8_t)pduUserData[10U] & 0xFFU;                              // Result b3
    m_authRes[1U] = (uint8_t)pduUserData[11U] & 0xFFU;                              // Result b2

    /** Block 2 */
    m_authRes[2U] = (uint8_t)pduUserData[12U] & 0xFFU;                              // Result b1
    m_authRes[3U] = (uint8_t)pduUserData[13U] & 0xFFU;                              // Result b0
    m_authStandalone = ((pduUserData[14U] & 0xFFU) & 0x01U) == 0x01U;               // Authentication Standalone Flag

    return true;
}

/* Encode a alternate trunking signalling block. */

void MBT_ISP_AUTH_RESP_M::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    /* stub */

    return;
}

/* Returns a string that represents the current TSBK. */

std::string MBT_ISP_AUTH_RESP_M::toString(bool isp)
{
    return std::string("TSBKO, ISP_AUTH_RESP_M (Authentication Response Mutual)");
}

/* Gets the authentication result. */

void MBT_ISP_AUTH_RESP_M::getAuthRes(uint8_t* res) const
{
    assert(res != nullptr);

    ::memcpy(res, m_authRes, AUTH_RES_LENGTH_BYTES);
}

/* Sets the authentication random challenge. */

void MBT_ISP_AUTH_RESP_M::setAuthRC(const uint8_t* rc)
{
    assert(rc != nullptr);

    ::memcpy(m_authRC, rc, AUTH_RAND_CHLNG_LENGTH_BYTES);
}

/* Gets the authentication random challenge. */

void MBT_ISP_AUTH_RESP_M::getAuthRC(uint8_t* rc) const
{
    assert(rc != nullptr);

    ::memcpy(rc, m_authRC, AUTH_RAND_CHLNG_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/* Internal helper to copy the the class. */

void MBT_ISP_AUTH_RESP_M::copy(const MBT_ISP_AUTH_RESP_M& data)
{
    TSBK::copy(data);

    m_authStandalone = data.m_authStandalone;

    if (m_authRes != nullptr) {
        delete[] m_authRes;
    }

    m_authRes = new uint8_t[AUTH_RES_LENGTH_BYTES];
    ::memcpy(m_authRes, data.m_authRes, AUTH_RES_LENGTH_BYTES);

    if (m_authRC != nullptr) {
        delete[] m_authRC;
    }

    m_authRC = new uint8_t[AUTH_RAND_CHLNG_LENGTH_BYTES];
    ::memcpy(m_authRC, data.m_authRC, AUTH_RAND_CHLNG_LENGTH_BYTES);
}
