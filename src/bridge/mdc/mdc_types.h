// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file mdc_types.h
 * @ingroup bridge
 */
/*-
 * mdc_types.h
 *  common typedef header for mdc_decode.h, mdc_encode.h
 *
 * Author: Matthew Kaufman (matthew@eeph.com)
 *
 * Copyright (c) 2010  Matthew Kaufman  All rights reserved.
 * 
 *  This file is part of Matthew Kaufman's MDC Encoder/Decoder Library
 *
 *  The MDC Encoder/Decoder Library is free software; you can
 *  redistribute it and/or modify it under the terms of version 2 of
 *  the GNU General Public License as published by the Free Software
 *  Foundation.
 *
 *  If you cannot comply with the terms of this license, contact
 *  the author for alternative license arrangements or do not use
 *  or redistribute this software.
 *
 *  The MDC Encoder/Decoder Library is distributed in the hope
 *  that it will be useful, but WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 *  USA.
 *
 *  or see http://www.gnu.org/copyleft/gpl.html
 *
-*/
#if !defined(__MDC_TYPES_H__)
#define __MDC_TYPES_H__

// ---------------------------------------------------------------------------
//  Types
// ---------------------------------------------------------------------------

typedef int mdc_s32;
typedef unsigned int mdc_u32_t;
typedef short mdc_s16_t;
typedef unsigned short mdc_u16_t;
typedef char mdc_s8_t;
typedef unsigned char mdc_u8_t;
typedef int mdc_int_t;

#ifndef MDC_FIXEDMATH
typedef double mdc_float_t;
#endif // MDC_FIXEDMATH

/* to change the data type, set this typedef: */
typedef short mdc_sample_t;
#define MDC_SAMPLE_FORMAT_S16

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

/*
 * Single Packets
 */
/// <summary>
/// Emergency
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     PTT_PRE (0x80)
///     EMERG_UNKNW (0x81)
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_EMERGENCY 0x00U
/// <summary>
/// Emergency Acknowledge
/// </summary>
/// <remarks>
/// Has no arguments, should always have NO_ARG.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_EMERGENCY_ACK 0x20U

/// <summary>
/// PTT ID
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     NO_ARG (0x00) value indicates Post- ID
///     PTT_PRE (0x80) value indicated Pre- ID
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_PTT_ID 0x01U

/// <summary>
/// Radio Check
/// </summary>
/// <remarks>
/// Always takes the argument RADIO_CHECK (0x85).
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_RADIO_CHECK 0x63U
/// <summary>
/// Radio Check Acknowledge
/// </summary>
/// <remarks>
/// Has no arguments, should always have NO_ARG.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_RADIO_CHECK_ACK 0x03U

/// <summary>
/// Message
/// </summary>
/// <remarks>
/// Argument contains the ID of the given message. This opcode
/// optionally supports setting of bit 7 for required acknowledgement
/// of receipt.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack (or) Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_MESSAGE 0x07U
#define OP_MESSAGE_WITH_ACK 0x47U
/// <summary>
/// Message/Status Acknowledge
/// </summary>
/// <remarks>
/// Always takes the argument NO_ARG.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_MESSAGE_ACK 0x23U

/// <summary>
/// Status Request
/// </summary>
/// <remarks>
/// Always takes the argument STATUS_REQ (0x06).
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_STATUS_REQUEST 0x22U

/// <summary>
/// Status Response
/// </summary>
/// <remarks>
/// Argument contains the ID of the given status.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_STATUS_RESPONSE 0x06U

/// <summary>
/// Remote Monitor
/// </summary>
/// <remarks>
/// Always takes the argument REMOTE_MONITOR (0x8A).
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_REMOTE_MONITOR 0x11U

/// <summary>
/// Selective Radio Inhibit
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     NO_ARG (0x00) value indicates Unit ID to inhibit
///     CANCEL_INHIBIT (0x0C) value indicates Unit ID to uninhibit
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_RADIO_INHIBIT 0x2BU

/// <summary>
/// Selective Radio Inhibit Acknowledge
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     NO_ARG (0x00) value indicates the Unit ID acknowledges the inhibit
///     CANCEL_INHIBIT (0x0C) value indicates the Unit ID acknowledges is uninhibited
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_RADIO_INHIBIT_ACK 0x0BU

/// <summary>
/// Repeater Access Code
/// </summary>
/// <remarks>
/// This operand is doubled to 0x41?
/// 
/// Always takes the argument RTT (0x01).
/// 
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_RAC 0x30U

/// <summary>
/// Request to Talk
/// </summary>
/// <remarks>
/// This operand is doubled to 0x41?
/// 
/// Always takes the argument RTT (0x01).
/// 
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_RTT_1 0x40U
#define OP_RTT_2 0x41U

/// <summary>
/// Request to Talk Acknowledge
/// </summary>
/// <remarks>
/// Has no arguments, should always have NO_ARG.
/// 
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_RTT_ACK 0x23U

/// <summary>
/// Simple Status
/// </summary>
/// <remarks>
/// Argument contains the ID of the given status.
/// 
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_SIMPLE_STATUS 0x46U

/*
 * Double Packets
 */
/// <summary>
/// Double Packet Operation (0x35)
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     DOUBLE_PACKET_FROM (0x89) value indicates who transmitted the double packet
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_DOUBLE_PACKET_TYPE1 0x35U
/// <summary>
/// Double Packet Operation (0x55)
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </summary>
#define OP_DOUBLE_PACKET_TYPE2 0x55U

/// <summary>
/// Call Alert/Page
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     CALL_ALERT_TO (0x0D) value indicates the intended target of the call
///     
/// The DOUBLE_PACKET_FROM (0x89) of the DOUBLE_PACKET_TYPE1 frame will contain the unit ID that transmitted
/// the call alert. This opcode expects an ack, regardless of the bit 7 setting.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Data
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_CALL_ALERT_ACK_EXPECTED 0x83U
/// <summary>
/// Call Alert/Page
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     CALL_ALERT_TO (0x0D) value indicates the intended target of the call
///     
/// The DOUBLE_PACKET_FROM (0x89) of the DOUBLE_PACKET_TYPE1 frame will contain the unit ID that transmitted
/// the call alert. This opcode does not expect an ack, regardless of the bit 7 setting.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Data
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_CALL_ALERT_NOACK_EXPECTED 0x81U

/// <summary>
/// Call Alert/Page Acknowledge
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     NO_ARG (0x00) value indicates the unit ID which initiated the call
///     
/// The DOUBLE_PACKET_FROM (0x89) of the DOUBLE_PACKET_TYPE1 frame will contain the unit ID that transmitted
/// the acknowledge.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Data
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_CALL_ALERT_ACK 0xA0U

/// <summary>
/// Voice Selective Call
/// </summary>
/// <remarks>
/// This opcode is doubled to operand 0x82 as well as 0x80?
/// 
/// Supports the following arguments:
///     SELECTIVE_CALL_TO (0x15) value indicates the intended target of the call
///
/// The DOUBLE_PACKET_FROM (0x89) of the DOUBLE_PACKET_TYPE1 frame will contain the unit ID that transmitted
/// the call alert.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Data
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Inbound
/// </remarks>
#define OP_SELECTIVE_CALL_1 0x80U
#define OP_SELECTIVE_CALL_2 0x82U

/// <summary>
/// Over-the-Air Rekey
/// </summary>
/// <remarks>
/// Supports the following arguments:
///     OTAR_UNK1 0x74 ?
///     OTAR_UNK2 0x76 ?
///
/// The DOUBLE_PACKET_FROM (0x89) of the DOUBLE_PACKET_TYPE1 frame will contain the unit ID that transmitted
/// the call alert.
///
/// Opcode Bits:
///     8 = Command/Data Packet     = Command
///     7 = Ack/No Ack Required     = No Ack
///     6 = Inbound/Outbound Packet = Outbound
/// </remarks>
#define OP_OTAR 0x86U

/// <summary>
/// No Argument
/// </summary>
#define ARG_NO_ARG 0x00U

/*
 * Single Packets
 */
/// <summary>
/// Emergency Argument (unknown use)
/// </summary>
#define ARG_EMERG_UNKNW 0x81U

/// <summary>
/// PTT ID Pre-
/// </summary>
#define ARG_PTT_PRE 0x80U

/// <summary>
/// Radio Check
/// </summary>
#define ARG_RADIO_CHECK 0x85U

/// <summary>
/// Status Request
/// </summary>
#define ARG_STATUS_REQ 0x06U

/// <summary>
/// Remote Monitor
/// </summary>
#define ARG_REMOTE_MONITOR 0x8AU

/// <summary>
/// Cancel Selective Radio Inhibit
/// </summary>
#define ARG_CANCEL_INHIBIT 0x0CU

/// <summary>
/// Request to Talk
/// </summary>
#define ARG_RTT 0x01U

/*
 * Double Packets
 */
/// <summary>
/// Double To Argument
/// </summary>
/// <remarks>Unit ID represents what radio ID the call is targeting</remarks>
#define ARG_DOUBLE_PACKET_TO 0x89U

/// <summary>
/// Call Alert Argument
/// </summary>
/// <remarks>Unit ID represents what radio ID the call originated from</remarks>
#define ARG_CALL_ALERT 0x0DU

/// <summary>
/// OTAR Argument Unknown 0xC6
/// </summary>
#define ARG_OTAR_DOUBLE 0xC6U

/// <summary>
/// OTAR Argument Unknown 0x74
/// </summary>
#define ARG_OTAR_UNK1 0x74U
/// <summary>
/// OTAR Argument Unknown 0x76
/// </summary>
#define ARG_OTAR_UNK2 0x76U

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 
 * @param crc 
 * @param bitnum 
 * @returns mdc_u16_t 
 */
mdc_u16_t _flip(mdc_u16_t crc, mdc_int_t bitnum);

/**
 * @brief 
 * @param p 
 * @param len 
 * @returns mdc_u16_t 
 */
mdc_u16_t _docrc(mdc_u8_t* p, int len);

#ifdef __cplusplus
}
#endif

#endif // __MDC_TYPES_H__
