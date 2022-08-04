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

    const uint32_t  DEFAULT_SILENCE_THRESHOLD = 14U;
    const uint32_t  MAX_NXDN_VOICE_ERRORS = 144U;
    const uint32_t  MAX_NXDN_VOICE_ERRORS_STEAL = 94U;

    // Message Types
    const uint8_t   MESSAGE_TYPE_VCALL = 0x01U;
    const uint8_t   MESSAGE_TYPE_VCALL_IV = 0x03U;
    const uint8_t   MESSAGE_TYPE_DCALL_HDR = 0x09U;
    const uint8_t   MESSAGE_TYPE_DCALL_DATA = 0x0BU;
    const uint8_t   MESSAGE_TYPE_DCALL_ACK = 0x0CU;
    const uint8_t   MESSAGE_TYPE_TX_REL = 0x08U;
    const uint8_t   MESSAGE_TYPE_HEAD_DLY = 0x0FU;
    const uint8_t   MESSAGE_TYPE_SDCALL_REQ_HDR = 0x38U;
    const uint8_t   MESSAGE_TYPE_SDCALL_REQ_DATA = 0x39U;
    const uint8_t   MESSAGE_TYPE_SDCALL_RESP = 0x3BU;
    const uint8_t   MESSAGE_TYPE_SDCALL_IV = 0x3AU;
    const uint8_t   MESSAGE_TYPE_STAT_INQ_REQ = 0x30U;
    const uint8_t   MESSAGE_TYPE_STAT_INQ_RESP = 0x31U;
    const uint8_t   MESSAGE_TYPE_STAT_REQ = 0x32U;
    const uint8_t   MESSAGE_TYPE_STAT_RESP = 0x33U;
    const uint8_t   MESSAGE_TYPE_REM_CON_REQ = 0x34U;
    const uint8_t   MESSAGE_TYPE_REM_CON_RESP = 0x35U;
    const uint8_t   MESSAGE_TYPE_IDLE = 0x10U;
    const uint8_t   MESSAGE_TYPE_AUTH_INQ_REQ = 0x28U;
    const uint8_t   MESSAGE_TYPE_AUTH_INQ_RESP = 0x29U;
    const uint8_t   MESSAGE_TYPE_PROP_FORM = 0x3FU;

    // Voice Call Options
    const uint8_t   VOICE_CALL_OPTION_HALF_DUPLEX = 0x00U;
    const uint8_t   VOICE_CALL_OPTION_DUPLEX = 0x10U;

    // Data Call Options
    const uint8_t   DATA_CALL_OPTION_HALF_DUPLEX = 0x00U;
    const uint8_t   DATA_CALL_OPTION_DUPLEX = 0x10U;
    const uint8_t   DATA_CALL_OPTION_4800 = 0x00U;
    const uint8_t   DATA_CALL_OPTION_9600 = 0x02U;

    const uint8_t   SACCH_IDLE[] = { MESSAGE_TYPE_IDLE, 0x00U, 0x00U };
} // namespace nxdn

#endif // __NXDN_DEFINES_H__
