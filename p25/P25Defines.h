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
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
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
#if !defined(__P25_DEFINES_H__)
#define  __P25_DEFINES_H__

#include "Defines.h"

// Data Unit ID String(s)
#define P25_HDU_STR "P25_DUID_HDU (Header Data Unit)"
#define P25_TDU_STR "P25_DUID_TDU (Simple Terminator Data Unit)"
#define P25_LDU1_STR "P25_DUID_LDU1 (Logical Link Data Unit 1)"
#define P25_TSDU_STR "P25_DUID_TSDU (Trunking System Data Unit)"
#define P25_LDU2_STR "P25_DUID_LDU2 (Logical Link Data Unit 2)"
#define P25_PDU_STR "P25_DUID_PDU (Packet Data Unit)"
#define P25_TDULC_STR "P25_DUID_TDULC (Terminator Data Unit with Link Control)"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Constants
    // ---------------------------------------------------------------------------

    const uint32_t  P25_HDU_FRAME_LENGTH_BYTES = 99U;
    const uint32_t  P25_HDU_FRAME_LENGTH_BITS = P25_HDU_FRAME_LENGTH_BYTES * 8U;

    const uint32_t  P25_TDU_FRAME_LENGTH_BYTES = 18U;
    const uint32_t  P25_TDU_FRAME_LENGTH_BITS = P25_TDU_FRAME_LENGTH_BYTES * 8U;

    const uint32_t  P25_LDU_FRAME_LENGTH_BYTES = 216U;
    const uint32_t  P25_LDU_FRAME_LENGTH_BITS = P25_LDU_FRAME_LENGTH_BYTES * 8U;

    const uint32_t  P25_TSDU_FRAME_LENGTH_BYTES = 45U;
    const uint32_t  P25_TSDU_FRAME_LENGTH_BITS = P25_TSDU_FRAME_LENGTH_BYTES * 8U;

    const uint32_t  P25_TSDU_DOUBLE_FRAME_LENGTH_BYTES = 72U;
    const uint32_t  P25_TSDU_DOUBLE_FRAME_LENGTH_BITS = P25_TSDU_DOUBLE_FRAME_LENGTH_BYTES * 8U;

    const uint32_t  P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES = 90U;
    const uint32_t  P25_TSDU_TRIPLE_FRAME_LENGTH_BITS = P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES * 8U;

    const uint32_t  P25_TDULC_FRAME_LENGTH_BYTES = 54U;
    const uint32_t  P25_TDULC_FRAME_LENGTH_BITS = P25_TDULC_FRAME_LENGTH_BYTES * 8U;

    const uint32_t  P25_NID_LENGTH_BYTES = 8U;
    const uint32_t  P25_NID_LENGTH_BITS = P25_NID_LENGTH_BYTES * 8U;

    const uint8_t   P25_SYNC_BYTES[] = { 0x55U, 0x75U, 0xF5U, 0xFFU, 0x77U, 0xFFU };
    const uint32_t  P25_SYNC_LENGTH_BYTES = 6U;
    const uint32_t  P25_SYNC_LENGTH_BITS = P25_SYNC_LENGTH_BYTES * 8U;
    const uint8_t   P25_START_SYNC = 0x5FU;

    const uint32_t  P25_PREAMBLE_LENGTH_BYTES = P25_SYNC_LENGTH_BYTES + P25_NID_LENGTH_BYTES;
    const uint32_t  P25_PREAMBLE_LENGTH_BITS = P25_SYNC_LENGTH_BITS + P25_NID_LENGTH_BITS;

    const uint32_t  P25_LDU_FRAME_TIME = 180U;

    const uint32_t  P25_HDU_LENGTH_BYTES = 81U;
    const uint32_t  P25_LDU_LC_LENGTH_BYTES = 18U;

    const uint32_t  P25_TDULC_FEC_LENGTH_BYTES = 36U;
    const uint32_t  P25_TDULC_LENGTH_BYTES = 18U;

    const uint32_t  P25_TSBK_FEC_LENGTH_BYTES = 25U;
    const uint32_t  P25_TSBK_FEC_LENGTH_BITS = P25_TSBK_FEC_LENGTH_BYTES * 8U - 4U; // Trellis is actually 196 bits
    const uint32_t  P25_TSBK_LENGTH_BYTES = 12U;

    const uint32_t  P25_MAX_PDU_COUNT = 32U;
    const uint32_t  P25_MAX_PDU_LENGTH = 512U;
    const uint32_t  P25_PDU_HEADER_LENGTH_BYTES = 12U;
    const uint32_t  P25_PDU_CONFIRMED_LENGTH_BYTES = 18U;
    const uint32_t  P25_PDU_CONFIRMED_DATA_LENGTH_BYTES = 16U;
    const uint32_t  P25_PDU_UNCONFIRMED_LENGTH_BYTES = 12U;

    const uint32_t  P25_PDU_FEC_LENGTH_BYTES = 25U;
    const uint32_t  P25_PDU_FEC_LENGTH_BITS = P25_PDU_FEC_LENGTH_BYTES * 8U - 4U; // Trellis is actually 196 bits

    const uint32_t  P25_MI_LENGTH_BYTES = 9U;
    const uint32_t  P25_RAW_IMBE_LENGTH_BYTES = 11U;

    const uint32_t  P25_SS0_START = 70U;
    const uint32_t  P25_SS1_START = 71U;
    const uint32_t  P25_SS_INCREMENT = 72U;

    const uint8_t   P25_NULL_IMBE[] = { 0x04U, 0x0CU, 0xFDU, 0x7BU, 0xFBU, 0x7DU, 0xF2U, 0x7BU, 0x3DU, 0x9EU, 0x45U };

    const uint8_t   P25_MFG_STANDARD = 0x00U;
    const uint8_t   P25_MFG_MOT = 0x90U;
    const uint8_t   P25_MFG_DVM = 0xFEU; // internal P25 MFId used for internal signalling (shouldn't be air transmitted!)

    const uint8_t   P25_MOT_CALLSIGN_LENGTH_BYTES = 8U;

    const uint8_t   P25_ALGO_UNENCRYPT = 0x80U;

    const uint8_t   P25_IDEN_UP_VU_BW_625K = 0x04U;
    const uint8_t   P25_IDEN_UP_VU_BW_125K = 0x05U;

    const uint8_t   P25_SVC_CLS_INVALID = 0x00U;
    const uint8_t   P25_SVC_CLS_COMPOSITE = 0x01U;
    const uint8_t   P25_SVC_CLS_VOICE = 0x10U;
    const uint8_t   P25_SVC_CLS_DATA = 0x20U;
    const uint8_t   P25_SVC_CLS_REG = 0x40U;
    const uint8_t   P25_SVC_CLS_AUTH = 0x80U;

    const uint32_t  P25_SYS_SRV_NET_ACTIVE = 0x0200000U;
    const uint32_t  P25_SYS_SRV_GROUP_VOICE = 0x0080000U;
    const uint32_t  P25_SYS_SRV_IND_VOICE = 0x0040000U;
    const uint32_t  P25_SYS_SRV_PSTN_UNIT_VOICE = 0x0020000U;
    const uint32_t  P25_SYS_SRV_UNIT_PSTN_VOICE = 0x0010000U;
    const uint32_t  P25_SYS_SRV_GROUP_DATA = 0x0004000U;
    const uint32_t  P25_SYS_SRV_IND_DATA = 0x0002000U;
    const uint32_t  P25_SYS_SRV_UNIT_REG = 0x0000800U;
    const uint32_t  P25_SYS_SRV_GROUP_AFF = 0x0000400U;
    const uint32_t  P25_SYS_SRV_GROUP_AFF_Q = 0x0000200U;
    const uint32_t  P25_SYS_SRV_USER_STS = 0x0000040U;
    const uint32_t  P25_SYS_SRV_USER_MSG = 0x0000020U;
    const uint32_t  P25_SYS_SRV_UNIT_STS = 0x0000010U;
    const uint32_t  P25_SYS_SRV_USER_STS_Q = 0x0000008U;
    const uint32_t  P25_SYS_SRV_UNIT_STS_Q = 0x0000004U;
    const uint32_t  P25_SYS_SRV_CALL_ALRT = 0x0000002U;
    const uint32_t  P25_SYS_SRV_EMERGENCY = 0x0000001U;

    const uint32_t  P25_SYS_SRV_DEFAULT = P25_SYS_SRV_EMERGENCY | P25_SYS_SRV_CALL_ALRT | P25_SYS_SRV_USER_MSG |
        P25_SYS_SRV_UNIT_STS_Q | P25_SYS_SRV_USER_STS_Q | P25_SYS_SRV_UNIT_STS | P25_SYS_SRV_USER_STS |
        P25_SYS_SRV_IND_DATA | P25_SYS_SRV_IND_VOICE | P25_SYS_SRV_GROUP_DATA | P25_SYS_SRV_GROUP_VOICE;
    const uint32_t  P25_SYS_SRV_TRUNK = P25_SYS_SRV_DEFAULT | P25_SYS_SRV_GROUP_AFF | P25_SYS_SRV_UNIT_REG | P25_SYS_SRV_GROUP_AFF_Q;

    const uint8_t   P25_CFVA_CONV = 0x08U;
    const uint8_t   P25_CFVA_FAILURE = 0x04U;
    const uint8_t   P25_CFVA_VALID = 0x02U;
    const uint8_t   P25_CFVA_NETWORK = 0x01U;

    const uint8_t   P25_RSP_ACCEPT = 0x00U;
    const uint8_t   P25_RSP_FAIL = 0x01U;
    const uint8_t   P25_RSP_DENY = 0x02U;
    const uint8_t   P25_RSP_REFUSED = 0x03U;

    const uint8_t   P25_ANS_RSP_PROCEED = 0x20U;
    const uint8_t   P25_ANS_RSP_DENY = 0x21U;
    const uint8_t   P25_ANS_RSP_WAIT = 0x22U;

    const uint8_t   P25_CAN_SRV_NONE = 0x00U;
    const uint8_t   P25_CAN_SRV_TERM_QUE = 0x10U;
    const uint8_t   P25_CAN_SRV_TERM_RSRC_ASSIGN = 0x20U;

    const uint32_t  P25_DENY_RSN_REQ_UNIT_NOT_VALID = 0x10U;
    const uint32_t  P25_DENY_RSN_REQ_UNIT_NOT_AUTH = 0x11U;
    const uint32_t  P25_DENY_RSN_TGT_UNIT_NOT_VALID = 0x20U;
    const uint32_t  P25_DENY_RSN_TGT_UNIT_NOT_AUTH = 0x21U;
    const uint32_t  P25_DENY_RSN_TGT_UNIT_REFUSED = 0x2FU;
    const uint32_t  P25_DENY_RSN_TGT_GROUP_NOT_VALID = 0x30U;
    const uint32_t  P25_DENY_RSN_TGT_GROUP_NOT_AUTH = 0x31U;
    const uint32_t  P25_DENY_RSN_SITE_ACCESS_DENIAL = 0x60U;
    const uint32_t  P25_DENY_RSN_PTT_COLLIDE = 0x67U;
    const uint32_t  P25_DENY_RSN_PTT_BONK = 0x77U;
    const uint32_t  P25_DENY_RSN_SYS_UNSUPPORTED_SVC = 0xFFU;

    const uint32_t  P25_QUE_RSN_REQ_ACTIVE_SERVICE = 0x10U;
    const uint32_t  P25_QUE_RSN_TGT_ACTIVE_SERVICE = 0x20U;
    const uint32_t  P25_QUE_RSN_TGT_UNIT_QUEUED = 0x2FU;
    const uint32_t  P25_QUE_RSN_CHN_RESOURCE_NOT_AVAIL = 0x40U;

    const uint32_t  P25_EXT_FNCT_CHECK = 0x0000U;           // Radio Check
    const uint32_t  P25_EXT_FNCT_UNINHIBIT = 0x007EU;       // Radio Uninhibit
    const uint32_t  P25_EXT_FNCT_INHIBIT = 0x007FU;         // Radio Inhibit
    const uint32_t  P25_EXT_FNCT_CHECK_ACK = 0x0080U;       // Radio Check Ack
    const uint32_t  P25_EXT_FNCT_UNINHIBIT_ACK = 0x00FEU;   // Radio Uninhibit Ack
    const uint32_t  P25_EXT_FNCT_INHIBIT_ACK = 0x00FFU;     // Radio Inhibit Ack

    const uint32_t  P25_WACN_STD_DEFAULT = 0xBB800U;

    const uint32_t  P25_SID_STD_DEFAULT = 0x001U;

    const uint32_t  P25_WUID_FNE = 0xFFFFFCU;
    const uint32_t  P25_WUID_REG = 0xFFFFFEU;
    const uint32_t  P25_WUID_ALL = 0xFFFFFFU;

    const uint32_t  P25_TGID_ALL = 0xFFFFU;

    const uint32_t  DEFAULT_SILENCE_THRESHOLD = 124U;
    const uint32_t  MAX_P25_VOICE_ERRORS = 1233U;

    // PDU Format Type(s)
    const uint8_t   PDU_FMT_RSP = 0x03U;
    const uint8_t   PDU_FMT_UNCONFIRMED = 0x15U;
    const uint8_t   PDU_FMT_CONFIRMED = 0x16U;
    const uint8_t   PDU_FMT_AMBT = 0x17U;

    // PDU SAP 
    const uint8_t   PDU_SAP_USER_DATA = 0x00U;
    const uint8_t   PDU_SAP_ENC_USER_DATA = 0x01U;

    const uint8_t   PDU_SAP_PACKET_DATA = 0x04U;

    const uint8_t   PDU_SAP_ARP = 0x05U;

    const uint8_t   PDU_SAP_SNDCP_CTRL_DATA = 0x06U;

    const uint8_t   PDU_SAP_EXT_ADDR = 0x1FU;

    const uint8_t   PDU_SAP_REG = 0x20U;

    const uint8_t   PDU_SAP_UNENC_KMM = 0x28U;
    const uint8_t   PDU_SAP_ENC_KMM = 0x29U;

    const uint8_t   PDU_SAP_TRUNK_CTRL = 0x3DU;

    // PDU ACK Class
    const uint8_t   PDU_ACK_CLASS_ACK = 0x00U;
    const uint8_t   PDU_ACK_CLASS_NACK = 0x01U;
    const uint8_t   PDU_ACK_CLASS_ACK_RETRY = 0x02U;

    // PDU ACK Type(s)
    const uint8_t   PDU_ACK_TYPE_ACK = 0x01U;

    const uint8_t   PDU_ACK_TYPE_NACK_ILLEGAL = 0x00U;      // Illegal Format
    const uint8_t   PDU_ACK_TYPE_NACK_PACKET_CRC = 0x01U;   // Packet CRC
    const uint8_t   PDU_ACK_TYPE_NACK_MEMORY_FULL = 0x02U;  // Memory Full
    const uint8_t   PDU_ACK_TYPE_NACK_SEQ = 0x03U;          // Out of logical sequence FSN
    const uint8_t   PDU_ACK_TYPE_NACK_UNDELIVERABLE = 0x04U;// Undeliverable
    const uint8_t   PDU_ACK_TYPE_NACK_OUT_OF_SEQ = 0x05U;   // Out of sequence, N(S) != V(R) or V(R) + 1
    const uint8_t   PDU_ACK_TYPE_NACK_INVL_USER = 0x06U;    // Invalid User disallowed by the system

    // PDU Registration Type(s)
    const uint8_t   PDU_REG_TYPE_REQ_CNCT = 0x00U;
    const uint8_t   PDU_REG_TYPE_REQ_DISCNCT = 0x01U;
    const uint8_t   PDU_REG_TYPE_RSP_ACCPT = 0x04U;
    const uint8_t   PDU_REG_TYPE_RSP_DENY = 0x05U;

    // PDU SNDCP Type(s)
    const uint8_t   PDU_TYPE_SNDCP_ACT_TDS_CTX_ACCPT = 0x00U;
    const uint8_t   PDU_TYPE_SNDCP_DEACT_TDS_CTX_ACCPT = 0x01U;
    const uint8_t   PDU_TYPE_SNDCP_DEACT_TDS_CTX_REQ = 0x02U;
    const uint8_t   PDU_TYPE_SNDCP_ACT_TDS_CTX_REJECT = 0x03U;
    const uint8_t   PDU_TYPE_SNDCP_RF_UNCONFIRMED = 0x04U;
    const uint8_t   PDU_TYPE_SNDCP_RF_CONFIRMED = 0x05U;

    const uint8_t   LC_SVC_OPT_EMERGENCY = 0x80U;
    const uint8_t   LC_SVC_OPT_ENCRYPTION = 0x40U;

    // LDUx/TDULC Link Control Opcode(s)
    const uint8_t   LC_GROUP = 0x00U;                   // GRP VCH USER - Group Voice Channel User
    const uint8_t   LC_GROUP_UPDT = 0x02U;              // GRP VCH UPDT - Group Voice Channel Update
    const uint8_t   LC_PRIVATE = 0x03U;                 // UU VCH USER - Unit-to-Unit Voice Channel User
    const uint8_t   LC_UU_ANS_REQ = 0x05U;              // UU ANS REQ - Unit to Unit Answer Request 
    const uint8_t   LC_TEL_INT_VCH_USER = 0x06U;        // TEL INT VCH USER - Telephone Interconnect Voice Channel User 
    const uint8_t   LC_TEL_INT_ANS_RQST = 0x07U;        // TEL INT ANS RQST - Telephone Interconnect Answer Request 
    const uint8_t   LC_CALL_TERM = 0x0FU;               // CALL TERM - Call Termination or Cancellation
    const uint8_t   LC_IDEN_UP = 0x18U;                 // IDEN UP - Channel Identifier Update
    const uint8_t   LC_SYS_SRV_BCAST = 0x20U;           // SYS SRV BCAST - System Service Broadcast
    const uint8_t   LC_ADJ_STS_BCAST = 0x22U;           // ADJ STS BCAST - Adjacent Site Status Broadcast
    const uint8_t   LC_RFSS_STS_BCAST = 0x23U;          // RFSS STS BCAST - RFSS Status Broadcast
    const uint8_t   LC_NET_STS_BCAST = 0x24U;           // NET STS BCAST - Network Status Broadcast

    // TSBK ISP/OSP Shared Opcode(s)
    const uint8_t   TSBK_IOSP_GRP_VCH = 0x00U;          // GRP VCH REQ - Group Voice Channel Request (ISP), GRP VCH GRANT - Group Voice Channel Grant (OSP)
    const uint8_t   TSBK_IOSP_UU_VCH = 0x04U;           // UU VCH REQ - Unit-to-Unit Voice Channel Request (ISP), UU VCH GRANT - Unit-to-Unit Voice Channel Grant (OSP)
    const uint8_t   TSBK_IOSP_UU_ANS = 0x05U;           // UU ANS RSP - Unit-to-Unit Answer Response (ISP), UU ANS REQ - Unit-to-Unit Answer Request (OSP)
    const uint8_t   TSBK_IOSP_TELE_INT_DIAL = 0x08U;    // TELE INT DIAL REQ - Telephone Interconnect Request - Explicit (ISP), TELE INT DIAL GRANT - Telephone Interconnect Grant (OSP)
    const uint8_t   TSBK_IOSP_TELE_INT_ANS = 0x0AU;     // TELE INT ANS RSP - Telephone Interconnect Answer Response (ISP), TELE INT ANS REQ - Telephone Interconnect Answer Request (OSP)
    const uint8_t   TSBK_IOSP_STS_UPDT = 0x18U;         // STS UPDT REQ - Status Update Request (ISP), STS UPDT - Status Update (OSP)
    const uint8_t   TSBK_IOSP_STS_Q = 0x1AU;            // STS Q REQ - Status Query Request (ISP), STS Q - Status Query (OSP)
    const uint8_t   TSBK_IOSP_MSG_UPDT = 0x1CU;         // MSG UPDT REQ - Message Update Request (ISP), MSG UPDT - Message Update (OSP)
    const uint8_t   TSBK_IOSP_CALL_ALRT = 0x1FU;        // CALL ALRT REQ - Call Alert Request (ISP), CALL ALRT - Call Alert (OSP)
    const uint8_t   TSBK_IOSP_ACK_RSP = 0x20U;          // ACK RSP U - Acknowledge Response - Unit (ISP), ACK RSP FNE - Acknowledge Response - FNE (OSP)
    const uint8_t   TSBK_IOSP_EXT_FNCT = 0x24U;         // EXT FNCT RSP - Extended Function Response (ISP), EXT FNCT CMD - Extended Function Command (OSP)
    const uint8_t   TSBK_IOSP_GRP_AFF = 0x28U;          // GRP AFF REQ - Group Affiliation Request (ISP), GRP AFF RSP - Group Affiliation Response (OSP)
    const uint8_t   TSBK_IOSP_U_REG = 0x2CU;            // U REG REQ - Unit Registration Request (ISP), U REG RSP - Unit Registration Response (OSP)

    // TSBK Inbound Signalling Packet (ISP) Opcode(s)
    const uint8_t   TSBK_ISP_TELE_INT_PSTN_REQ = 0x09U; // TELE INT PSTN REQ - Telephone Interconnect Request - Implicit
    const uint8_t   TSBK_ISP_SNDCP_CH_REQ = 0x12U;      // SNDCP CH REQ - SNDCP Data Channel Request
    const uint8_t   TSBK_ISP_STS_Q_RSP = 0x19U;         // STS Q RSP - Status Query Response
    const uint8_t   TSBK_ISP_CAN_SRV_REQ = 0x23U;       // CAN SRV REQ - Cancel Service Request
    const uint8_t   TSBK_ISP_EMERG_ALRM_REQ = 0x27U;    // EMERG ALRM REQ - Emergency Alarm Request
    const uint8_t   TSBK_ISP_GRP_AFF_Q_RSP = 0x29U;     // GRP AFF Q RSP - Group Affiliation Query Response
    const uint8_t   TSBK_ISP_U_DEREG_REQ = 0x2BU;       // U DE REG REQ - Unit De-Registration Request
    const uint8_t   TSBK_ISP_LOC_REG_REQ = 0x2DU;       // LOC REG REQ - Location Registration Request

    // TSBK Outbound Signalling Packet (OSP) Opcode(s)
    const uint8_t   TSBK_OSP_GRP_VCH_GRANT_UPD = 0x02U; // GRP VCH GRANT UPD - Group Voice Channel Grant Update
    const uint8_t   TSBK_OSP_UU_VCH_GRANT_UPD = 0x06U;  // UU VCH GRANT UPD - Unit-to-Unit Voice Channel Grant Update
    const uint8_t   TSBK_OSP_SNDCP_CH_GNT = 0x14U;      // SNDCP CH GNT - SNDCP Data Channel Grant
    const uint8_t   TSBK_OSP_SNDCP_CH_ANN = 0x16U;      // SNDCP CH ANN - SNDCP Data Channel Announcement
    const uint8_t   TSBK_OSP_DENY_RSP = 0x27U;          // DENY RSP - Deny Response
    const uint8_t   TSBK_OSP_SCCB_EXP = 0x29U;          // SCCB - Secondary Control Channel Broadcast - Explicit 
    const uint8_t   TSBK_OSP_GRP_AFF_Q = 0x2AU;         // GRP AFF Q - Group Affiliation Query
    const uint8_t   TSBK_OSP_LOC_REG_RSP = 0x2BU;       // LOC REG RSP - Location Registration Response
    const uint8_t   TSBK_OSP_U_REG_CMD = 0x2DU;         // U REG CMD - Unit Registration Command
    const uint8_t   TSBK_OSP_U_DEREG_ACK = 0x2FU;       // U DE REG ACK - Unit De-Registration Acknowledge
    const uint8_t   TSBK_OSP_QUE_RSP = 0x33U;           // QUE RSP - Queued Response
    const uint8_t   TSBK_OSP_IDEN_UP_VU = 0x34U;        // IDEN UP VU - Channel Identifier Update for VHF/UHF Bands
    const uint8_t   TSBK_OSP_SYS_SRV_BCAST = 0x38U;     // SYS SRV BCAST - System Service Broadcast
    const uint8_t   TSBK_OSP_SCCB = 0x39U;              // SCCB - Secondary Control Channel Broadcast
    const uint8_t   TSBK_OSP_RFSS_STS_BCAST = 0x3AU;    // RFSS STS BCAST - RFSS Status Broadcast
    const uint8_t   TSBK_OSP_NET_STS_BCAST = 0x3BU;     // NET STS BCAST - Network Status Broadcast
    const uint8_t   TSBK_OSP_ADJ_STS_BCAST = 0x3CU;     // ADJ STS BCAST - Adjacent Site Status Broadcast
    const uint8_t   TSBK_OSP_IDEN_UP = 0x3DU;           // IDEN UP - Channel Identifier Update

    // TSBK Motorola Outbound Signalling Packet (OSP) Opcode(s)
    const uint8_t   TSBK_OSP_MOT_GRG_ADD = 0x00U;       // MOT GRG ADD - Motorola / Group Regroup Add (Patch Supergroup)
    const uint8_t   TSBK_OSP_MOT_GRG_DEL = 0x01U;       // MOT GRG DEL - Motorola / Group Regroup Delete (Unpatch Supergroup)
    const uint8_t   TSBK_OSP_MOT_GRG_VCH_GRANT = 0x02U; // MOT GRG GROUP VCH GRANT / Group Regroup Voice Channel Grant
    const uint8_t   TSBK_OSP_MOT_GRG_VCH_UPD = 0x03U;   // MOT GRG GROUP VCH GRANT UPD / Group Regroup Voice Channel Grant Update
    const uint8_t   TSBK_OSP_MOT_CC_BSI = 0x0BU;        // MOT CC BSI - Motorola / Control Channel Base Station Identifier
    const uint8_t   TSBK_OSP_MOT_PSH_CCH = 0x0EU;       // MOT PSH CCH - Motorola / Planned Control Channel Shutdown

    // Data Unit ID(s)
    const uint8_t   P25_DUID_HDU = 0x00U;               // Header Data Unit
    const uint8_t   P25_DUID_TDU = 0x03U;               // Simple Terminator Data Unit
    const uint8_t   P25_DUID_LDU1 = 0x05U;              // Logical Link Data Unit 1
    const uint8_t   P25_DUID_TSDU = 0x07U;              // Trunking System Data Unit
    const uint8_t   P25_DUID_LDU2 = 0x0AU;              // Logical Link Data Unit 2
    const uint8_t   P25_DUID_PDU = 0x0CU;               // Packet Data Unit 
    const uint8_t   P25_DUID_TDULC = 0x0FU;             // Terminator Data Unit with Link Control
} // namespace p25

// ---------------------------------------------------------------------------
//  Namespace Prototypes
// ---------------------------------------------------------------------------
namespace edac { }
namespace p25
{
    namespace edac
    {
        using namespace ::edac;
    } // namespace edac
} // namespace p25

#endif // __P25_DEFINES_H__
