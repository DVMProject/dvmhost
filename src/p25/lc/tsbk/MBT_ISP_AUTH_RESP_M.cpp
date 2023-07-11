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
#include "p25/lc/tsbk/MBT_ISP_AUTH_RESP_M.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>
#include <cmath>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MBT_ISP_AUTH_RESP_M class.
/// </summary>
MBT_ISP_AUTH_RESP_M::MBT_ISP_AUTH_RESP_M() : AMBT()
{
    m_lco = TSBK_ISP_AUTH_RESP_M;

    m_authRes = new uint8_t[P25_AUTH_RES_LENGTH_BYTES];
    ::memset(m_authRes, 0x00U, P25_AUTH_RES_LENGTH_BYTES);
    m_authRC = new uint8_t[P25_AUTH_RAND_CHLNG_LENGTH_BYTES];
    ::memset(m_authRC, 0x00U, P25_AUTH_RAND_CHLNG_LENGTH_BYTES);
}

/// <summary>
/// Finalizes a instance of MBT_ISP_AUTH_RESP_M class.
/// </summary>
MBT_ISP_AUTH_RESP_M::~MBT_ISP_AUTH_RESP_M()
{
    if (m_authRes != NULL) {
        delete[] m_authRes;
        m_authRes = NULL;
    }

    if (m_authRC != NULL) {
        delete[] m_authRC;
        m_authRC = NULL;
    }
}

/// <summary>
/// Decode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool MBT_ISP_AUTH_RESP_M::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != NULL);

    uint8_t pduUserData[P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow()];
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    ulong64_t tsbkValue = AMBT::toValue(dataHeader, pduUserData);

    if (dataHeader.getBlocksToFollow() != 2) {
        LogError(LOG_P25, "TSBK::decodeMBT(), PDU does not contain the appropriate amount of data blocks");
        return false;
    }

    m_netId = (uint32_t)((tsbkValue >> 44) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 32) & 0xFFFU);                               // System ID
    m_srcId = dataHeader.getLLId();                                                 // Source Radio Address

    /** Block 1 */
    m_authRC[4U] = (uint8_t)pduUserData[5U] & 0xFFU;                                // Random Challenge b4
    m_authRC[3U] = (uint8_t)pduUserData[6U] & 0xFFU;                                // Random Challenge b3
    m_authRC[2U] = (uint8_t)pduUserData[7U] & 0xFFU;                                // Random Challenge b2
    m_authRC[1U] = (uint8_t)pduUserData[8U] & 0xFFU;                                // Random Challenge b1
    m_authRC[0U] = (uint8_t)pduUserData[9U] & 0xFFU;                                // Random Challenge b0
    m_authRes[3U] = (uint8_t)pduUserData[10U] & 0xFFU;                              // Result b3
    m_authRes[2U] = (uint8_t)pduUserData[11U] & 0xFFU;                              // Result b2

    /** Block 2 */
    m_authRes[1U] = (uint8_t)pduUserData[12U] & 0xFFU;                              // Result b1
    m_authRes[0U] = (uint8_t)pduUserData[13U] & 0xFFU;                              // Result b0
    m_authStandalone = ((pduUserData[14U] & 0xFFU) & 0x01U) == 0x01U;               // Authentication Standalone Flag

    return true;
}

/// <summary>
/// Encode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="pduUserData"></param>
void MBT_ISP_AUTH_RESP_M::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != NULL);

    /* stub */

    return;
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MBT_ISP_AUTH_RESP_M::toString(bool isp)
{
    return std::string("TSBK_ISP_AUTH_RESP_M (Authentication Response Mutual)");
}

/// <summary>Gets the authentication result.</summary>
/// <returns></returns>
void MBT_ISP_AUTH_RESP_M::getAuthRes(uint8_t* res) const
{
    assert(res != NULL);

    ::memcpy(res, m_authRes, P25_AUTH_RES_LENGTH_BYTES);
}

/// <summary>Sets the authentication random challenge.</summary>
/// <param name="rc"></param>
void MBT_ISP_AUTH_RESP_M::setAuthRC(const uint8_t* rc)
{
    assert(rc != NULL);

    ::memcpy(m_authRC, rc, P25_AUTH_RAND_CHLNG_LENGTH_BYTES);
}

/// <summary>Gets the authentication random challenge.</summary>
/// <returns></returns>
void MBT_ISP_AUTH_RESP_M::getAuthRC(uint8_t* rc) const
{
    assert(rc != NULL);

    ::memcpy(rc, m_authRC, P25_AUTH_RAND_CHLNG_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void MBT_ISP_AUTH_RESP_M::copy(const MBT_ISP_AUTH_RESP_M& data)
{
    TSBK::copy(data);

    m_authStandalone = data.m_authStandalone;

    if (m_authRes != NULL) {
        delete[] m_authRes;
    }

    m_authRes = new uint8_t[P25_AUTH_RES_LENGTH_BYTES];
    ::memcpy(m_authRes, data.m_authRes, P25_AUTH_RES_LENGTH_BYTES);

    if (m_authRC != NULL) {
        delete[] m_authRC;
    }

    m_authRC = new uint8_t[P25_AUTH_RAND_CHLNG_LENGTH_BYTES];
    ::memcpy(m_authRC, data.m_authRC, P25_AUTH_RAND_CHLNG_LENGTH_BYTES);
}
