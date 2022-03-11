/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2017-2022 by Bryan Biedenkapp N2PLL
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
#include "p25/P25Defines.h"
#include "p25/lc/TSBK.h"
#include "p25/P25Utils.h"
#include "edac/CRC.h"
#include "Log.h"
#include "Utils.h"

using namespace p25::lc;
using namespace p25;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a copy instance of the TSBK class.
/// </summary>
/// <param name="data"></param>
TSBK::TSBK(const TSBK& data) : TSBK()
{
    copy(data);
}

/// <summary>
/// Initializes a new instance of the TSBK class.
/// </summary>
/// <param name="siteData"></param>
/// <param name="entry"></param>
TSBK::TSBK(SiteData siteData, lookups::IdenTable entry) : TSBK(siteData)
{
    m_siteIdenEntry = entry;
}

/// <summary>
/// Initializes a new instance of the TSBK class.
/// </summary>
/// <param name="siteData"></param>
/// <param name="entry"></param>
/// <param name="verbose"></param>
TSBK::TSBK(SiteData siteData, lookups::IdenTable entry, bool verbose) : TSBK(siteData, entry, verbose, false)
{
    m_verbose = verbose;
}

/// <summary>
/// Initializes a new instance of the TSBK class.
/// </summary>
/// <param name="siteData"></param>
/// <param name="entry"></param>
/// <param name="verbose"></param>
TSBK::TSBK(SiteData siteData, lookups::IdenTable entry, bool verbose, bool warnCRC) : TSBK(siteData)
{
    m_warnCRC = warnCRC;
    m_siteIdenEntry = entry;
}

/// <summary>
/// Initializes a new instance of the TSBK class.
/// </summary>
/// <param name="lc"></param>
TSBK::TSBK(LC* lc) : TSBK(lc->siteData())
{
    m_protect = lc->m_protect;
    m_lco = lc->m_lco;
    m_mfId = lc->m_mfId;

    m_srcId = lc->m_srcId;
    m_dstId = lc->m_dstId;

    m_grpVchNo = lc->m_grpVchNo;

    m_emergency = lc->m_emergency;
    m_encrypted = lc->m_encrypted;
    m_priority = lc->m_priority;

    m_group = lc->m_group;
}

/// <summary>
/// Finalizes a instance of TSBK class.
/// </summary>
TSBK::~TSBK()
{
    delete[] m_siteCallsign;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
TSBK& TSBK::operator=(const TSBK& data)
{
    if (this != &data) {
        copy(data);
    }

    return *this;
}

/// <summary>
/// Decode a alternate trunking signalling block.
/// </summary>
/// <param name="dataHeader"></param>
/// <param name="block"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool TSBK::decodeMBT(const data::DataHeader dataHeader, const uint8_t* block)
{
    m_lco = dataHeader.getAMBTOpcode();                                             // LCO
    m_lastBlock = true;
    m_mfId = dataHeader.getMFId();                                                  // Mfg Id.

    if (dataHeader.getOutbound()) {
        LogWarning(LOG_P25, "TSBK::decodeMBT(), MBT is an outbound MBT?, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
    }

    ulong64_t tsbkValue = 0U;

    if (dataHeader.getFormat() == PDU_FMT_AMBT) {
        // combine bytes into rs value
        tsbkValue = dataHeader.getAMBTField8();
        tsbkValue = (tsbkValue << 8) + dataHeader.getAMBTField9();
        tsbkValue = (tsbkValue << 8) + block[0U];
        tsbkValue = (tsbkValue << 8) + block[1U];
        tsbkValue = (tsbkValue << 8) + block[2U];
        tsbkValue = (tsbkValue << 8) + block[3U];
        tsbkValue = (tsbkValue << 8) + block[4U];
        tsbkValue = (tsbkValue << 8) + block[5U];
    } else {
        // combine bytes into rs value
        tsbkValue = block[0U];
        tsbkValue = (tsbkValue << 8) + block[1U];
        tsbkValue = (tsbkValue << 8) + block[2U];
        tsbkValue = (tsbkValue << 8) + block[3U];
        tsbkValue = (tsbkValue << 8) + block[4U];
        tsbkValue = (tsbkValue << 8) + block[5U];
        tsbkValue = (tsbkValue << 8) + block[6U];
        tsbkValue = (tsbkValue << 8) + block[7U];
    }

    // Motorola P25 vendor opcodes
    if (m_mfId == P25_MFG_MOT) {
        switch (m_lco) {
        case TSBK_IOSP_GRP_VCH:
        case TSBK_IOSP_UU_VCH:
        case TSBK_IOSP_UU_ANS:
        case TSBK_IOSP_TELE_INT_ANS:
        case TSBK_IOSP_STS_UPDT:
        case TSBK_IOSP_STS_Q:
        case TSBK_IOSP_MSG_UPDT:
        case TSBK_IOSP_CALL_ALRT:
        case TSBK_IOSP_ACK_RSP:
        case TSBK_IOSP_GRP_AFF:
        case TSBK_IOSP_U_REG:
        case TSBK_ISP_CAN_SRV_REQ:
        case TSBK_ISP_GRP_AFF_Q_RSP:
        case TSBK_OSP_DENY_RSP:
        case TSBK_OSP_QUE_RSP:
        case TSBK_ISP_U_DEREG_REQ:
        case TSBK_OSP_U_DEREG_ACK:
        case TSBK_ISP_LOC_REG_REQ:
            m_mfId = P25_MFG_STANDARD;
            break;
        default:
            LogError(LOG_P25, "TSBK::decodeMBT(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
            break;
        }

        if (m_mfId == P25_MFG_MOT) {
            return true;
        }
        else {
            m_mfId = dataHeader.getMFId();
        }
    }

    // standard P25 reference opcodes
    switch (m_lco) {
    case TSBK_IOSP_STS_UPDT:
        m_statusValue = (uint8_t)((tsbkValue >> 48) & 0xFFFFU);                     // Message Value
        m_netId = (uint32_t)((tsbkValue >> 28) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 16) & 0xFFFU);                           // System ID
        m_dstId = (uint32_t)(((tsbkValue) & 0xFFFFU) << 8) + block[6U];             // Target Radio Address
        m_srcId = dataHeader.getLLId();                                             // Source Radio Address
        break;
    case TSBK_IOSP_MSG_UPDT:
        m_messageValue = (uint32_t)((tsbkValue >> 48) & 0xFFFFU);                   // Message Value
        m_netId = (uint32_t)((tsbkValue >> 28) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 16) & 0xFFFU);                           // System ID
        m_dstId = (uint32_t)(((tsbkValue) & 0xFFFFU) << 8) + block[6U];             // Target Radio Address
        m_srcId = dataHeader.getLLId();                                             // Source Radio Address
        break;
    case TSBK_IOSP_CALL_ALRT:
        m_netId = (uint32_t)((tsbkValue >> 44) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 32) & 0xFFFU);                           // System ID
        m_dstId = (uint32_t)((tsbkValue >> 8) & 0xFFFFFFU);                         // Target Radio Address
        m_srcId = dataHeader.getLLId();                                             // Source Radio Address
        break;
    case TSBK_IOSP_ACK_RSP:
        m_aivFlag = false;
        m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                           // Service Type
        m_netId = (uint32_t)((tsbkValue >> 36) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 24) & 0xFFFU);                           // System ID
        m_dstId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Target Radio Address
        m_srcId = dataHeader.getLLId();                                             // Source Radio Address
        break;
    case TSBK_IOSP_GRP_AFF:
        m_netId = (uint32_t)((tsbkValue >> 44) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 32) & 0xFFFU);                           // System ID
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                          // Talkgroup Address
        m_srcId = dataHeader.getLLId();                                             // Source Radio Address
        break;
    case TSBK_ISP_CAN_SRV_REQ:
        m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 // Additional Info. Flag
        m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                           // Service Type
        m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                          // Reason
        m_netId = (uint32_t)((tsbkValue >> 20) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 8) & 0xFFFU);                            // System ID
        m_dstId = (uint32_t)((((tsbkValue) & 0xFFU) << 16) +                        // Target Radio Address
            (block[6U] << 8) + (block[7U]));
        m_srcId = dataHeader.getLLId();                                             // Source Radio Address
        break;
    case TSBK_IOSP_EXT_FNCT:
        m_netId = (uint32_t)((tsbkValue >> 44) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 32) & 0xFFFU);                           // System ID
        m_extendedFunction = (uint32_t)(((tsbkValue) & 0xFFFFU) << 8) + block[6U];  // Extended Function
        m_srcId = dataHeader.getLLId();                                             // Source Radio Address
        break;
    default:
        LogError(LOG_P25, "TSBK::decodeMBT(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
        break;
    }

    return true;
}

/// <summary>
/// Decode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <returns>True, if TSBK was decoded, otherwise false.</returns>
bool TSBK::decode(const uint8_t* data, bool rawTSBK)
{
    assert(data != NULL);

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES + 1U];
    if (rawTSBK) {
        ::memcpy(tsbk, data, P25_TSBK_LENGTH_BYTES);
        
        bool ret = edac::CRC::checkCCITT162(tsbk, P25_TSBK_LENGTH_BYTES);
        if (!ret) {
            if (m_warnCRC) {
                LogWarning(LOG_P25, "TSBK::decode(), failed CRC CCITT-162 check");
                ret = true; // ignore CRC error
            }
            else {
                LogError(LOG_P25, "TSBK::decode(), failed CRC CCITT-162 check");
            }
        }
    }
    else {
        // deinterleave
        uint8_t raw[P25_TSBK_FEC_LENGTH_BYTES];
        P25Utils::decode(data, raw, 114U, 318U);

        // decode 1/2 rate Trellis & check CRC-CCITT 16
        try {
            bool ret = m_trellis.decode12(raw, tsbk);
            if (!ret) {
                LogError(LOG_P25, "TSBK::decode(), failed to decode Trellis 1/2 rate coding");
            }

            if (ret) {
                ret = edac::CRC::checkCCITT162(tsbk, P25_TSBK_LENGTH_BYTES);
                if (!ret) {
                    if (m_warnCRC) {
                        LogWarning(LOG_P25, "TSBK::decode(), failed CRC CCITT-162 check");
                        ret = true; // ignore CRC error
                    }
                    else {
                        LogError(LOG_P25, "TSBK::decode(), failed CRC CCITT-162 check");
                    }
                }
            }

            if (!ret)
                return false;
        }
        catch (...) {
            Utils::dump(2U, "P25, decoding excepted with input data", tsbk, P25_TSBK_LENGTH_BYTES);
            return false;
        }
    } 

    if (m_verbose) {
        Utils::dump(2U, "Decoded TSBK", tsbk, P25_TSBK_LENGTH_BYTES);
    }

    m_lco = tsbk[0U] & 0x3F;                                                        // LCO
    m_lastBlock = (tsbk[0U] & 0x80U) == 0x80U;                                      // Last Block Marker
    m_mfId = tsbk[1U];                                                              // Mfg Id.

    ulong64_t tsbkValue = 0U;

    // combine bytes into rs value
    tsbkValue = tsbk[2U];
    tsbkValue = (tsbkValue << 8) + tsbk[3U];
    tsbkValue = (tsbkValue << 8) + tsbk[4U];
    tsbkValue = (tsbkValue << 8) + tsbk[5U];
    tsbkValue = (tsbkValue << 8) + tsbk[6U];
    tsbkValue = (tsbkValue << 8) + tsbk[7U];
    tsbkValue = (tsbkValue << 8) + tsbk[8U];
    tsbkValue = (tsbkValue << 8) + tsbk[9U];

    // Motorola P25 vendor opcodes
    if (m_mfId == P25_MFG_MOT) {
        switch (m_lco) {
        case TSBK_IOSP_GRP_VCH:
        case TSBK_IOSP_UU_VCH:
        case TSBK_IOSP_UU_ANS:
        case TSBK_IOSP_TELE_INT_ANS:
        case TSBK_IOSP_STS_UPDT:
        case TSBK_IOSP_STS_Q:
        case TSBK_IOSP_MSG_UPDT:
        case TSBK_IOSP_CALL_ALRT:
        case TSBK_IOSP_ACK_RSP:
        case TSBK_IOSP_GRP_AFF:
        case TSBK_IOSP_U_REG:
        case TSBK_ISP_CAN_SRV_REQ:
        case TSBK_ISP_GRP_AFF_Q_RSP:
        case TSBK_OSP_DENY_RSP:
        case TSBK_OSP_QUE_RSP:
        case TSBK_ISP_U_DEREG_REQ:
        case TSBK_OSP_U_DEREG_ACK:
        case TSBK_ISP_LOC_REG_REQ:
            m_mfId = P25_MFG_STANDARD;
            break;
        default:
            LogError(LOG_P25, "TSBK::decode(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
            break;
        }

        if (m_mfId == P25_MFG_MOT) {
            return true;
        }
        else {
            m_mfId = tsbk[1U];
        }
    }

    // internal P25 vendor opcodes
    if (m_mfId == P25_MFG_DVM) {
        switch (m_lco) {
        case LC_CALL_TERM:
            m_grpVchId = ((tsbkValue >> 52) & 0x0FU);                               // Channel ID
            m_grpVchNo = ((tsbkValue >> 40) & 0xFFFU);                              // Channel Number
            m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                      // Target Radio Address
            m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                            // Source Radio Address
            break;
        default:
            m_mfId = P25_MFG_STANDARD;
            break;
        }

        if (m_mfId == P25_MFG_DVM) {
            return true;
        }
        else {
            m_mfId = tsbk[1U];
        }
    }

    // standard P25 reference opcodes
    switch (m_lco) {
    case TSBK_IOSP_GRP_VCH:
        m_emergency = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;               // Emergency Flag
        m_encrypted = (((tsbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;               // Encryption Flag
        m_priority = (((tsbkValue >> 56) & 0xFFU) & 0x07U);                         // Priority
        m_grpVchId = ((tsbkValue >> 52) & 0x0FU);                                   // Channel ID
        m_grpVchNo = ((tsbkValue >> 40) & 0xFFFU);                                  // Channel Number
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_UU_VCH:
        m_emergency = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;               // Emergency Flag
        m_encrypted = (((tsbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;               // Encryption Flag
        m_priority = (((tsbkValue >> 56) & 0xFFU) & 0x07U);                         // Priority
        m_grpVchId = ((tsbkValue >> 52) & 0x0FU);                                   // Channel ID
        m_grpVchNo = ((tsbkValue >> 40) & 0xFFFU);                                  // Channel Number
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_UU_ANS:
        m_emergency = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;               // Emergency Flag
        m_encrypted = (((tsbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;               // Encryption Flag
        m_priority = (((tsbkValue >> 56) & 0xFFU) & 0x07U);                         // Priority
        m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                          // Answer Response
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_TELE_INT_ANS:
        m_emergency = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;               // Emergency Flag
        m_encrypted = (((tsbkValue >> 56) & 0xFFU) & 0x40U) == 0x40U;               // Encryption Flag
        m_priority = (((tsbkValue >> 56) & 0xFFU) & 0x07U);                         // Priority
        m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                          // Answer Response
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_ISP_SNDCP_CH_REQ:
        m_dataServiceOptions = (uint8_t)((tsbkValue >> 56) & 0xFFU);                // Data Service Options
        m_dataAccessControl = (uint32_t)((tsbkValue >> 40) & 0xFFFFFFFFU);          // Data Access Control
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_STS_UPDT:
        m_statusValue = (uint8_t)((tsbkValue >> 56) & 0xFFU);                       // Status Value
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_MSG_UPDT:
        m_messageValue = (uint32_t)((tsbkValue >> 48) & 0xFFFFU);                   // Message Value
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_CALL_ALRT:
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_ACK_RSP:
        m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 // Additional Info. Flag
        m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                           // Service Type
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_ISP_EMERG_ALRM_REQ: // TSBK_OSP_DENY_RSP
        /*
        ** these are used by TSBK_OSP_DENY_RSP; best way to check is for m_response > 0
        */
        m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 // Additional Info. Flag
        m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                           // Service Type
        m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                          // Reason

        if (m_response == 0U) {
            m_emergency = true;
        } else {
            m_emergency = false;
        }

        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                          // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_EXT_FNCT:
        m_extendedFunction = (uint32_t)((tsbkValue >> 48) & 0xFFFFU);               // Extended Function
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Argument
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Target Radio Address
        break;
    case TSBK_IOSP_GRP_AFF:
        m_sysId = (uint32_t)((tsbkValue >> 40) & 0xFFFU);                           // System ID
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                          // Talkgroup Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_IOSP_U_REG:
        m_netId = (uint32_t)((tsbkValue >> 36) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 24) & 0xFFFU);                           // System ID
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_ISP_CAN_SRV_REQ:
        m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 // Additional Info. Flag
        m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                           // Service Type
        m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                          // Reason
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_ISP_GRP_AFF_Q_RSP:
        m_patchSuperGroupId = (uint32_t)((tsbkValue >> 40) & 0xFFFFU);              // Announcement Group Address
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                          // Talkgroup Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_OSP_QUE_RSP:
        m_aivFlag = (((tsbkValue >> 56) & 0xFFU) & 0x80U) == 0x80U;                 // Additional Info. Flag
        m_service = (uint8_t)((tsbkValue >> 56) & 0x3FU);                           // Service Type
        m_response = (uint8_t)((tsbkValue >> 48) & 0xFFU);                          // Reason
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFFFU);                        // Target Radio Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_ISP_U_DEREG_REQ:
    case TSBK_OSP_U_DEREG_ACK:
        m_netId = (uint32_t)((tsbkValue >> 36) & 0xFFFFFU);                         // Network ID
        m_sysId = (uint32_t)((tsbkValue >> 24) & 0xFFFU);                           // System ID
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_ISP_LOC_REG_REQ:
        m_lra = (uint8_t)((tsbkValue >> 40) & 0xFFU);                               // LRA
        m_dstId = (uint32_t)((tsbkValue >> 24) & 0xFFFFU);                          // Talkgroup Address
        m_srcId = (uint32_t)(tsbkValue & 0xFFFFFFU);                                // Source Radio Address
        break;
    case TSBK_OSP_ADJ_STS_BCAST:
        m_adjSysId = (uint32_t)((tsbkValue >> 40) & 0xFFFU);                        // Site System ID
        m_adjRfssId = (uint8_t)((tsbkValue >> 32) & 0xFFU);                         // Site RFSS ID
        m_adjSiteId = (uint8_t)((tsbkValue >> 24) & 0xFFU);                         // Site ID
        m_adjChannelId = (uint8_t)((tsbkValue >> 20) & 0xFU);                       // Site Channel ID
        m_adjChannelNo = (uint32_t)((tsbkValue >> 8) & 0xFFFU);                     // Site Channel Number
        m_adjServiceClass = (uint8_t)(tsbkValue & 0xFFU);                           // Site Service Class
        break;
    default:
        LogError(LOG_P25, "TSBK::decode(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
        break;
    }

    return true;
}

/// <summary>
/// Encode a trunking signalling block.
/// </summary>
/// <param name="data"></param>
/// <param name="rawTSBK"></param>
/// <param name="noTrellis"></param>
void TSBK::encode(uint8_t * data, bool rawTSBK, bool noTrellis)
{
    assert(data != NULL);

    const uint32_t services = (m_siteData.netActive()) ? P25_SYS_SRV_NET_ACTIVE : 0U | P25_SYS_SRV_DEFAULT;

    uint8_t tsbk[P25_TSBK_LENGTH_BYTES];
    ::memset(tsbk, 0x00U, P25_TSBK_LENGTH_BYTES);

    ulong64_t tsbkValue = 0U;
    tsbk[0U] = m_lco;                                                               // LCO
    tsbk[0U] |= (m_lastBlock) ? 0x80U : 0x00U;                                      // Last Block Marker

    tsbk[1U] = m_mfId;

    // standard P25 reference opcodes
    switch (m_lco) {
    case TSBK_IOSP_GRP_VCH:
        tsbkValue =
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel ID
        tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                 // Channel Number
        tsbkValue = (tsbkValue << 16) + m_dstId;                                    // Talkgroup Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_IOSP_UU_VCH:
        tsbkValue =
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel ID
        tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                 // Channel Number
        tsbkValue = (tsbkValue << 24) + m_dstId;                                    // Target Radio Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_IOSP_UU_ANS:
        tsbkValue =
            (m_emergency ? 0x80U : 0x00U) +                                         // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U) +                                         // Encrypted Flag
            (m_priority & 0x07U);                                                   // Priority
        tsbkValue = (tsbkValue << 32) + m_dstId;                                    // Target Radio Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_IOSP_STS_UPDT:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 16) + m_statusValue;                              // Status Value
        tsbkValue = (tsbkValue << 24) + m_dstId;                                    // Target Radio Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_IOSP_MSG_UPDT:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 16) + m_messageValue;                             // Message Value
        tsbkValue = (tsbkValue << 24) + m_dstId;                                    // Target Radio Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_IOSP_CALL_ALRT:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 40) + m_dstId;                                    // Target Radio Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_IOSP_ACK_RSP:
        tsbkValue = (m_service & 0x3F);                                             // Service Type
        tsbkValue |= (m_aivFlag) ? 0x80U : 0x00U;                                   // Additional Info. Valid Flag
        tsbkValue |= (m_extendedAddrFlag) ? 0x40U : 0x00U;                          // Extended Addressing Flag
        if (m_aivFlag && m_extendedAddrFlag) {
            tsbkValue = (tsbkValue << 20) + m_siteData.netId();                     // Network ID
            tsbkValue = (tsbkValue << 12) + m_siteData.sysId();                     // System ID
        }
        else {
            tsbkValue = (tsbkValue << 32) + m_dstId;                                // Target Radio Address
        }
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_IOSP_EXT_FNCT:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 16) + m_extendedFunction;                         // Extended Function
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Argument
        tsbkValue = (tsbkValue << 24) + m_dstId;                                    // Target Radio Address
        break;
    case TSBK_IOSP_GRP_AFF:
        tsbkValue = 1U;                                                             // Local/Global Affiliation Flag (0 = Local, 1 = Global)
        tsbkValue = (tsbkValue << 7) + (m_response & 0x3U);                         // Affiliation Response
        tsbkValue = (tsbkValue << 16) + (m_patchSuperGroupId & 0xFFFFU);            // Announcement Group Address
        tsbkValue = (tsbkValue << 16) + (m_dstId & 0xFFFFU);                        // Talkgroup Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_OSP_SNDCP_CH_ANN:
        tsbkValue = 0U;                                                             // 
        tsbkValue = (m_emergency ? 0x80U : 0x00U) +                                 // Emergency Flag
            (m_encrypted ? 0x40U : 0x00U);                                          // Encrypted Flag
        tsbkValue = (tsbkValue << 8) +
            (m_sndcpAutoAccess ? 0x80U : 0x00U) +                                   // Autonomous Access
            (m_sndcpAutoAccess ? 0x40U : 0x00U);                                    // Requested Access
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel (T) ID
        tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                     // Channel (T) Number
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel (R) ID
        tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                     // Channel (R) Number
        tsbkValue = (tsbkValue << 16) + m_sndcpDAC;                                 // Data Access Control
        break;
    case TSBK_IOSP_U_REG:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 2) + (m_response & 0x3U);                         // Unit Registration Response
        tsbkValue = (tsbkValue << 12) + m_siteData.sysId();                         // System ID
        tsbkValue = (tsbkValue << 24) + m_dstId;                                    // Source ID
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_OSP_GRP_VCH_GRANT_UPD:
        tsbkValue = 0U;
        tsbkValue = m_siteData.channelId();                                         // Channel ID
        tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                 // Channel Number
        tsbkValue = (tsbkValue << 16) + m_dstId;                                    // Talkgroup Address
        tsbkValue = (tsbkValue << 32) + 0;
        break;
    case TSBK_OSP_DENY_RSP:
    case TSBK_OSP_QUE_RSP:
    {
        if (m_response == 0U) {
            if (m_lco == TSBK_OSP_QUE_RSP) {
                LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_QUE_RSP, reason = %u", m_response);
            }
            else {
                LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_DENY_RSP, reason = %u", m_response);
            }
            return; // blatently ignore creating this TSBK
        }

        tsbkValue = (m_aivFlag) ? 0x80U : 0x00U;                                    // Additional Info Flag
        tsbkValue = (tsbkValue << 6) + m_service;                                   // Service Type
        tsbkValue = (tsbkValue << 8) + m_response;                                  // Deny/Queue Reason

        if (m_group) {
            // group deny/queue
            tsbkValue = (tsbkValue << 8) + 0U;                                      // Call Options
            tsbkValue = (tsbkValue << 16) + m_dstId;                                // Talkgroup Address
            tsbkValue = (tsbkValue << 24) + m_srcId;                                // Source Radio Address
        }
        else {
            // private/individual deny/queue
            tsbkValue = (tsbkValue << 24) + m_dstId;                                // Target Radio Address
            tsbkValue = (tsbkValue << 24) + m_srcId;                                // Source Radio Address
        }
    }
    break;
    case TSBK_OSP_SCCB_EXP:
        tsbkValue = m_siteData.rfssId();                                            // RF Sub-System ID
        tsbkValue = (tsbkValue << 8) + m_siteData.siteId();                         // Site ID

        tsbkValue = (tsbkValue << 4) + m_sccbChannelId1;                            // Channel (T) ID
        tsbkValue = (tsbkValue << 12) + m_sccbChannelNo;                            // Channel (T) Number
        tsbkValue = (tsbkValue << 12) + m_sccbChannelId1;                           // Channel (R) ID
        tsbkValue = (tsbkValue << 12) + m_sccbChannelNo;                            // Channel (R) Number

        if (m_sccbChannelId1 > 0) {
            tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();               // System Service Class
        }
        else {
            tsbkValue = (tsbkValue << 8) + (P25_SVC_CLS_INVALID);                   // System Service Class
        }
        break;
    case TSBK_OSP_GRP_AFF_Q:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 24) + m_dstId;                                    // Target Radio Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_OSP_LOC_REG_RSP:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 6) + (m_response & 0x3U);                         // Registration Response
        tsbkValue = (tsbkValue << 16) + (m_dstId & 0xFFFFU);                        // Talkgroup Address
        tsbkValue = (tsbkValue << 8) + m_siteData.rfssId();                         // RF Sub-System ID
        tsbkValue = (tsbkValue << 8) + m_siteData.sysId();                          // Site ID
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_OSP_U_REG_CMD:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 24) + m_dstId;                                    // Target Radio Address
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_OSP_U_DEREG_ACK:
        tsbkValue = 0U;
        tsbkValue = (tsbkValue << 8) + m_siteData.netId();                          // Network ID
        tsbkValue = (tsbkValue << 12) + m_siteData.sysId();                         // System ID
        tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
        break;
    case TSBK_OSP_IDEN_UP_VU:
    {
        if ((m_siteIdenEntry.chBandwidthKhz() != 0.0F) && (m_siteIdenEntry.chSpaceKhz() != 0.0F) &&
            (m_siteIdenEntry.txOffsetMhz() != 0.0F) && (m_siteIdenEntry.baseFrequency() != 0U)) {
            uint32_t calcSpace = (uint32_t)(m_siteIdenEntry.chSpaceKhz() / 0.125);
            uint32_t calcTxOffset = (uint32_t)((abs(m_siteIdenEntry.txOffsetMhz()) / m_siteIdenEntry.chSpaceKhz()) * 1000);
            if (m_siteIdenEntry.txOffsetMhz() > 0.0F)
                calcTxOffset |= 0x2000U; // this sets a positive offset ...

            uint32_t calcBaseFreq = (uint32_t)(m_siteIdenEntry.baseFrequency() / 5);
            uint8_t chanBw = (m_siteIdenEntry.chBandwidthKhz() >= 12.5F) ? P25_IDEN_UP_VU_BW_125K : P25_IDEN_UP_VU_BW_625K;

            tsbkValue = m_siteIdenEntry.channelId();                                // Channel ID
            tsbkValue = (tsbkValue << 4) + chanBw;                                  // Channel Bandwidth
            tsbkValue = (tsbkValue << 14) + calcTxOffset;                           // Transmit Offset
            tsbkValue = (tsbkValue << 10) + calcSpace;                              // Channel Spacing
            tsbkValue = (tsbkValue << 32) + calcBaseFreq;                           // Base Frequency
        }
        else {
            LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_IDEN_UP_VU, baseFrequency = %uHz, txOffsetMhz = %uMHz, chBandwidthKhz = %fKHz, chSpaceKhz = %fKHz",
                m_siteIdenEntry.baseFrequency(), m_siteIdenEntry.txOffsetMhz(), m_siteIdenEntry.chBandwidthKhz(),
                m_siteIdenEntry.chSpaceKhz());
            return; // blatently ignore creating this TSBK
        }
    }
    break;
    case TSBK_OSP_SYS_SRV_BCAST:
        tsbkValue = 0U;                                                             //
        tsbkValue = (tsbkValue << 16) + services;                                   // System Services Available
        tsbkValue = (tsbkValue << 24) + services;                                   // System Services Supported
        break;
    case TSBK_OSP_SCCB:
        tsbkValue = m_siteData.rfssId();                                            // RF Sub-System ID
        tsbkValue = (tsbkValue << 8) + m_siteData.siteId();                         // Site ID
        tsbkValue = (tsbkValue << 16) + m_sccbChannelId1;                           // SCCB Channel ID 1
        if (m_sccbChannelId1 > 0) {
            tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();               // System Service Class
        }
        else {
            tsbkValue = (tsbkValue << 8) + (P25_SVC_CLS_INVALID);                   // System Service Class
        }
        tsbkValue = (tsbkValue << 16) + m_sccbChannelId2;                           // SCCB Channel ID 2
        if (m_sccbChannelId2 > 0) {
            tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();               // System Service Class
        }
        else {
            tsbkValue = (tsbkValue << 8) + (P25_SVC_CLS_INVALID);                   // System Service Class
        }
        break;
    case TSBK_OSP_RFSS_STS_BCAST:
        tsbkValue = m_siteData.lra();                                               // Location Registration Area
        tsbkValue = (tsbkValue << 4) +
            (m_siteData.netActive()) ? P25_CFVA_NETWORK : 0U;                       // CFVA
        tsbkValue = (tsbkValue << 12) + m_siteData.sysId();                         // System ID
        tsbkValue = (tsbkValue << 8) + m_siteData.rfssId();                         // RF Sub-System ID
        tsbkValue = (tsbkValue << 8) + m_siteData.siteId();                         // Site ID
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel ID
        tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                     // Channel Number
        tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();                   // System Service Class
        break;
    case TSBK_OSP_NET_STS_BCAST:
        tsbkValue = m_siteData.lra();                                               // Location Registration Area
        tsbkValue = (tsbkValue << 20) + m_siteData.netId();                         // Network ID
        tsbkValue = (tsbkValue << 12) + m_siteData.sysId();                         // System ID
        tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel ID
        tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();                     // Channel Number
        tsbkValue = (tsbkValue << 8) + m_siteData.serviceClass();                   // System Service Class
        break;
    case TSBK_OSP_ADJ_STS_BCAST:
    {
        if ((m_adjRfssId != 0U) && (m_adjSiteId != 0U) && (m_adjChannelNo != 0U)) {
            if (m_adjSysId == 0U) {
                m_adjSysId = m_siteData.sysId();
            }

            tsbkValue = m_siteData.lra();                                           // Location Registration Area
            tsbkValue = (tsbkValue << 8) + m_adjCFVA;                               // CFVA
            tsbkValue = (tsbkValue << 12) + m_adjSysId;                             // System ID
            tsbkValue = (tsbkValue << 8) + m_adjRfssId;                             // RF Sub-System ID
            tsbkValue = (tsbkValue << 8) + m_adjSiteId;                             // Site ID
            tsbkValue = (tsbkValue << 4) + m_adjChannelId;                          // Channel ID
            tsbkValue = (tsbkValue << 12) + m_adjChannelNo;                         // Channel Number
            tsbkValue = (tsbkValue << 8) + m_adjServiceClass;                       // System Service Class
        }
        else {
            LogError(LOG_P25, "TSBK::encode(), invalid values for OSP_ADJ_STS_BCAST, adjRfssId = $%02X, adjSiteId = $%02X, adjChannelId = %u, adjChannelNo = $%02X, adjSvcClass = $%02X",
                m_adjRfssId, m_adjSiteId, m_adjChannelId, m_adjChannelNo, m_adjServiceClass);
            return; // blatently ignore creating this TSBK
        }
    }
    break;
    case TSBK_OSP_IDEN_UP:
    {
        if ((m_siteIdenEntry.chBandwidthKhz() != 0.0F) && (m_siteIdenEntry.chSpaceKhz() != 0.0F) &&
            (m_siteIdenEntry.txOffsetMhz() != 0.0F) && (m_siteIdenEntry.baseFrequency() != 0U)) {
            if (m_siteIdenEntry.baseFrequency() < 762000000U) {
                LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_IDEN_UP, baseFrequency = %uHz",
                    m_siteIdenEntry.baseFrequency());
                return; // blatently ignore creating this TSBK
            }

            uint32_t calcSpace = (uint32_t)(m_siteIdenEntry.chSpaceKhz() / 0.125);
            uint32_t calcTxOffset = (uint32_t)((abs(m_siteIdenEntry.txOffsetMhz()) * 1000000) / 250000);
            if (m_siteIdenEntry.txOffsetMhz() > 0.0F)
                calcTxOffset |= 0x100U; // this sets a positive offset ...

            uint32_t calcBaseFreq = (uint32_t)(m_siteIdenEntry.baseFrequency() / 5);
            uint16_t chanBw = (uint16_t)((m_siteIdenEntry.chBandwidthKhz() * 1000) / 125);

            tsbkValue = m_siteIdenEntry.channelId();                                // Channel ID
            tsbkValue = (tsbkValue << 9) + chanBw;                                  // Channel Bandwidth
            tsbkValue = (tsbkValue << 9) + calcTxOffset;                            // Transmit Offset
            tsbkValue = (tsbkValue << 10) + calcSpace;                              // Channel Spacing
            tsbkValue = (tsbkValue << 32) + calcBaseFreq;                           // Base Frequency
        }
        else {
            LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_IDEN_UP, baseFrequency = %uHz, txOffsetMhz = %uMHz, chBandwidthKhz = %fKHz, chSpaceKhz = %fKHz",
                m_siteIdenEntry.baseFrequency(), m_siteIdenEntry.txOffsetMhz(), m_siteIdenEntry.chBandwidthKhz(),
                m_siteIdenEntry.chSpaceKhz());
            return; // blatently ignore creating this TSBK
        }
    }
    break;
    default:
        if (m_mfId == P25_MFG_STANDARD) {
            LogError(LOG_P25, "TSBK::encode(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
        }
        break;
    }

    if (!m_vendorSkip) {
        // Motorola P25 vendor opcodes
        /**
         * The sequence of data in these opcodes was gleaned from the sdrtrunk project (https://github.com/DSheirer/sdrtrunk)
         */
        if (m_mfId == P25_MFG_MOT) {
            switch (m_lco) {
            case TSBK_OSP_MOT_GRG_ADD:
            {
                if ((m_patchSuperGroupId != 0U)) {
                    tsbkValue = m_patchSuperGroupId;                                // Patch Super Group Address
                    tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;                // Patch Group 1 Address

                    if (m_patchGroup2Id != 0U) {
                        tsbkValue = (tsbkValue << 16) + m_patchGroup2Id;            // Patch Group 2 Address
                    }
                    else {
                        tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;            // Patch Group 1 Address
                    }

                    if (m_patchGroup3Id != 0U) {
                        tsbkValue = (tsbkValue << 16) + m_patchGroup3Id;            // Patch Group 3 Address
                    }
                    else {
                        tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;            // Patch Group 1 Address
                    }
                } // otherwise handled as a TSBK_OSP_MOT_GRG_ADD
            }
            break;
            case TSBK_OSP_MOT_GRG_DEL:
            {
                if ((m_patchSuperGroupId != 0U) && (m_patchGroup1Id != 0U)) {
                    tsbkValue = m_patchSuperGroupId;                                // Patch Super Group Address
                    tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;                // Patch Group 1 Address

                    if (m_patchGroup2Id != 0U) {
                        tsbkValue = (tsbkValue << 16) + m_patchGroup2Id;            // Patch Group 2 Address
                    }
                    else {
                        tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;            // Patch Group 1 Address
                    }

                    if (m_patchGroup3Id != 0U) {
                        tsbkValue = (tsbkValue << 16) + m_patchGroup3Id;            // Patch Group 3 Address
                    }
                    else {
                        tsbkValue = (tsbkValue << 16) + m_patchGroup1Id;            // Patch Group 1 Address
                    }
                }
                else {
                    LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_MOT_GRG_DEL, patchSuperGroupId = $%02X, patchGroup1Id = $%02X",
                        m_patchSuperGroupId, m_patchGroup1Id);
                    return; // blatently ignore creating this TSBK
                }
            }
            break;
            case TSBK_OSP_MOT_GRG_VCH_GRANT:
            {
                if (m_patchSuperGroupId != 0U) {
                    tsbkValue = 0U;                                                 // Priority
                    tsbkValue = (tsbkValue << 4) + m_siteData.channelId();          // Channel ID
                    tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();         // Channel Number
                    tsbkValue = (tsbkValue << 16) + m_patchSuperGroupId;            // Patch Supergroup Address
                    tsbkValue = (tsbkValue << 24) + m_srcId;                        // Source Radio Address
                }
                else {
                    LogError(LOG_P25, "TSBK::encode(), invalid values for TSBK_OSP_MOT_GRG_VCH_GRANT, patchSuperGroupId = $%02X", m_patchSuperGroupId);
                    return; // blatently ignore creating this TSBK
                }
            }
            break;
            case TSBK_OSP_MOT_GRG_VCH_UPD:
                tsbkValue = m_siteData.channelId();                                 // Channel ID
                tsbkValue = (tsbkValue << 4) + m_siteData.channelNo();              // Channel Number
                tsbkValue = (tsbkValue << 12) + m_patchGroup1Id;                    // Patch Group 1
                tsbkValue = (tsbkValue << 16) + m_siteData.channelId();             // Channel ID
                tsbkValue = (tsbkValue << 4) + m_siteData.channelNo();              // Channel Number
                tsbkValue = (tsbkValue << 12) + m_patchGroup2Id;                    // Patch Group 2
                break;
            break;
            case TSBK_OSP_MOT_CC_BSI:
                tsbkValue = (m_siteCallsign[0] - 43U) & 0x3F;                       // Character 0
                for (int i = 1; i < P25_MOT_CALLSIGN_LENGTH_BYTES; i++) {
                    tsbkValue = (tsbkValue << 6) + ((m_siteCallsign[i] - 43U) & 0x3F); // Character 1 - 7
                }
                tsbkValue = (tsbkValue << 4) + m_siteData.channelId();              // Channel ID
                tsbkValue = (tsbkValue << 12) + m_siteData.channelNo();             // Channel Number
                break;
            case TSBK_OSP_MOT_PSH_CCH:
                tsbkValue = 0U;
                break;
            // because of how MFId is handled; we have to skip these opcodes
            case TSBK_IOSP_UU_VCH:
            case TSBK_IOSP_STS_UPDT:
            case TSBK_IOSP_STS_Q:
            case TSBK_IOSP_MSG_UPDT:
            case TSBK_IOSP_CALL_ALRT:
            case TSBK_IOSP_GRP_AFF:
            case TSBK_IOSP_ACK_RSP:
            case TSBK_IOSP_U_REG:
            case TSBK_OSP_DENY_RSP:
            case TSBK_OSP_QUE_RSP:
            case TSBK_OSP_GRP_AFF_Q:
            case TSBK_OSP_U_REG_CMD:
            case TSBK_OSP_U_DEREG_ACK:
                break;
            default:
                LogError(LOG_P25, "TSBK::encode(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
                break;
            }
        }

        // internal P25 vendor opcodes
        if (m_mfId == P25_MFG_DVM) {
            switch (m_lco) {
            case LC_CALL_TERM:
                tsbkValue = (tsbkValue << 4) + m_siteData.channelId();                      // Channel ID
                tsbkValue = (tsbkValue << 12) + m_grpVchNo;                                 // Channel Number
                tsbkValue = (tsbkValue << 16) + m_dstId;                                    // Talkgroup Address
                tsbkValue = (tsbkValue << 24) + m_srcId;                                    // Source Radio Address
                break;
            default:
                LogError(LOG_P25, "TSBK::encode(), unknown TSBK LCO value, mfId = $%02X, lco = $%02X", m_mfId, m_lco);
                break;
            }
        }
    }

    // split rs value into bytes
    tsbk[2U] = (uint8_t)((tsbkValue >> 56) & 0xFFU);
    tsbk[3U] = (uint8_t)((tsbkValue >> 48) & 0xFFU);
    tsbk[4U] = (uint8_t)((tsbkValue >> 40) & 0xFFU);
    tsbk[5U] = (uint8_t)((tsbkValue >> 32) & 0xFFU);
    tsbk[6U] = (uint8_t)((tsbkValue >> 24) & 0xFFU);
    tsbk[7U] = (uint8_t)((tsbkValue >> 16) & 0xFFU);
    tsbk[8U] = (uint8_t)((tsbkValue >> 8) & 0xFFU);
    tsbk[9U] = (uint8_t)((tsbkValue >> 0) & 0xFFU);

    // compute CRC-CCITT 16
    edac::CRC::addCCITT162(tsbk, P25_TSBK_LENGTH_BYTES);

    if (m_verbose) {
        Utils::dump(2U, "Encoded TSBK", tsbk, P25_TSBK_LENGTH_BYTES);
    }

    uint8_t raw[P25_TSBK_FEC_LENGTH_BYTES];
    ::memset(raw, 0x00U, P25_TSBK_FEC_LENGTH_BYTES);

    // encode 1/2 rate Trellis
    m_trellis.encode12(tsbk, raw);

    // are we encoding a raw TSBK?
    if (rawTSBK) {
        if (noTrellis) {
            ::memcpy(data, tsbk, P25_TSBK_LENGTH_BYTES);
        }
        else {
            ::memcpy(data, raw, P25_TSBK_FEC_LENGTH_BYTES);
        }
    }
    else {
        // interleave
        P25Utils::encode(raw, data, 114U, 318U);

#if DEBUG_P25_TSBK
        Utils::dump(2U, "TSBK::encode(), TSBK Interleave", data, P25_TSBK_FEC_LENGTH_BYTES + P25_PREAMBLE_LENGTH_BYTES);
#endif
    }
}

/// <summary>
/// Sets the flag to skip vendor opcode processing.
/// </summary>
/// <param name="skip">Flag indicating to skip vendor opcode processing.</param>
void TSBK::setVendorSkip(bool skip)
{
    m_vendorSkip = skip;
}

/// <summary>
/// Sets the callsign.
/// </summary>
/// <param name="callsign">Callsign.</param>
void TSBK::setCallsign(std::string callsign)
{
    uint32_t idLength = callsign.length();
    if (idLength > 0) {
        ::memset(m_siteCallsign, 0x20U, P25_MOT_CALLSIGN_LENGTH_BYTES);

        if (idLength > P25_MOT_CALLSIGN_LENGTH_BYTES)
            idLength = P25_MOT_CALLSIGN_LENGTH_BYTES;
        for (uint32_t i = 0; i < idLength; i++)
            m_siteCallsign[i] = callsign[i];
    }
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------
/// <summary>
/// Initializes a new instance of the TSBK class.
/// </summary>
/// <remarks>This should never be used.</remarks>
TSBK::TSBK() : TSBK(SiteData())
{
    /* stub */
}

/// <summary>
/// Initializes a new instance of the TSBK class.
/// </summary>
/// <param name="siteData"></param>
TSBK::TSBK(SiteData siteData) :
    m_verbose(false),
#if FORCE_TSBK_CRC_WARN
    m_warnCRC(true),
#else
    m_warnCRC(false),
#endif
    m_protect(false),
    m_lco(LC_GROUP),
    m_mfId(P25_MFG_STANDARD),
    m_srcId(0U),
    m_dstId(0U),
    m_lastBlock(false),
    m_aivFlag(true),
    m_extendedAddrFlag(false),
    m_service(0U),
    m_response(P25_RSP_ACCEPT),
    m_netId(P25_WACN_STD_DEFAULT),
    m_sysId(P25_SID_STD_DEFAULT),
    m_grpVchId(0U),
    m_grpVchNo(0U),
    m_messageValue(0U),
    m_statusValue(0U),
    m_extendedFunction(P25_EXT_FNCT_CHECK),
    m_adjCFVA(P25_CFVA_FAILURE),
    m_adjRfssId(0U),
    m_adjSiteId(0U),
    m_adjChannelId(0U),
    m_adjChannelNo(0U),
    m_adjServiceClass(P25_SVC_CLS_INVALID),
    m_sccbChannelId1(0U),
    m_sccbChannelId2(0U),
    m_sccbChannelNo(0U),
    m_lra(0U),
    m_patchSuperGroupId(0U),
    m_patchGroup1Id(0U),
    m_patchGroup2Id(0U),
    m_patchGroup3Id(0U),
    m_emergency(false),
    m_encrypted(false),
    m_priority(4U),
    m_group(true),
    m_siteData(siteData),
    m_siteIdenEntry(),
    m_rs(),
    m_trellis(),
    m_vendorSkip(false),
    m_sndcpAutoAccess(true),
    m_sndcpReqAccess(false),
    m_sndcpDAC(1U),
    m_siteCallsign(NULL)
{
    m_siteCallsign = new uint8_t[P25_MOT_CALLSIGN_LENGTH_BYTES];
    ::memset(m_siteCallsign, 0x00U, P25_MOT_CALLSIGN_LENGTH_BYTES);
    setCallsign(siteData.callsign());
}

/// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void TSBK::copy(const TSBK& data)
{
    m_verbose = data.m_verbose;
    m_warnCRC = data.m_warnCRC;
    m_protect = data.m_protect;
    m_lco = data.m_lco;
    m_mfId = data.m_mfId;

    m_srcId = data.m_srcId;
    m_dstId = data.m_dstId;

    m_lastBlock = data.m_lastBlock;
    m_aivFlag = data.m_aivFlag;
    m_extendedAddrFlag = data.m_extendedAddrFlag;

    m_service = data.m_service;
    m_response = data.m_response;

    m_netId = data.m_netId;
    m_sysId = data.m_sysId;

    m_grpVchNo = data.m_grpVchNo;

    m_messageValue = data.m_messageValue;
    m_statusValue = data.m_statusValue;

    m_extendedFunction = data.m_extendedFunction;

    m_adjCFVA = data.m_adjCFVA;
    m_adjRfssId = data.m_adjRfssId;
    m_adjSiteId = data.m_adjSiteId;
    m_adjChannelId = data.m_adjChannelId;
    m_adjChannelNo = data.m_adjChannelNo;
    m_adjServiceClass = data.m_adjServiceClass;

    m_sccbChannelId1 = data.m_sccbChannelId1;
    m_sccbChannelId2 = data.m_sccbChannelId2;
    m_sccbChannelNo = data.m_sccbChannelNo;

    m_lra = data.m_lra;

    m_patchSuperGroupId = data.m_patchSuperGroupId;
    m_patchGroup1Id = data.m_patchGroup1Id;
    m_patchGroup2Id = data.m_patchGroup2Id;
    m_patchGroup3Id = data.m_patchGroup3Id;

    m_emergency = data.m_emergency;
    m_encrypted = data.m_encrypted;
    m_priority = data.m_priority;

    m_group = data.m_group;

    m_siteData = data.m_siteData;
    m_siteIdenEntry = data.m_siteIdenEntry;

    delete[] m_siteCallsign;

    uint8_t* callsign = new uint8_t[P25_MOT_CALLSIGN_LENGTH_BYTES];
    ::memcpy(callsign, data.m_siteCallsign, P25_MOT_CALLSIGN_LENGTH_BYTES);

    m_siteCallsign = callsign;
}
