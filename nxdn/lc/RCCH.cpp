/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
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
#include "nxdn/NXDNDefines.h"
#include "nxdn/lc/RCCH.h"
#include "Log.h"
#include "Utils.h"

using namespace nxdn;
using namespace nxdn::lc;

#include <cstdio>
#include <cassert>
#include <cstring>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a copy instance of the RCCH class.
/// </summary>
/// <param name="data"></param>
RCCH::RCCH(const RCCH& data) : RCCH(SiteData())
{
    copy(data);
}

/// <summary>
/// Initializes a new instance of the RCCH class.
/// </summary>
/// <param name="siteData"></param>
/// <param name="entry"></param>
RCCH::RCCH(SiteData siteData, lookups::IdenTable entry) : RCCH(siteData)
{
    m_siteIdenEntry = entry;
}

/// <summary>
/// Initializes a new instance of the RCCH class.
/// </summary>
/// <param name="siteData"></param>
/// <param name="entry"></param>
/// <param name="verbose"></param>
RCCH::RCCH(SiteData siteData, lookups::IdenTable entry, bool verbose) : RCCH(siteData)
{
    m_verbose = verbose;
    m_siteIdenEntry = entry;
}

/// <summary>
/// Finalizes a instance of RCCH class.
/// </summary>
RCCH::~RCCH()
{
    delete[] m_data;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
RCCH& RCCH::operator=(const RCCH& data)
{
    if (&data != this) {
        ::memcpy(m_data, data.m_data, NXDN_RCCH_LC_LENGTH_BYTES);
        decodeLC(m_data);
    }

    return *this;
}

/// <summary>
/// Decode call link control data.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if RCCH was decoded, otherwise false.</returns>
void RCCH::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(data, i);
        WRITE_BIT(m_data, offset, b);
    }

    if (m_verbose) {
        Utils::dump(2U, "Decoded RCCH Data", m_data, NXDN_RCCH_LC_LENGTH_BYTES);
    }

    decodeLC(m_data);
}

/// <summary>
/// Encode call link control data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void RCCH::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    encodeLC(m_data);

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(m_data, offset);
        WRITE_BIT(data, i, b);
    }

    if (m_verbose) {
        Utils::dump(2U, "Encoded RCCH Data", data, length);
    }
}

/// <summary>
///
/// </summary>
void RCCH::reset()
{
    ::memset(m_data, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    m_messageType = MESSAGE_TYPE_IDLE;

    m_srcId = 0U;
    m_dstId = 0U;

    m_locId = 0U;
    m_regOption = 0U;

    m_version = 0U;

    m_causeRsp = NXDN_CAUSE_MM_REG_ACCEPTED;

    m_grpVchNo = 0U;

    m_emergency = false;
    m_encrypted = false;
    m_priority = false;
    m_group = true;
    m_duplex = false;
    m_transmissionMode = TRANSMISSION_MODE_4800;
}

/// <summary>
/// Gets the raw layer 3 data.
/// </summary>
/// <param name="data"></param>
void RCCH::getData(uint8_t* data) const
{
    ::memcpy(data, m_data, NXDN_RCCH_LC_LENGTH_BYTES);
}

/// <summary>
/// Sets the raw layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
void RCCH::setData(const uint8_t* data, uint32_t length)
{
    ::memset(m_data, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);
    ::memcpy(m_data, data, length);

    decodeLC(m_data);
}

/// <summary>
/// Sets the callsign.
/// </summary>
/// <param name="callsign">Callsign.</param>
void RCCH::setCallsign(std::string callsign)
{
    uint32_t idLength = callsign.length();
    if (idLength > 0) {
        ::memset(m_siteCallsign, 0x20U, NXDN_CALLSIGN_LENGTH_BYTES);

        if (idLength > NXDN_CALLSIGN_LENGTH_BYTES)
            idLength = NXDN_CALLSIGN_LENGTH_BYTES;
        for (uint32_t i = 0; i < idLength; i++)
            m_siteCallsign[i] = callsign[i];
    }
}

/// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the RCCH class.
/// </summary>
RCCH::RCCH() : RCCH(SiteData())
{
    /* stub */
}

/// <summary>
/// Initializes a new instance of the RCCH class.
/// </summary>
/// <param name="siteData"></param>
RCCH::RCCH(SiteData siteData) :
    m_verbose(false),
    m_messageType(MESSAGE_TYPE_IDLE),
    m_srcId(0U),
    m_dstId(0U),
    m_locId(0U),
    m_regOption(0U),
    m_version(0U),
    m_causeRsp(NXDN_CAUSE_MM_REG_ACCEPTED),
    m_grpVchNo(0U),
    m_callType(CALL_TYPE_UNSPECIFIED),
    m_emergency(false),
    m_encrypted(false),
    m_priority(false),
    m_group(true),
    m_duplex(false),
    m_transmissionMode(TRANSMISSION_MODE_4800),
    m_siteData(siteData),
    m_siteIdenEntry(),
    m_data(NULL)
{
    m_data = new uint8_t[NXDN_RCCH_LC_LENGTH_BYTES];
    ::memset(m_data, 0x00U, NXDN_RCCH_LC_LENGTH_BYTES);

    m_siteCallsign = new uint8_t[NXDN_CALLSIGN_LENGTH_BYTES];
    ::memset(m_siteCallsign, 0x00U, NXDN_CALLSIGN_LENGTH_BYTES);
    setCallsign(siteData.callsign());
}

/// <summary>
/// Decode link control.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool RCCH::decodeLC(const uint8_t* data)
{
    if (m_verbose) {
        Utils::dump(2U, "Decoded RCCH", data, NXDN_RCCH_LC_LENGTH_BYTES);
    }

    m_messageType = data[0U] & 0x3FU;                                               // Message Type

    // message type opcodes
    switch (m_messageType) {
    case RTCH_MESSAGE_TYPE_VCALL:
    case RCCH_MESSAGE_TYPE_VCALL_CONN:
        m_callType = (data[2U] >> 5) & 0x07U;                                       // Call Type
        m_emergency = (data[1U] & 0x80U) == 0x80U;                                  // Emergency Flag
        m_priority = (data[1U] & 0x20U) == 0x20U;                                   // Priority Flag
        m_duplex = (data[2U] & 0x10U) == 0x10U;                                     // Half/Full Duplex Flag
        m_transmissionMode = (data[2U] & 0x07U);                                    // Transmission Mode
        m_srcId = (uint16_t)((data[3U] << 8) | data[4U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[5U] << 8) | data[6U]) & 0xFFFFU;                 // Target Radio Address
        break;
    case RTCH_MESSAGE_TYPE_DCALL_HDR:
        m_callType = (data[2U] >> 5) & 0x07U;                                       // Call Type
        m_emergency = (data[1U] & 0x80U) == 0x80U;                                  // Emergency Flag
        m_priority = (data[1U] & 0x20U) == 0x20U;                                   // Priority Flag
        m_duplex = (data[2U] & 0x10U) == 0x10U;                                     // Half/Full Duplex Flag
        m_transmissionMode = (data[2U] & 0x07U);                                    // Transmission Mode
        m_srcId = (uint16_t)((data[3U] << 8) | data[4U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[5U] << 8) | data[6U]) & 0xFFFFU;                 // Target Radio Address
        break;
    case MESSAGE_TYPE_IDLE:
        break;
    case RCCH_MESSAGE_TYPE_REG:
        m_regOption = data[1U] >> 3;                                                // Registration Option
        m_locId = (uint16_t)((data[2U] << 8) | data[3U]) & 0xFFFFU;                 // Location ID
        m_srcId = (uint16_t)((data[4U] << 8) | data[5U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[6U] << 8) | data[7U]) & 0xFFFFU;                 // Target Radio Address
        // bryanb: maybe process subscriber type? (byte 8 and 9)
        m_version = data[10U];                                                      // Version
        break;
    case RCCH_MESSAGE_TYPE_REG_C:
        m_regOption = data[1U] >> 3;                                                // Registration Option
        m_locId = (uint16_t)((data[2U] << 8) | data[3U]) & 0xFFFFU;                 // Location ID
        m_srcId = (uint16_t)((data[4U] << 8) | data[5U]) & 0xFFFFU;                 // Source Radio Address
        break;
    case RCCH_MESSAGE_TYPE_GRP_REG:
        m_regOption = data[1U];                                                     // Group Registration Option
        m_srcId = (uint16_t)((data[2U] << 8) | data[3U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[4U] << 8) | data[5U]) & 0xFFFFU;                 // Target Radio Address
        break;
    default:
        LogError(LOG_NXDN, "RCCH::decodeRCCH(), unknown RCCH value, messageType = $%02X", m_messageType);
        return false;
    }

    return true;
}

/// <summary>
/// Encode link control.
/// </summary>
/// <param name="rs"></param>
void RCCH::encodeLC(uint8_t* data)
{
    m_data[0U] = m_messageType & 0x3FU;                                             // Message Type

    // message type opcodes
    switch (m_messageType) {
    case RTCH_MESSAGE_TYPE_VCALL:
    case RCCH_MESSAGE_TYPE_VCALL_CONN:
        m_data[1U] = (m_emergency ? 0x80U : 0x00U) +                                // Emergency Flag
            (m_priority ? 0x20U : 0x00U);                                           // Priority Flag
        m_data[2U] = ((m_callType & 0x07U) << 5) +                                  // Call Type
            (m_duplex ? 0x10U : 0x00U) +                                            // Half/Full Duplex Flag
            (m_transmissionMode & 0x07U);                                           // Transmission Mode

        m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                       // ...

        m_data[7U] = m_causeRsp;                                                    // Cause (VD)
        m_data[9U] = (m_siteData.locId() >> 8) & 0xFFU;                             // Location ID
        m_data[10U] = (m_siteData.locId() >> 0) & 0xFFU;                            // ...
        break;
    case RCCH_MESSAGE_TYPE_VCALL_ASSGN:
    case RCCH_MESSAGE_TYPE_DCALL_ASSGN:
        m_data[1U] = (m_emergency ? 0x80U : 0x00U) +                                // Emergency Flag
            (m_priority ? 0x20U : 0x00U);                                           // Priority Flag
        m_data[2U] = ((m_callType & 0x07U) << 5) +                                  // Call Type
            (m_duplex ? 0x10U : 0x00U) +                                            // Half/Full Duplex Flag
            (m_transmissionMode & 0x07U);                                           // Transmission Mode

        m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                       // ...

        m_data[7U] = (m_grpVchNo >> 10) & 0x03U;                                    // Channel
        m_data[8U] = (m_grpVchNo & 0xFFU);                                          // ...
        
        m_data[10U] = (m_siteData.locId() >> 8) & 0xFFU;                            // Location ID
        m_data[11U] = (m_siteData.locId() >> 0) & 0xFFU;                            // ...
        break;
    case RTCH_MESSAGE_TYPE_DCALL_HDR:
        m_data[1U] = (m_emergency ? 0x80U : 0x00U) +                                // Emergency Flag
            (m_priority ? 0x20U : 0x00U);                                           // Priority Flag
        m_data[2U] = ((m_callType & 0x07U) << 5) +                                  // Call Type
            (m_duplex ? 0x10U : 0x00U) +                                            // Half/Full Duplex Flag
            (m_transmissionMode & 0x07U);                                           // Transmission Mode

        m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                       // ...

        m_data[7U] = m_causeRsp;                                                    // Cause (VD)
        m_data[9U] = (m_siteData.locId() >> 8) & 0xFFU;                             // Location ID
        m_data[10U] = (m_siteData.locId() >> 0) & 0xFFU;                            // ...
        break;
    case MESSAGE_TYPE_IDLE:
        break;
    case MESSAGE_TYPE_DST_ID_INFO:
        m_data[1U] = 0xC0U + NXDN_CALLSIGN_LENGTH_BYTES;                            // Station ID Option - Start / End / Character Count
        m_data[2U] = (m_siteCallsign[0]);                                           // Character 0
        for (uint8_t i = 1; i < NXDN_CALLSIGN_LENGTH_BYTES; i++) {
            m_data[i + 2U] = m_siteCallsign[i];                                     // Character 1 - 7
        }
        break;
    case RCCH_MESSAGE_TYPE_SITE_INFO:
    {
        m_data[1U] = (m_siteData.locId() >> 16) & 0xFFU;                            // Location ID
        m_data[2U] = (m_siteData.locId() >> 8) & 0xFFU;                             // ...
        m_data[3U] = (m_siteData.locId() >> 0) & 0xFFU;                             // ...

        // bryanb: this is currently fixed -- maybe dynamic in the future
        m_data[4U] = (1 << 6) +                                                     // Channel Structure - Number of BCCH (1)
            (1 << 3) +                                                              // ...               - Number of Grouping (1)
            (1 << 0);                                                               // ...               - Number of Paging Frames (1)
        m_data[5U] = (1 << 0);                                                      // ...               - Number of Iteration (1)

        m_data[6U] = m_siteData.serviceClass();                                     // Service Information
        m_data[7U] = (m_siteData.netActive() ? NXDN_SIF2_IP_NETWORK : 0x00U);       // ...

        // bryanb: this is currently fixed -- maybe dynamic in the future
        m_data[8U] = 0U;                                                            // Restriction Information - No access restriction / No cycle restriction
        m_data[9U] = 0x08U;                                                         // ...                     - No group restriction / GMS; Location Registration Restriction
        m_data[10U] = (!m_siteData.netActive() ? 0x01U : 0x00U);                    // ...                     - No group ratio restriction / No delay time extension / ISO

        // bryanb: this is currently fixed -- maybe dynamic in the future
        m_data[11U] = NXDN_CH_ACCESS_BASE_FREQ_SYS_DEFINED;                         // Channel Access Information - Channel Version / Sys Defined Step / Sys Defined Base Freq

        m_data[14U] = 1U;                                                           // Version

        uint32_t channelNo = m_siteData.channelNo();
        m_data[15U] = (channelNo >> 6) & 0x0FU;                                     // 1st Control Channel
        m_data[16U] = (channelNo & 0x3FU) << 2;                                     // ...
    }
    break;
    case MESSAGE_TYPE_SRV_INFO:
        m_data[1U] = (m_siteData.locId() >> 16) & 0xFFU;                            // Location ID
        m_data[2U] = (m_siteData.locId() >> 8) & 0xFFU;                             // ...
        m_data[3U] = (m_siteData.locId() >> 0) & 0xFFU;                             // ...
        m_data[4U] = m_siteData.serviceClass();                                     // Service Information
        m_data[5U] = (m_siteData.netActive() ? NXDN_SIF2_IP_NETWORK : 0x00U);       // ...

        // bryanb: this is currently fixed -- maybe dynamic in the future
        m_data[6U] = 0U;                                                            // Restriction Information - No access restriction / No cycle restriction
        m_data[7U] = 0x08U;                                                         // ...                     - No group restriction / GMS; Location Registration Restriction
        m_data[8U] = (!m_siteData.netActive() ? 0x01U : 0x00U);                     // ...                     - No group ratio restriction / No delay time extension / ISO
        break;
    case RCCH_MESSAGE_TYPE_REG:
        m_data[2U] = (m_siteData.locId() >> 8) & 0xFFU;                             // ...
        m_data[3U] = (m_siteData.locId() >> 0) & 0xFFU;                             // ...
        m_data[4U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[5U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[6U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[7U] = (m_dstId >> 0U) & 0xFFU;                                       // ...
        m_data[8U] = m_causeRsp;                                                    // Cause (MM)
        break;
    case RCCH_MESSAGE_TYPE_REG_C:
        m_data[2U] = (m_siteData.locId() >> 8) & 0xFFU;                             // Location ID
        m_data[3U] = (m_siteData.locId() >> 0) & 0xFFU;                             // ...
        m_data[4U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[5U] = (m_dstId >> 0U) & 0xFFU;                                       // ...
        m_data[6U] = m_causeRsp;                                                    // Cause (MM)
        break;
    case RCCH_MESSAGE_TYPE_REG_COMM:
        m_data[2U] = (m_siteData.locId() >> 8) & 0xFFU;                             // Location ID
        m_data[3U] = (m_siteData.locId() >> 0) & 0xFFU;                             // ...
        m_data[4U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[5U] = (m_dstId >> 0U) & 0xFFU;                                       // ...
        break;
    case RCCH_MESSAGE_TYPE_GRP_REG:
        m_data[2U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[3U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[4U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[5U] = (m_dstId >> 0U) & 0xFFU;                                       // ...
        m_data[6U] = m_causeRsp;                                                    // Cause (MM)
        m_data[8U] = (m_siteData.locId() >> 8) & 0xFFU;                             // Location ID
        m_data[9U] = (m_siteData.locId() >> 0) & 0xFFU;                             // ...
        break;
    default:
        LogError(LOG_NXDN, "RCCH::encodeRCCH(), unknown RCCH value, messageType = $%02X", m_messageType);
        return;
    }

    if (m_verbose) {
        Utils::dump(2U, "Encoded RCCH", m_data, NXDN_RCCH_LC_LENGTH_BYTES);
    }
}

// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void RCCH::copy(const RCCH& data)
{
    m_data = new uint8_t[22U];
    ::memcpy(m_data, data.m_data, 22U);

    m_verbose = data.m_verbose;

    m_siteData = data.m_siteData;
    m_siteIdenEntry = data.m_siteIdenEntry;

    delete[] m_siteCallsign;

    uint8_t* callsign = new uint8_t[NXDN_CALLSIGN_LENGTH_BYTES];
    ::memcpy(callsign, data.m_siteCallsign, NXDN_CALLSIGN_LENGTH_BYTES);

    m_siteCallsign = callsign;

    decodeLC(m_data);
}
