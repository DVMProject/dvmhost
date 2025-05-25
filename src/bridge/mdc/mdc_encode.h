// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2011 Matthew Kaufman
 *
 */
/**
 * @file mdc_encode.h
 * @ingroup bridge
 * @file mdc_encode.c
 * @ingroup bridge
 */
/*-
 * mdc_encode.h
 *  header for mdc_encode.c
 *
 * 9 October 2010 - typedefs for easier porting
 *
 * Author: Matthew Kaufman (matthew@eeph.com)
 *
 * Copyright (c) 2005  Matthew Kaufman  All rights reserved.
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
#if !defined(__MDC_ENCODE_H__)
#define __MDC_ENCODE_H__

#include "mdc/mdc_types.h"

//#define FILL_FINAL                    // fills the end of the last block with zeros, rather than returning fewer samples than requested
//#define MDC_ENCODE_FULL_AMPLITUDE     // encode at 100% amplitude (default is 68% amplitude for recommended deviation)

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

/**
 * @brief 
 */
typedef struct {
    mdc_int_t loaded;
    mdc_int_t bpos;
    mdc_int_t ipos;
    mdc_int_t preamble_set;
    mdc_int_t preamble_count;
    mdc_u32_t thu;
    mdc_u32_t tthu;
    mdc_u32_t incru;
    mdc_u32_t incru18;
    mdc_int_t state;
    mdc_int_t lb;
    mdc_int_t xorb;
    mdc_u8_t data[14 + 14 + 5 + 7];
} mdc_encoder_t;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new mdc_encoder object.
 * @param sampleRate The sampling rate in Hz.
 * @returns mdc_encoder_t* An mdc_encoder object or null if failure.
 */
mdc_encoder_t* mdc_encoder_new(int sampleRate);

/**
 * @brief 
 * @param[in] encoder Instance of mdc_encoder object.
 * @param preambleLength length of additional preamble (in bytes) - preamble time is 6.66 msec * preambleLength.
 * @returns int -1 for error, 0 otherwise
 */
int mdc_encoder_set_preamble(mdc_encoder_t* encoder, int preambleLength);

/**
 * @brief Set up a normal-length MDC packet for transmission.
 * @param[in] encoder Instance of mdc_encoder object.
 * @param op MDC Opcode.
 * @param arg MDC Argument.
 * @param unitID Unit ID.
 * @returns int -1 for error, 0 otherwise
 */
int mdc_encoder_set_packet(mdc_encoder_t* encoder, mdc_u8_t op, mdc_u8_t arg, mdc_u16_t unitID);

/**
 * @brief Set up a double-length MDC packet for transmission.
 * @param[in] encoder Instance of mdc_encoder object.
 * @param op MDC Opcode.
 * @param arg MDC Argument.
 * @param unitID Unit ID.
 * @param extra0 1st extra byte.
 * @param extra1 2nd extra byte.
 * @param extra2 3rd extra byte.
 * @param extra3 4th extra byte.
 * @returns int -1 for error, 0 otherwise
 */
int mdc_encoder_set_double_packet(mdc_encoder_t* encoder, mdc_u8_t op, mdc_u8_t arg,
    mdc_u16_t unitID, mdc_u8_t extra0, mdc_u8_t extra1, mdc_u8_t extra2, mdc_u8_t extra3);

/**
 * @brief Get generated output audio samples from encoder.
 * @param[out] buffer Sample buffer to write into.
 * @param[out] bufferSize The size (in samples) of the sample buffer.
 * @returns int -1 for error, otherwise returns the number of samples written
 *  into the buffer (will be equal to bufferSize unless the end has
 *  been reached, in which case the last block may be less than
 *  bufferSize and all subsequent calls will return zero, until
 *  a new packet is loaded for transmission
 */
int mdc_encoder_get_samples(mdc_encoder_t* encoder, mdc_sample_t* buffer, int bufferSize);

#ifdef __cplusplus
}
#endif

#endif // __MDC_ENCODE_H__
