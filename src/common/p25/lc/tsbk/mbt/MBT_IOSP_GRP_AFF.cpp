// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
*
*/
#include "Defines.h"
#include "p25/lc/tsbk/mbt/MBT_IOSP_GRP_AFF.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MBT_IOSP_GRP_AFF class.
/// </summary>
MBT_IOSP_GRP_AFF::MBT_IOSP_GRP_AFF() : AMBT(),
    m_announceGroup(P25_WUID_ALL)
{
    m_lco = TSBK_IOSP_GRP_AFF;
}

/// <summary>
/// Decode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool MBT_IOSP_GRP_AFF::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
{
    assert(blocks != nullptr);

    uint8_t pduUserData[P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow()];
    ::memset(pduUserData, 0x00U, P25_PDU_UNCONFIRMED_LENGTH_BYTES * dataHeader.getBlocksToFollow());

    bool ret = AMBT::decode(dataHeader, blocks, pduUserData);
    if (!ret)
        return false;

    ulong64_t tsbkValue = AMBT::toValue(dataHeader, pduUserData);

    m_netId = (uint32_t)((tsbkValue >> 44) & 0xFFFFFU);                             // Network ID
    m_sysId = (uint32_t)((tsbkValue >> 32) & 0xFFFU);                               // System ID
    m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                              // Talkgroup Address
    m_srcId = dataHeader.getLLId();                                                 // Source Radio Address

    return true;
}

/// <summary>
/// Encode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="pduUserData"></param>
void MBT_IOSP_GRP_AFF::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    dataHeader.setBlocksToFollow(2U);

    dataHeader.setAMBTField8((m_siteData.netId() >> 12) & 0xFFU);                   // Network ID (b19-12)
    dataHeader.setAMBTField9((m_siteData.netId() >> 4) & 0xFFU);                    // Network ID (b11-b4)

    /** Block 1 */
    pduUserData[0U] = ((m_siteData.netId() & 0x0FU) << 4) +                         // Network ID (b3-b0)
        ((m_siteData.sysId() >> 8) & 0xFFU);                                        // System ID (b11-b8)
    pduUserData[1U] = (m_siteData.sysId() & 0xFFU);                                 // System ID (b7-b0)

    __SET_UINT16B(m_dstId, pduUserData, 2U);                                        // Group ID
    __SET_UINT16B(m_announceGroup, pduUserData, 4U);                                // Announcement Group
    __SET_UINT16B(m_dstId, pduUserData, 6U);                                        // Talkgroup Address

    /** Block 2 */
    ::memset(pduUserData + 12U, 0x00U, 7U);

    AMBT::encode(dataHeader, pduUserData);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MBT_IOSP_GRP_AFF::toString(bool isp)
{
    if (isp) return std::string("TSBK_IOSP_GRP_AFF (Group Affiliation Request)");
    else return std::string("TSBK_IOSP_GRP_AFF (Group Affiliation Response)");
}
