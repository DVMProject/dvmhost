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
#include "p25/lc/tsbk/mbt/MBT_OSP_ADJ_STS_BCAST.h"
#include "Log.h"

using namespace p25::lc::tsbk;
using namespace p25::lc;
using namespace p25;

#include <cassert>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the MBT_OSP_ADJ_STS_BCAST class.
/// </summary>
MBT_OSP_ADJ_STS_BCAST::MBT_OSP_ADJ_STS_BCAST() : AMBT(),
    m_adjCFVA(P25_CFVA_FAILURE),
    m_adjSysId(0U),
    m_adjRfssId(0U),
    m_adjSiteId(0U),
    m_adjChannelId(0U),
    m_adjChannelNo(0U),
    m_adjServiceClass(P25_SVC_CLS_INVALID)
{
    m_lco = TSBK_OSP_ADJ_STS_BCAST;
}

/// <summary>
/// Decode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="blocks"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool MBT_OSP_ADJ_STS_BCAST::decodeMBT(const data::DataHeader& dataHeader, const data::DataBlock* blocks)
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
void MBT_OSP_ADJ_STS_BCAST::encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData)
{
    assert(pduUserData != nullptr);

    if ((m_adjRfssId != 0U) && (m_adjSiteId != 0U) && (m_adjChannelNo != 0U)) {
        if (m_adjSysId == 0U) {
            m_adjSysId = m_siteData.sysId();
        }

        // pack LRA, CFVA and system ID into LLID
        uint32_t llId = m_siteData.lra();                                           // Location Registration Area
        llId = (llId << 8) + m_adjCFVA;                                             // CFVA
        llId = (llId << 4) + m_siteData.siteId();                                   // System ID
        dataHeader.setLLId(llId);

        dataHeader.setAMBTField8(m_adjRfssId);                                      // RF Sub-System ID
        dataHeader.setAMBTField9(m_adjSiteId);                                      // Site ID

        /** Block 1 */
        pduUserData[0U] = ((m_adjChannelId & 0x0FU) << 4) +                         // Transmit Channel ID & Channel Number MSB
            ((m_adjChannelNo >> 8) & 0xFFU);
        pduUserData[1U] = (m_adjChannelNo >> 0) & 0xFFU;                            // Transmit Channel Number LSB
        pduUserData[2U] = ((m_adjChannelId & 0x0FU) << 4) +                         // Receive Channel ID & Channel Number MSB
            ((m_adjChannelNo >> 8) & 0xFFU);
        pduUserData[3U] = (m_adjChannelNo >> 0) & 0xFFU;                            // Receive Channel Number LSB
        pduUserData[4U] = m_adjServiceClass;                                        // System Service Class
        pduUserData[5U] = (m_siteData.netId() >> 12) & 0xFFU;                       // Network ID (b19-12)
        pduUserData[6U] = (m_siteData.netId() >> 4) & 0xFFU;                        // Network ID (b11-b4)
        pduUserData[7U] = (m_siteData.netId() & 0x0FU) << 4;                        // Network ID (b3-b0)
    }
    else {
        LogError(LOG_P25, "MBT_OSP_ADJ_STS_BCAST::encodeMBT(), invalid values for OSP_ADJ_STS_BCAST, adjRfssId = $%02X, adjSiteId = $%02X, adjChannelId = %u, adjChannelNo = $%02X, adjSvcClass = $%02X",
            m_adjRfssId, m_adjSiteId, m_adjChannelId, m_adjChannelNo, m_adjServiceClass);
        return; // blatantly ignore creating this TSBK
    }

    AMBT::encode(dataHeader, pduUserData);
}

/// <summary>
/// Returns a string that represents the current TSBK.
/// </summary>
/// <param name="isp"></param>
/// <returns></returns>
std::string MBT_OSP_ADJ_STS_BCAST::toString(bool isp)
{
    return std::string("TSBK_OSP_ADJ_STS_BCAST (Adjacent Site Status Broadcast - Explicit)");
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void MBT_OSP_ADJ_STS_BCAST::copy(const MBT_OSP_ADJ_STS_BCAST& data)
{
    TSBK::copy(data);

    m_adjCFVA = data.m_adjCFVA;
    m_adjSysId = data.m_adjSysId;
    m_adjRfssId = data.m_adjRfssId;
    m_adjSiteId = data.m_adjSiteId;
    m_adjChannelId = data.m_adjChannelId;
    m_adjChannelNo = data.m_adjChannelNo;
    m_adjServiceClass = data.m_adjServiceClass;
}
