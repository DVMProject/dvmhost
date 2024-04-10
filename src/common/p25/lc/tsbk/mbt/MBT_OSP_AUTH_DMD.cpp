// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/lc/tsbk/mbt/MBT_OSP_AUTH_DMD.h"
#include "Utils.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MBT_OSP_AUTH_DMD class.
/// </summary>
MBT_OSP_AUTH_DMD::MBT_OSP_AUTH_DMD() : AMBT()
{
    m_lco = TSBK_OSP_AUTH_DMD;

    m_authRS = new uint8_t[P25_AUTH_RAND_SEED_LENGTH_BYTES];
    ::memset(m_authRS, 0x00U, P25_AUTH_RAND_SEED_LENGTH_BYTES);
    m_authRC = new uint8_t[P25_AUTH_RAND_CHLNG_LENGTH_BYTES];
    ::memset(m_authRC, 0x00U, P25_AUTH_RAND_CHLNG_LENGTH_BYTES);
}

/// <summary>
/// Finalizes a instance of MBT_OSP_AUTH_DMD class.
/// </summary>
MBT_OSP_AUTH_DMD::~MBT_OSP_AUTH_DMD()
{
    if (m_authRS != nullptr) {
        delete[] m_authRS;
        m_authRS = nullptr;
    }

    if (m_authRC != nullptr) {
        delete[] m_authRC;
        m_authRC = nullptr;
    }
}

/// <summary>
/// Decode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool MBT_OSP_AUTH_DMD::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    /* stub */

    return true;
}

/// <summary>
/// Encode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="pduUserData"></param>
void MBT_OSP_AUTH_DMD::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    dataHeader.setBlocksToFollow(2U);

    dataHeader.setAMBTField8((m_siteData.netId() >> 12) & 0xFFU);                   // Network ID (b19-12)
    dataHeader.setAMBTField9((m_siteData.netId() >> 4) & 0xFFU);                    // Network ID (b11-b4)

    /** Block 1 */
    pduUserData[0U] = ((m_siteData.netId() & 0x0FU) << 4) +                         // Network ID (b3-b0)
        ((m_siteData.sysId() >> 8) & 0xFFU);                                        // System ID (b11-b8)
    pduUserData[1U] = (m_siteData.sysId() & 0xFFU);                                 // System ID (b7-b0)

    __SET_UINT16(m_dstId, pduUserData, 2U);                                         // Target Radio Address

    pduUserData[5U] = m_authRS[0U];                                                 // Random Salt b9
    pduUserData[6U] = m_authRS[1U];                                                 // Random Salt b8
    pduUserData[7U] = m_authRS[2U];                                                 // Random Salt b7
    pduUserData[8U] = m_authRS[3U];                                                 // Random Salt b6
    pduUserData[9U] = m_authRS[4U];                                                 // Random Salt b5
    pduUserData[10U] = m_authRS[5U];                                                // Random Salt b4
    pduUserData[11U] = m_authRS[6U];                                                // Random Salt b3

    /** Block 2 */
    pduUserData[12U] = m_authRS[7U];                                                // Random Salt b2
    pduUserData[13U] = m_authRS[8U];                                                // Random Salt b1
    pduUserData[14U] = m_authRS[9U];                                                // Random Salt b0
    pduUserData[15U] = m_authRC[0U];                                                // Random Challenge b4
    pduUserData[16U] = m_authRC[1U];                                                // Random Challenge b3
    pduUserData[17U] = m_authRC[2U];                                                // Random Challenge b2
    pduUserData[18U] = m_authRC[3U];                                                // Random Challenge b1
    pduUserData[19U] = m_authRC[4U];                                                // Random Challenge b0

    AMBT::encode(dataHeader, pduUserData);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MBT_OSP_AUTH_DMD::toString(bool isp)
{
    return std::string("TSBK_OSP_AUTH_DMD (Authentication Demand)");
}

/// <summary>Sets the authentication random seed.</summary>
/// <param name="mi"></param>
void MBT_OSP_AUTH_DMD::setAuthRS(const uint8_t* rs)
{
    assert(rs != nullptr);

    ::memcpy(m_authRS, rs, P25_AUTH_RAND_SEED_LENGTH_BYTES);
}

/// <summary>Gets the authentication random seed.</summary>
/// <returns></returns>
void MBT_OSP_AUTH_DMD::getAuthRS(uint8_t* rs) const
{
    assert(rs != nullptr);

    ::memcpy(rs, m_authRS, P25_AUTH_RAND_SEED_LENGTH_BYTES);
}

/// <summary>Sets the authentication random challenge.</summary>
/// <param name="rc"></param>
void MBT_OSP_AUTH_DMD::setAuthRC(const uint8_t* rc)
{
    assert(rc != nullptr);

    ::memcpy(m_authRC, rc, P25_AUTH_RAND_CHLNG_LENGTH_BYTES);
}

/// <summary>Gets the authentication random challenge.</summary>
/// <returns></returns>
void MBT_OSP_AUTH_DMD::getAuthRC(uint8_t* rc) const
{
    assert(rc != nullptr);

    ::memcpy(rc, m_authRC, P25_AUTH_RAND_CHLNG_LENGTH_BYTES);
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void MBT_OSP_AUTH_DMD::copy(const MBT_OSP_AUTH_DMD& data)
{
    TSBK::copy(data);

    if (m_authRS != nullptr) {
        delete[] m_authRS;
    }

    m_authRS = new uint8_t[P25_AUTH_RAND_SEED_LENGTH_BYTES];
    ::memcpy(m_authRS, data.m_authRS, P25_AUTH_RAND_SEED_LENGTH_BYTES);

    if (m_authRC != nullptr) {
        delete[] m_authRC;
    }

    m_authRC = new uint8_t[P25_AUTH_RAND_CHLNG_LENGTH_BYTES];
    ::memcpy(m_authRC, data.m_authRC, P25_AUTH_RAND_CHLNG_LENGTH_BYTES);
}
