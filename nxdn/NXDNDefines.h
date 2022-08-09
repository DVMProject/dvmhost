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
*   Copyright (C) 2016,2017,2018 by Jonathan Naylor G4KLX
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
#if !defined(__NXDN_DEFINES_H__)
#define  __NXDN_DEFINES_H__

#include "Defines.h"

// Message Type String(s)
#define NXDN_MESSAGE_TYPE_VCALL "MESSAGE_TYPE_VCALL (NXDN Voice Call)"
#define NXDN_MESSAGE_TYPE_TX_REL "MESSAGE_TYPE_TX_REL (NXDN Transmit Release)"
#define NXDN_MESSAGE_TYPE_DCALL "MESSAGE_TYPE_VCALL (NXDN Data Call)"

namespace nxdn
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t  NXDN_FRAME_LENGTH_BITS = 384U;
    const uint32_t  NXDN_FRAME_LENGTH_BYTES = NXDN_FRAME_LENGTH_BITS / 8U;
    const uint32_t  NXDN_FRAME_LENGTH_SYMBOLS = NXDN_FRAME_LENGTH_BITS / 2U;

    const uint32_t  NXDN_FSW_LENGTH_BITS = 20U;
    const uint32_t  NXDN_FSW_LENGTH_SYMBOLS = NXDN_FSW_LENGTH_BITS / 2U;

    const uint8_t   NXDN_FSW_BYTES[] = { 0xCDU, 0xF5U, 0x90U };
    const uint8_t   NXDN_FSW_BYTES_MASK[] = { 0xFFU, 0xFFU, 0xF0U };
    const uint32_t  NXDN_FSW_BYTES_LENGTH = 3U;

    const uint32_t  NXDN_LICH_LENGTH_BITS = 16U;
    const uint32_t  NXDN_LICH_LENGTH_BYTES = NXDN_LICH_LENGTH_BITS / 8U;

    const uint32_t  NXDN_SACCH_LENGTH_BITS = 60U;
    const uint32_t  NXDN_SACCH_LENGTH_BYTES = NXDN_SACCH_LENGTH_BITS / 8U;
    const uint32_t  NXDN_FACCH1_LENGTH_BITS = 144U;
    const uint32_t  NXDN_FACCH1_LENGTH_BYTES = NXDN_FACCH1_LENGTH_BITS / 8U;
    const uint32_t  NXDN_FACCH2_LENGTH_BITS = 348U;
    const uint32_t  NXDN_FACCH2_LENGTH_BYTES = NXDN_FACCH2_LENGTH_BITS / 8U;

    const uint32_t  NXDN_FSW_LICH_SACCH_LENGTH_BITS  = NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS;
    const uint32_t  NXDN_FSW_LICH_SACCH_LENGTH_BYTES = NXDN_FSW_LICH_SACCH_LENGTH_BITS / 8U;

    const uint8_t   NXDN_LICH_RFCT_RCCH = 0U;
    const uint8_t   NXDN_LICH_RFCT_RTCH = 1U;
    const uint8_t   NXDN_LICH_RFCT_RDCH = 2U;
    const uint8_t   NXDN_LICH_RFCT_RTCH_C = 3U;

    const uint8_t   NXDN_LICH_USC_SACCH_NS = 0U;
    const uint8_t   NXDN_LICH_USC_UDCH = 1U;
    const uint8_t   NXDN_LICH_USC_SACCH_SS = 2U;
    const uint8_t   NXDN_LICH_USC_SACCH_SS_IDLE = 3U;

    const uint8_t   NXDN_LICH_STEAL_NONE = 3U;
    const uint8_t   NXDN_LICH_STEAL_FACCH1_2 = 2U;
    const uint8_t   NXDN_LICH_STEAL_FACCH1_1 = 1U;
    const uint8_t   NXDN_LICH_STEAL_FACCH = 0U;

    const uint8_t   NXDN_LICH_DIRECTION_INBOUND  = 0U;
    const uint8_t   NXDN_LICH_DIRECTION_OUTBOUND = 1U;

    const uint8_t   NXDN_SR_SINGLE = 0U;
    const uint8_t   NXDN_SR_4_4 = 0U;
    const uint8_t   NXDN_SR_3_4 = 1U;
    const uint8_t   NXDN_SR_2_4 = 2U;
    const uint8_t   NXDN_SR_1_4 = 3U;

    const uint8_t   SACCH_IDLE[] = { 0x10U, 0x00U, 0x00U };

    const uint32_t  DEFAULT_SILENCE_THRESHOLD = 14U;
    const uint32_t  MAX_NXDN_VOICE_ERRORS = 144U;
    const uint32_t  MAX_NXDN_VOICE_ERRORS_STEAL = 94U;

    const uint32_t  NXDN_MI_LENGTH_BYTES = 8U;
    const uint32_t  NXDN_PCKT_INFO_LENGTH_BYTES = 3U;

    const uint8_t   NXDN_CIPHER_TYPE_NONE = 0x00U;

    const uint8_t   DATA_RSP_CLASS_ACK = 0x00U;
    const uint8_t   DATA_RSP_CLASS_ACK_S = 0x01U;
    const uint8_t   DATA_RSP_CLASS_NACK = 0x03U;
    
    const uint8_t   NXDN_CAUSE_RESOURCE_NOT_AVAIL = 0x05U;
    const uint8_t   NXDN_CAUSE_SVC_UNAVAILABLE = 0x06U;
    const uint8_t   NXDN_CAUSE_PROC_ERROR = 0x07U;

    const uint8_t   NXDN_CAUSE_MM_NORMAL_1 = 0x01U;
    const uint8_t   NXDN_CAUSE_MM_NORMAL_2 = 0x04U;

    const uint8_t   NXDN_CAUSE_VD_NORMAL_1 = 0x01U;
    const uint8_t   NXDN_CAUSE_VD_NORMAL_2 = 0x02U;
    const uint8_t   NXDN_CAUSE_VD_QUEUED = 0x03U;

    const uint8_t   NXDN_CAUSE_SS_NORMAL = 0x00U;
    const uint8_t   NXDN_CAUSE_SS_NORMAL_1 = 0x01U;
    const uint8_t   NXDN_CAUSE_SS_NORMAL_2 = 0x02U;

    const uint8_t   NXDN_CAUSE_DREQ_NORMAL = 0x01U;

    const uint8_t   NXDN_CAUSE_DISC_NORMAL = 0x01U;
    const uint8_t   NXDN_CAUSE_DISC_NORMAL_TC = 0x02U;

    // Common Message Types
    const uint8_t   MESSAGE_TYPE_IDLE = 0x10U;                  // IDLE - Idle

    // Traffic Channel Message Types
    const uint8_t   RTCH_MESSAGE_TYPE_VCALL = 0x01U;            // VCALL - Voice Call
    const uint8_t   RTCH_MESSAGE_TYPE_VCALL_IV = 0x03U;         // VCALL_IV - Voice Call Initialization Vector
    const uint8_t   RTCH_MESSAGE_TYPE_TX_REL_EX = 0x07U;        // TX_REL_EX - Transmission Release Extension
    const uint8_t   RTCH_MESSAGE_TYPE_TX_REL = 0x08U;           // TX_REL - Transmission Release
    const uint8_t   RTCH_MESSAGE_TYPE_DCALL_HDR = 0x09U;        // DCALL - Data Call (Header)
    const uint8_t   RTCH_MESSAGE_TYPE_DCALL_DATA = 0x0BU;       // DCALL - Data Call (User Data Format)
    const uint8_t   RTCH_MESSAGE_TYPE_DCALL_ACK = 0x0CU;        // DCALL_ACK - Data Call Acknowledge
    const uint8_t   RTCH_MESSAGE_TYPE_HEAD_DLY = 0x0FU;         // HEAD_DLY - Header Delay
    const uint8_t   RTCH_MESSAGE_TYPE_SDCALL_REQ_HDR = 0x38U;   // SDCALL_REQ - Short Data Call Request (Header)
    const uint8_t   RTCH_MESSAGE_TYPE_SDCALL_REQ_DATA = 0x39U;  // SDCALL_REQ - Short Data Call Request (User Data Format)
    const uint8_t   RTCH_MESSAGE_TYPE_SDCALL_IV = 0x3AU;        // SDCALL_IV - Short Data Call Initialization Vector
    const uint8_t   RTCH_MESSAGE_TYPE_SDCALL_RESP = 0x3BU;      // SDCALL_RESP - Short Data Call Response

    // Control Channel Message Types
    const uint8_t   RCCH_MESSAGE_TYPE_PROP_FORM = 0x3FU;        // PROP_FORM - Proprietary Form

    // Call Types
    const uint8_t   CALL_TYPE_BROADCAST = 0x00U;
    const uint8_t   CALL_TYPE_CONFERENCE = 0x01U;
    const uint8_t   CALL_TYPE_UNSPECIFIED = 0x02U;
    const uint8_t   CALL_TYPE_INDIVIDUAL = 0x04U;
    const uint8_t   CALL_TYPE_INTERCONNECT = 0x06U;
    const uint8_t   CALL_TYPE_SPEED_DIAL = 0x07U;

    // Transmission Mode
    const uint8_t   TRANSMISSION_MODE_4800 = 0x00U;
    const uint8_t   TRANSMISSION_MODE_9600 = 0x02U;
    const uint8_t   TRANSMISSION_MODE_9600_EFR = 0x03U; // should never be used on data calls

} // namespace nxdn

#endif // __NXDN_DEFINES_H__
