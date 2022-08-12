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
*   Copyright (C) 2018 by Jonathan Naylor G4KLX
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
#include "nxdn/lc/RTCH.h"
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
/// Initializes a new instance of the RTCH class.
/// </summary>
RTCH::RTCH() :
    m_verbose(false),
    m_messageType(MESSAGE_TYPE_IDLE),
    m_callType(CALL_TYPE_UNSPECIFIED),
    m_srcId(0U),
    m_dstId(0U),
    m_emergency(false),
    m_encrypted(false),
    m_priority(false),
    m_group(true),
    m_duplex(false),
    m_transmissionMode(TRANSMISSION_MODE_4800),
    m_packetInfo(),
    m_rsp(),
    m_dataFrameNumber(0U),
    m_dataBlockNumber(0U),
    m_delayCount(0U),
    m_algId(NXDN_CIPHER_TYPE_NONE),
    m_kId(0U),
    m_causeRsp(NXDN_CAUSE_VD_ACCEPTED),
    m_data(NULL)
{
    m_data = new uint8_t[NXDN_RTCH_LC_LENGTH_BYTES];
    ::memset(m_data, 0x00U, NXDN_RTCH_LC_LENGTH_BYTES);

    m_mi = new uint8_t[NXDN_MI_LENGTH_BYTES];
    ::memset(m_mi, 0x00U, NXDN_MI_LENGTH_BYTES);
}

/// <summary>
/// Initializes a copy instance of the RTCH class.
/// </summary>
/// <param name="data"></param>
RTCH::RTCH(const RTCH& data) :
    m_verbose(false),
    m_messageType(MESSAGE_TYPE_IDLE),
    m_callType(CALL_TYPE_UNSPECIFIED),
    m_srcId(0U),
    m_dstId(0U),
    m_emergency(false),
    m_encrypted(false),
    m_priority(false),
    m_group(true),
    m_duplex(false),
    m_transmissionMode(TRANSMISSION_MODE_4800),
    m_packetInfo(),
    m_rsp(),
    m_dataFrameNumber(0U),
    m_dataBlockNumber(0U),
    m_delayCount(0U),
    m_algId(NXDN_CIPHER_TYPE_NONE),
    m_kId(0U),
    m_causeRsp(NXDN_CAUSE_VD_ACCEPTED),
    m_data(NULL)
{
    copy(data);
}

/// <summary>
/// Finalizes a instance of RTCH class.
/// </summary>
RTCH::~RTCH()
{
    delete[] m_data;
    delete[] m_mi;
}

/// <summary>
/// Equals operator.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
RTCH& RTCH::operator=(const RTCH& data)
{
    if (&data != this) {
        ::memcpy(m_data, data.m_data, NXDN_RTCH_LC_LENGTH_BYTES);

        m_verbose = data.m_verbose;
        if (m_data != NULL) {
            if ((m_data[0] & 0x3FU) != 0U) {
                decodeLC(m_data);
            }
        }
    }

    return *this;
}

/// <summary>
/// Decode call link control data.
/// </summary>
/// <param name="data"></param>
/// <returns>True, if RTCH was decoded, otherwise false.</returns>
void RTCH::decode(const uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(data, i);
        WRITE_BIT(m_data, offset, b);
    }

    if (m_verbose) {
        Utils::dump(2U, "Decoded RTCH Data", m_data, NXDN_RTCH_LC_LENGTH_BYTES);
    }

    decodeLC(m_data);
}

/// <summary>
/// Encode call link control data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
/// <param name="offset"></param>
void RTCH::encode(uint8_t* data, uint32_t length, uint32_t offset)
{
    assert(data != NULL);

    encodeLC(m_data);

    for (uint32_t i = 0U; i < length; i++, offset++) {
        bool b = READ_BIT(m_data, offset);
        WRITE_BIT(data, i, b);
    }

    if (m_verbose) {
        Utils::dump(2U, "Encoded RTCH Data", data, length);
    }
}

/// <summary>
///
/// </summary>
void RTCH::reset()
{
    ::memset(m_data, 0x00U, NXDN_RTCH_LC_LENGTH_BYTES);

    m_messageType = MESSAGE_TYPE_IDLE;
    m_callType = CALL_TYPE_UNSPECIFIED;

    m_srcId = 0U;
    m_dstId = 0U;

    m_emergency = false;
    m_encrypted = false;
    m_priority = false;
    m_group = true;
    m_duplex = false;
    m_transmissionMode = TRANSMISSION_MODE_4800;

    m_packetInfo = PacketInformation();
    m_rsp = PacketInformation();
    m_dataFrameNumber = 0U;
    m_dataBlockNumber = 0U;

    m_delayCount = 0U;

    m_algId = NXDN_CIPHER_TYPE_NONE;
    m_kId = 0U;

    m_causeRsp = NXDN_CAUSE_VD_ACCEPTED;
}

/// <summary>
/// Gets the raw layer 3 data.
/// </summary>
/// <param name="data"></param>
void RTCH::getData(uint8_t* data) const
{
    ::memcpy(data, m_data, NXDN_RTCH_LC_LENGTH_BYTES);
}

/// <summary>
/// Sets the raw layer 3 data.
/// </summary>
/// <param name="data"></param>
/// <param name="length"></param>
void RTCH::setData(const uint8_t* data, uint32_t length)
{
    ::memset(m_data, 0x00U, NXDN_RTCH_LC_LENGTH_BYTES);
    ::memcpy(m_data, data, length);

    decodeLC(m_data);
}

/// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------


/// <summary>
/// Decode link control.
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool RTCH::decodeLC(const uint8_t* data)
{
    m_messageType = data[0U] & 0x3FU;                                               // Message Type

    // message type opcodes
    switch (m_messageType) {
    case RTCH_MESSAGE_TYPE_VCALL:
        m_callType = (data[2U] >> 5) & 0x07U;                                       // Call Type
        m_emergency = (data[1U] & 0x80U) == 0x80U;                                  // Emergency Flag
        m_priority = (data[1U] & 0x20U) == 0x20U;                                   // Priority Flag
        m_duplex = (data[2U] & 0x10U) == 0x10U;                                     // Half/Full Duplex Flag
        m_transmissionMode = (data[2U] & 0x07U);                                    // Transmission Mode
        m_srcId = (uint16_t)((data[3U] << 8) | data[4U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[5U] << 8) | data[6U]) & 0xFFFFU;                 // Target Radio Address
        m_algId = (data[7U] >> 6) & 0x03U;                                          // Cipher Type
        m_kId = (data[7U] & 0x3FU);                                                 // Key ID
        break;
    case RTCH_MESSAGE_TYPE_VCALL_IV:
    case RTCH_MESSAGE_TYPE_SDCALL_IV:
        if (m_algId != NXDN_CIPHER_TYPE_NONE && m_kId > 0U) {
            m_mi = new uint8_t[NXDN_MI_LENGTH_BYTES];
            ::memset(m_mi, 0x00U, NXDN_MI_LENGTH_BYTES);
            ::memcpy(m_mi, data + 1U, NXDN_MI_LENGTH_BYTES);                        // Message Indicator
        }
        break;
    case RTCH_MESSAGE_TYPE_TX_REL:
        m_callType = (data[2U] >> 5) & 0x07U;                                       // Call Type
        m_emergency = (data[1U] & 0x80U) == 0x80U;                                  // Emergency Flag
        m_priority = (data[1U] & 0x20U) == 0x20U;                                   // Priority Flag
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
        m_algId = (data[7U] >> 6) & 0x03U;                                          // Cipher Type
        m_kId = (data[7U] & 0x3FU);                                                 // Key ID

        m_packetInfo = PacketInformation();
        m_packetInfo.decode(m_messageType, data + 8U);                              // Packet Information

        if (m_algId != NXDN_CIPHER_TYPE_NONE && m_kId > 0U) {
            ::memset(m_mi, 0x00U, NXDN_MI_LENGTH_BYTES);
            ::memcpy(m_mi, data + 11U, NXDN_MI_LENGTH_BYTES);                       // Message Indicator
        }
        break;
    case RTCH_MESSAGE_TYPE_DCALL_DATA:
    case RTCH_MESSAGE_TYPE_SDCALL_REQ_DATA:
        m_dataFrameNumber = (data[1U] >> 4) & 0x0FU;                                // Frame Number
        m_dataBlockNumber = (data[1U] & 0x0FU);                                     // Block Number
        break;
    case RTCH_MESSAGE_TYPE_DCALL_ACK:
        m_callType = (data[2U] >> 5) & 0x07U;                                       // Call Type
        m_emergency = (data[1U] & 0x80U) == 0x80U;                                  // Emergency Flag
        m_priority = (data[1U] & 0x20U) == 0x20U;                                   // Priority Flag
        m_duplex = (data[2U] & 0x10U) == 0x10U;                                     // Half/Full Duplex Flag
        m_transmissionMode = (data[2U] & 0x07U);                                    // Transmission Mode
        m_srcId = (uint16_t)((data[3U] << 8) | data[4U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[5U] << 8) | data[6U]) & 0xFFFFU;                 // Target Radio Address
    
        m_rsp = PacketInformation();
        m_rsp.decode(m_messageType, data + 7U);                                     // Response
        break;
    case RTCH_MESSAGE_TYPE_HEAD_DLY:
        m_callType = (data[2U] >> 5) & 0x07U;                                       // Call Type
        m_emergency = (data[1U] & 0x80U) == 0x80U;                                  // Emergency Flag
        m_priority = (data[1U] & 0x20U) == 0x20U;                                   // Priority Flag
        m_srcId = (uint16_t)((data[3U] << 8) | data[4U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[5U] << 8) | data[6U]) & 0xFFFFU;                 // Target Radio Address
        m_delayCount = (uint16_t)((data[7U] << 8) | data[8U]) & 0xFFFFU;            // Delay Count
        break;
    case MESSAGE_TYPE_IDLE:
        break;
    case RTCH_MESSAGE_TYPE_SDCALL_REQ_HDR:
        m_callType = (data[2U] >> 5) & 0x07U;                                       // Call Type
        m_emergency = (data[1U] & 0x80U) == 0x80U;                                  // Emergency Flag
        m_priority = (data[1U] & 0x20U) == 0x20U;                                   // Priority Flag
        m_duplex = (data[2U] & 0x10U) == 0x10U;                                     // Half/Full Duplex Flag
        m_transmissionMode = (data[2U] & 0x07U);                                    // Transmission Mode
        m_srcId = (uint16_t)((data[3U] << 8) | data[4U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[5U] << 8) | data[6U]) & 0xFFFFU;                 // Target Radio Address
        m_algId = (data[7U] >> 6) & 0x03U;                                          // Cipher Type
        m_kId = (data[7U] & 0x3FU);                                                 // Key ID

        m_packetInfo = PacketInformation();
        m_packetInfo.decode(m_messageType, data + 8U);                              // Packet Information
        break;
    case RTCH_MESSAGE_TYPE_SDCALL_RESP:
        m_callType = (data[2U] >> 5) & 0x07U;                                       // Call Type
        m_emergency = (data[1U] & 0x80U) == 0x80U;                                  // Emergency Flag
        m_priority = (data[1U] & 0x20U) == 0x20U;                                   // Priority Flag
        m_duplex = (data[2U] & 0x10U) == 0x10U;                                     // Half/Full Duplex Flag
        m_transmissionMode = (data[2U] & 0x07U);                                    // Transmission Mode
        m_srcId = (uint16_t)((data[3U] << 8) | data[4U]) & 0xFFFFU;                 // Source Radio Address
        m_dstId = (uint16_t)((data[5U] << 8) | data[6U]) & 0xFFFFU;                 // Target Radio Address
        m_causeRsp = data[7U];                                                      // Cause (SS)
        break;
    default:
        LogError(LOG_NXDN, "RTCH::decodeRTCH(), unknown RTCH value, messageType = $%02X", m_messageType);
        return false;
    }

    return true;
}

/// <summary>
/// Encode link control.
/// </summary>
/// <param name="rs"></param>
void RTCH::encodeLC(uint8_t* data)
{
    m_messageType = m_data[0U] & 0x3FU;                                             // Message Type

    // message type opcodes
    switch (m_messageType) {
    case RTCH_MESSAGE_TYPE_VCALL:
        m_data[1U] = (m_emergency ? 0x80U : 0x00U) +                                // Emergency Flag
            (m_priority ? 0x20U : 0x00U);                                           // Priority Flag
        m_data[2U] = ((m_callType & 0x07U) << 5) +                                  // Call Type
            (m_duplex ? 0x10U : 0x00U) +                                            // Half/Full Duplex Flag
            (m_transmissionMode & 0x07U);                                           // Transmission Mode

        m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                       // ...

        m_data[7U] = ((m_algId & 0x03U) << 6) +                                     // Cipher Type
            (m_kId & 0x3FU);                                                        // Key ID
        break;
    case RTCH_MESSAGE_TYPE_VCALL_IV:
        if (m_algId != NXDN_CIPHER_TYPE_NONE && m_kId > 0U) {
            ::memcpy(m_data + 1U, m_mi, NXDN_MI_LENGTH_BYTES);                      // Message Indicator
        }
        break;
    case RTCH_MESSAGE_TYPE_TX_REL:
        m_data[1U] = (m_emergency ? 0x80U : 0x00U) +                                // Emergency Flag
            (m_priority ? 0x20U : 0x00U);                                           // Priority Flag
        m_data[2U] = (m_callType & 0x07U) << 5;                                     // Call Type

        m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                       // ...
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

        m_data[7U] = ((m_algId & 0x03U) << 6) +                                     // Cipher Type
            (m_kId & 0x3FU);                                                        // Key ID

        m_packetInfo.encode(m_messageType, data + 8U);                              // Packet Information

        if (m_algId != NXDN_CIPHER_TYPE_NONE && m_kId > 0U) {
            ::memcpy(m_data + 11U, m_mi, NXDN_MI_LENGTH_BYTES);                     // Message Indicator
        }
        break;
    case RTCH_MESSAGE_TYPE_DCALL_DATA:
    case RTCH_MESSAGE_TYPE_SDCALL_REQ_DATA:
        data[1U] = (m_dataFrameNumber & 0x0FU << 4) +                               // Frame Number
            (m_dataBlockNumber & 0x0FU);                                            // Block Number
        break;
    case RTCH_MESSAGE_TYPE_DCALL_ACK:
        m_data[1U] = (m_emergency ? 0x80U : 0x00U) +                                // Emergency Flag
            (m_priority ? 0x20U : 0x00U);                                           // Priority Flag
        m_data[2U] = ((m_callType & 0x07U) << 5) +                                  // Call Type
            (m_duplex ? 0x10U : 0x00U) +                                            // Half/Full Duplex Flag
            (m_transmissionMode & 0x07U);                                           // Transmission Mode

        m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                       // ...

        m_rsp.encode(m_messageType, data + 7U);                                     // Response
        break;
    case RTCH_MESSAGE_TYPE_HEAD_DLY:
        m_data[1U] = (m_emergency ? 0x80U : 0x00U) +                                // Emergency Flag
            (m_priority ? 0x20U : 0x00U);                                           // Priority Flag
        m_data[2U] = (m_callType & 0x07U) << 5;                                     // Call Type

        m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                       // ...

        m_data[7U] = (m_delayCount >> 8U) & 0xFFU;                                  // Delay Count
        m_data[8U] = (m_delayCount >> 0U) & 0xFFU;                                  // ...
        break;
    case MESSAGE_TYPE_IDLE:
        break;
    case RTCH_MESSAGE_TYPE_SDCALL_REQ_HDR:
        m_data[1U] = (m_emergency ? 0x80U : 0x00U) +                                // Emergency Flag
            (m_priority ? 0x20U : 0x00U);                                           // Priority Flag
        m_data[2U] = ((m_callType & 0x07U) << 5) +                                  // Call Type
            (m_duplex ? 0x10U : 0x00U) +                                            // Half/Full Duplex Flag
            (m_transmissionMode & 0x07U);                                           // Transmission Mode

        m_data[3U] = (m_srcId >> 8U) & 0xFFU;                                       // Source Radio Address
        m_data[4U] = (m_srcId >> 0U) & 0xFFU;                                       // ...
        m_data[5U] = (m_dstId >> 8U) & 0xFFU;                                       // Target Radio Address
        m_data[6U] = (m_dstId >> 0U) & 0xFFU;                                       // ...

        m_data[7U] = ((m_algId & 0x03U) << 6) +                                     // Cipher Type
            (m_kId & 0x3FU);                                                        // Key ID

        m_packetInfo.encode(m_messageType, data + 8U);                              // Packet Information
        break;
    default:
        LogError(LOG_NXDN, "RTCH::encodeRTCH(), unknown RTCH value, messageType = $%02X", m_messageType);
        return;
    }
}

// <summary>
/// Internal helper to copy the the class.
/// </summary>
/// <param name="data"></param>
void RTCH::copy(const RTCH& data)
{
    m_data = new uint8_t[NXDN_RTCH_LC_LENGTH_BYTES];
    ::memcpy(m_data, data.m_data, NXDN_RTCH_LC_LENGTH_BYTES);

    m_verbose = data.m_verbose;

    decodeLC(m_data);
}
