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
#if !defined(__NXDN_DEFINES_H__)
#define  __NXDN_DEFINES_H__

#include "Defines.h"

// Message Type Strings
#define NXDN_RTCH_MSG_TYPE_VCALL "VCALL (Voice Call)"
#define NXDN_RTCH_MSG_TYPE_VCALL_REQ "VCALL_REQ (Voice Call Request)"
#define NXDN_RTCH_MSG_TYPE_VCALL_RESP "VCALL_RESP (Voice Call Response)"
#define NXDN_RTCH_MSG_TYPE_VCALL_IV "VCALL_IV (Voice Call Init Vector)"
#define NXDN_RCCH_MSG_TYPE_VCALL_CONN_REQ "VCALL_CONN_REQ (Voice Call Connection Request)"
#define NXDN_RCCH_MSG_TYPE_VCALL_CONN_RESP "VCALL_CONN_RESP (Voice Call Connection Response)"
#define NXDN_RCCH_MSG_TYPE_VCALL_ASSGN "VCALL_ASSGN (Voice Call Assignment)"
#define NXDN_RTCH_MSG_TYPE_TX_REL_EX "TX_REL_EX (Transmission Release Extension)"
#define NXDN_RTCH_MSG_TYPE_TX_REL "TX_REL (Transmission Release)"
#define NXDN_RTCH_MSG_TYPE_DCALL_HDR "DCALL (Data Call Header)"
#define NXDN_RCCH_MSG_TYPE_DCALL_REQ "DCALL_REQ (Data Call Request)"
#define NXDN_RCCH_MSG_TYPE_DCALL_RESP "DCALL_RESP (Data Call Response)"
#define NXDN_RTCH_MSG_TYPE_DCALL_DATA "DCALL (Data Call User Data)"
#define NXDN_RTCH_MSG_TYPE_DCALL_ACK "DCALL_ACL (Data Call Acknowledge)"
#define NXDN_RTCH_MSG_TYPE_HEAD_DLY "HEAD_DLY (Header Delay)"
#define NXDN_MSG_TYPE_IDLE "IDLE (Idle)"
#define NXDN_MSG_TYPE_DISC "DISC (Disconnect)"
#define NXDN_RCCH_MSG_TYPE_DCALL_ASSGN "DCALL_ASSGN (Data Call Assignment)"
#define NXDN_RCCH_MSG_TYPE_REG_REQ "REG_REQ (Registration Request)"
#define NXDN_RCCH_MSG_TYPE_REG_RESP "REG_RESP (Registration Response)"
#define NXDN_RCCH_MSG_TYPE_REG_C_REQ "REG_C_REQ (Registration Clear Request)"
#define NXDN_RCCH_MSG_TYPE_REG_C_RESP "REG_C_RESP (Registration Clear Response)"
#define NXDN_RCCH_MSG_TYPE_REG_COMM "REG_COMM (Registration Command)"
#define NXDN_RCCH_MSG_TYPE_GRP_REG_REQ "GRP_REG_REQ (Group Registration Request)"
#define NXDN_RCCH_MSG_TYPE_GRP_REG_RESP "GRP_REG_RESP (Group Registration Response)"

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

    const uint8_t   NXDN_PREAMBLE[] = { 0x57U, 0x75U, 0xFDU };

    const uint32_t  NXDN_LICH_LENGTH_BITS = 16U;
    const uint32_t  NXDN_LICH_LENGTH_BYTES = NXDN_LICH_LENGTH_BITS / 8U;

    const uint32_t  NXDN_SACCH_FEC_LENGTH_BITS = 60U;                                               // Puncture and Interleave Length
    const uint32_t  NXDN_SACCH_FEC_LENGTH_BYTES = (NXDN_SACCH_FEC_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_SACCH_FEC_CONV_LENGTH_BITS = 72U;                                          // Convolution Length
    const uint32_t  NXDN_SACCH_FEC_CONV_LENGTH_BYTES = NXDN_SACCH_FEC_CONV_LENGTH_BITS / 8U;
    const uint32_t  NXDN_SACCH_CRC_LENGTH_BITS = 36U;                                               // Data + CRC-6 + 4-bit NULL
    const uint32_t  NXDN_SACCH_CRC_LENGTH_BYTES = (NXDN_SACCH_CRC_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_SACCH_LENGTH_BITS = 26U;

    const uint32_t  NXDN_FACCH1_FEC_LENGTH_BITS = 144U;                                             // Puncture and Interleave Length
    const uint32_t  NXDN_FACCH1_FEC_LENGTH_BYTES = NXDN_FACCH1_FEC_LENGTH_BITS / 8U;
    const uint32_t  NXDN_FACCH1_FEC_CONV_LENGTH_BITS = 192U;                                        // Convolution Length
    const uint32_t  NXDN_FACCH1_FEC_CONV_LENGTH_BYTES = NXDN_FACCH1_FEC_CONV_LENGTH_BITS / 8U;
    const uint32_t  NXDN_FACCH1_CRC_LENGTH_BITS = 96U;                                              // Data + CRC-12 + 4-bit NULL
    const uint32_t  NXDN_FACCH1_CRC_LENGTH_BYTES = NXDN_FACCH1_CRC_LENGTH_BITS / 8U;
    const uint32_t  NXDN_FACCH1_LENGTH_BITS = 80U;

    const uint32_t  NXDN_UDCH_FEC_LENGTH_BITS = 348U;                                               // Puncture and Interleave Length
    const uint32_t  NXDN_UDCH_FEC_LENGTH_BYTES = (NXDN_UDCH_FEC_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_UDCH_FEC_CONV_LENGTH_BITS = 406U;                                          // Convolution Length
    const uint32_t  NXDN_UDCH_FEC_CONV_LENGTH_BYTES = (NXDN_UDCH_FEC_CONV_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_UDCH_CRC_LENGTH_BITS = 203U;                                               // Data + CRC-15 + 4-bit NULL
    const uint32_t  NXDN_UDCH_CRC_LENGTH_BYTES = (NXDN_UDCH_CRC_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_UDCH_LENGTH_BITS = 184U;

    const uint32_t  NXDN_CAC_FEC_LENGTH_BITS = 300U;                                                // Puncture and Interleave Length
    const uint32_t  NXDN_CAC_FEC_LENGTH_BYTES = (NXDN_CAC_FEC_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_CAC_FEC_CONV_LENGTH_BITS = 350U;                                           // Convolution Length
    const uint32_t  NXDN_CAC_FEC_CONV_LENGTH_BYTES = (NXDN_CAC_FEC_CONV_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_CAC_CRC_LENGTH_BITS = 175U;                                                // Data + CRC-16 + 4-bit NULL
    const uint32_t  NXDN_CAC_CRC_LENGTH_BYTES = (NXDN_CAC_CRC_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_CAC_LENGTH_BITS = 155U;
    const uint32_t  NXDN_CAC_E_POST_FIELD_BITS = 24U;

    const uint32_t  NXDN_CAC_IN_FEC_LENGTH_BITS = 252U;                                             // Puncture and Interleave Length
    const uint32_t  NXDN_CAC_IN_FEC_LENGTH_BYTES = (NXDN_CAC_IN_FEC_LENGTH_BITS / 8U) + 1U;
    const uint32_t  NXDN_CAC_LONG_FEC_CONV_LENGTH_BITS = 312U;                                      // Convolution Length
    const uint32_t  NXDN_CAC_LONG_FEC_CONV_LENGTH_BYTES = NXDN_CAC_LONG_FEC_CONV_LENGTH_BITS / 8U;
    const uint32_t  NXDN_CAC_LONG_CRC_LENGTH_BITS = 156U;                                           // Data + CRC-16 + 4-bit NULL
    const uint32_t  NXDN_CAC_LONG_LENGTH_BITS = 136U;
    const uint32_t  NXDN_CAC_SHORT_CRC_LENGTH_BITS = 126U;                                          // Data + CRC-16 + 4-bit NULL
    const uint32_t  NXDN_CAC_SHORT_LENGTH_BITS = 106U;

    const uint32_t  NXDN_RTCH_LC_LENGTH_BITS = 176U;
    const uint32_t  NXDN_RTCH_LC_LENGTH_BYTES = (NXDN_RTCH_LC_LENGTH_BITS / 8U);

    const uint32_t  NXDN_RCCH_LC_LENGTH_BITS = 144U;
    const uint32_t  NXDN_RCCH_LC_LENGTH_BYTES = (NXDN_RCCH_LC_LENGTH_BITS / 8U);
    const uint32_t  NXDN_RCCH_CAC_LC_LONG_LENGTH_BITS = 128U;
    const uint32_t  NXDN_RCCH_CAC_LC_SHORT_LENGTH_BITS = 96U;

    const uint32_t  NXDN_FSW_LICH_SACCH_LENGTH_BITS  = NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_LENGTH_BITS;
    const uint32_t  NXDN_FSW_LICH_SACCH_LENGTH_BYTES = NXDN_FSW_LICH_SACCH_LENGTH_BITS / 8U;

    const uint8_t   NXDN_LICH_RFCT_RCCH = 0U;
    const uint8_t   NXDN_LICH_RFCT_RTCH = 1U;
    const uint8_t   NXDN_LICH_RFCT_RDCH = 2U;
    const uint8_t   NXDN_LICH_RFCT_RTCH_C = 3U;

    const uint8_t   NXDN_LICH_CAC_OUTBOUND = 0U;
    const uint8_t   NXDN_LICH_CAC_INBOUND_LONG = 1U;
    const uint8_t   NXDN_LICH_CAC_INBOUND_SHORT = 3U;

    const uint8_t   NXDN_LICH_USC_SACCH_NS = 0U;
    const uint8_t   NXDN_LICH_USC_UDCH = 1U;
    const uint8_t   NXDN_LICH_USC_SACCH_SS = 2U;
    const uint8_t   NXDN_LICH_USC_SACCH_SS_IDLE = 3U;

    const uint8_t   NXDN_LICH_DATA_NORMAL = 0U;
    const uint8_t   NXDN_LICH_DATA_IDLE = 1U;
    const uint8_t   NXDN_LICH_DATA_COMMON = 2U;

    const uint8_t   NXDN_LICH_STEAL_NONE = 3U;
    const uint8_t   NXDN_LICH_STEAL_FACCH1_2 = 2U;
    const uint8_t   NXDN_LICH_STEAL_FACCH1_1 = 1U;
    const uint8_t   NXDN_LICH_STEAL_FACCH = 0U;

    const uint8_t   NXDN_SR_RCCH_SINGLE = 0x00U;
    const uint8_t   NXDN_SR_RCCH_DUAL = 0x01U;
    const uint8_t   NXDN_SR_RCCH_HEAD_SINGLE = 0x02U;
    const uint8_t   NXDN_SR_RCCH_HEAD_DUAL = 0x03U;

    const uint8_t   NXDN_SR_SINGLE = 0U;
    const uint8_t   NXDN_SR_4_4 = 0U;
    const uint8_t   NXDN_SR_3_4 = 1U;
    const uint8_t   NXDN_SR_2_4 = 2U;
    const uint8_t   NXDN_SR_1_4 = 3U;

    const uint8_t   SACCH_IDLE[] = { 0x10U, 0x00U, 0x00U };

    const uint32_t  DEFAULT_SILENCE_THRESHOLD = 14U;
    const uint32_t  MAX_NXDN_VOICE_ERRORS = 144U;
    const uint32_t  MAX_NXDN_VOICE_ERRORS_STEAL = 94U;

    const uint8_t   NXDN_NULL_AMBE[] = { 0xB1U, 0xA8U, 0x22U, 0x25U, 0x6BU, 0xD1U, 0x6CU, 0xCFU, 0x67U };

    const uint8_t   NXDN_CALLSIGN_LENGTH_BYTES = 8U;

    const uint32_t  NXDN_MI_LENGTH_BYTES = 8U;
    const uint32_t  NXDN_PCKT_INFO_LENGTH_BYTES = 3U;

    const uint8_t   NXDN_CIPHER_TYPE_NONE = 0x00U;

    const uint8_t   DATA_RSP_CLASS_ACK = 0x00U;
    const uint8_t   DATA_RSP_CLASS_ACK_S = 0x01U;
    const uint8_t   DATA_RSP_CLASS_NACK = 0x03U;

    const uint8_t   NXDN_LOC_CAT_GLOBAL = 0x00U;
    const uint8_t   NXDN_LOC_CAT_LOCAL = 0x01U;
    const uint8_t   NXDN_LOC_CAT_REGIONAL = 0x02U;

    const uint8_t   NXDN_CAUSE_RSRC_NOT_AVAIL_NETWORK = 0x51U;
    const uint8_t   NXDN_CAUSE_RSRC_NOT_AVAIL_TEMP = 0x52U;
    const uint8_t   NXDN_CAUSE_RSRC_NOT_AVAIL_QUEUED = 0x53U;
    const uint8_t   NXDN_CAUSE_SVC_UNAVAILABLE = 0x06U;
    const uint8_t   NXDN_CAUSE_PROC_ERROR = 0x70U;
    const uint8_t   NXDN_CAUSE_PROC_ERROR_UNDEF = 0x71U;

    const uint8_t   NXDN_CAUSE_MM_REG_ACCEPTED = 0x01U;
    const uint8_t   NXDN_CAUSE_MM_LOC_ACPT_GRP_FAIL = 0x04U;
    const uint8_t   NXDN_CAUSE_MM_LOC_ACPT_GRP_REFUSE = 0x04U;
    const uint8_t   NXDN_CAUSE_MM_REG_FAILED = 0x06U;
    const uint8_t   NXDN_CAUSE_MM_REG_REFUSED = 0x08U;

    const uint8_t   NXDN_CAUSE_VD_ACCEPTED = 0x10U;
    const uint8_t   NXDN_CAUSE_VD_GRP_NOT_PERM = 0x11U;
    const uint8_t   NXDN_CAUSE_VD_REQ_UNIT_NOT_PERM = 0x12U;
    const uint8_t   NXDN_CAUSE_VD_TGT_UNIT_NOT_PERM = 0x13U;
    const uint8_t   NXDN_CAUSE_VD_REQ_UNIT_NOT_REG = 0x1CU;
    const uint8_t   NXDN_CAUSE_VD_QUE_CHN_RESOURCE_NOT_AVAIL = 0x30U;
    const uint8_t   NXDN_CAUSE_VD_QUE_TGT_UNIT_BUSY = 0x38U;
    const uint8_t   NXDN_CAUSE_VD_QUE_GRP_BUSY = 0x39U;

    const uint8_t   NXDN_CAUSE_SS_ACK_R = 0x01U;
    const uint8_t   NXDN_CAUSE_SS_ACK_S = 0x02U;
    const uint8_t   NXDN_CAUSE_SS_NACK = 0x08U;
    const uint8_t   NXDN_CAUSE_SS_ACCEPTED = 0x10U;
    const uint8_t   NXDN_CAUSE_SS_GRP_NOT_PERM = 0x11U;
    const uint8_t   NXDN_CAUSE_SS_REQ_UNIT_NOT_PERM = 0x12U;
    const uint8_t   NXDN_CAUSE_SS_TGT_UNIT_NOT_PERM = 0x13U;
    const uint8_t   NXDN_CAUSE_SS_REQ_UNIT_NOT_REG = 0x1CU;

    const uint8_t   NXDN_CAUSE_DREQ_USER = 0x10U;
    const uint8_t   NXDN_CAUSE_DREQ_OTHER = 0x1FU;

    const uint8_t   NXDN_CAUSE_DISC_USER = 0x10U;
    const uint8_t   NXDN_CAUSE_DISC_OTHER = 0x1FU;

    const uint8_t   NXDN_SIF1_DATA_CALL_SVC = 0x01U;
    const uint8_t   NXDN_SIF1_VOICE_CALL_SVC = 0x02U;
    const uint8_t   NXDN_SIF1_COMPOSITE_CONTROL = 0x04U;
    const uint8_t   NXDN_SIF1_AUTH_SVC = 0x08U;
    const uint8_t   NXDN_SIF1_GRP_REG_SVC = 0x10U;
    const uint8_t   NXDN_SIF1_LOC_REG_SVC = 0x20U;
    const uint8_t   NXDN_SIF1_MULTI_SYSTEM_SVC = 0x40U;
    const uint8_t   NXDN_SIF1_MULTI_SITE_SVC = 0x80U;

    const uint8_t   NXDN_SIF2_IP_NETWORK = 0x10U;
    const uint8_t   NXDN_SIF2_PSTN_NETWORK = 0x20U;
    const uint8_t   NXDN_SIF2_STATUS_CALL_REM_CTRL = 0x40U;
    const uint8_t   NXDN_SIF2_SHORT_DATA_CALL_SVC = 0x80U;

    const uint8_t   NXDN_CH_ACCESS_STEP_SYS_DEFINED = 0x00U;
    const uint8_t   NXDN_CH_ACCESS_STEP_1DOT25K = 0x02U;
    const uint8_t   NXDN_CH_ACCESS_STEP_3DOT125K = 0x03U;

    const uint8_t   NXDN_CH_ACCESS_BASE_FREQ_100 = 0x01U;
    const uint8_t   NXDN_CH_ACCESS_BASE_FREQ_330 = 0x02U;
    const uint8_t   NXDN_CH_ACCESS_BASE_FREQ_400 = 0x03U;
    const uint8_t   NXDN_CH_ACCESS_BASE_FREQ_750 = 0x04U;
    const uint8_t   NXDN_CH_ACCESS_BASE_FREQ_SYS_DEFINED = 0x07U;

    // Common Message Types
    const uint8_t   MESSAGE_TYPE_IDLE = 0x10U;                  // IDLE - Idle
    const uint8_t   MESSAGE_TYPE_DISC = 0x11U;                  // DISC - Disconnect
    const uint8_t   MESSAGE_TYPE_DST_ID_INFO = 0x17U;           // DST_ID_INFO - Digital Station ID
    const uint8_t   MESSAGE_TYPE_SRV_INFO = 0x19U;              // SRV_INFO - Service Information
    const uint8_t   MESSAGE_TYPE_CCH_INFO = 0x1AU;              // CCH_INFO - Control Channel Information
    const uint8_t   MESSAGE_TYPE_ADJ_SITE_INFO = 0x1BU;         // ADJ_SITE_INFO - Adjacent Site Information

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
    const uint8_t   RCCH_MESSAGE_TYPE_VCALL_CONN = 0x03U;       // VCALL_CONN - Voice Call Connection Request (ISP) / Voice Call Connection Response (OSP)
    const uint8_t   RCCH_MESSAGE_TYPE_VCALL_ASSGN = 0x04U;      // VCALL_ASSGN - Voice Call Assignment
    const uint8_t   RCCH_MESSAGE_TYPE_DCALL_ASSGN = 0x14U;      // DCALL_ASSGN - Data Call Assignment
    const uint8_t   RCCH_MESSAGE_TYPE_SITE_INFO = 0x18U;        // SITE_INFO - Site Information
    const uint8_t   RCCH_MESSAGE_TYPE_REG = 0x20U;              // REG - Registration Request (ISP) / Registration Response (OSP)
    const uint8_t   RCCH_MESSAGE_TYPE_REG_C = 0x22U;            // REG_C - Registration Clear Request (ISP) / Registration Clear Response (OSP)
    const uint8_t   RCCH_MESSAGE_TYPE_REG_COMM = 0x23U;         // REG_COMM - Registration Command
    const uint8_t   RCCH_MESSAGE_TYPE_GRP_REG = 0x24U;          // GRP_REG - Group Registration Request (ISP) / Group Registration Response (OSP)
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

// ---------------------------------------------------------------------------
//  Namespace Prototypes
// ---------------------------------------------------------------------------
namespace edac { }
namespace nxdn
{
    namespace edac
    {
        using namespace ::edac;
    } // namespace edac
} // namespace nxdn

#endif // __NXDN_DEFINES_H__
