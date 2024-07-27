// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 */
/**
 * @file mdc_decode.h
 * @ingroup bridge
 * @file mdc_decode.c
 * @ingroup bridge
 */
/*-
 * mdc_decode.h
 *  header for mdc_decode.c
 *
 * Author: Matthew Kaufman (matthew@eeph.com)
 *
 * Copyright (c) 2005, 2010  Matthew Kaufman  All rights reserved.
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
#if !defined(__MDC_DECODE_H__)
#define __MDC_DECODE_H__

// #define MDC_FIXEDMATH // if you want this, define before mdc_types.h

#include "mdc_types.h"

#define MDC_GDTHRESH 5  // "good bits" threshold

#define MDC_ECC

#define MDC_FOURPOINT	// recommended 4-point method, requires high sample rates (16000 or higher)
#undef  MDC_ONEPOINT    // alternative 1-point method

#ifdef MDC_FOURPOINT
 #define MDC_ND 5  // recommended for four-point method
#endif

#ifdef MDC_ONEPOINT
 #define MDC_ND 4  // recommended for one-point method
#endif

typedef void (*mdc_decoder_callback_t)(int frameCount, // 1 or 2 - if 2 then extra0-3 are valid
    mdc_u8_t op, mdc_u8_t arg, mdc_u16_t unitID,
    mdc_u8_t extra0, mdc_u8_t extra1, mdc_u8_t extra2, mdc_u8_t extra3,
    void *context);

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

/**
 * @brief 
 */
typedef struct
{
//  mdc_float_t th;
    mdc_u32_t thu;
//  mdc_int_t zc; - deprecated
    mdc_int_t xorb;
    mdc_int_t invert;
#ifdef MDC_FOURPOINT
#ifdef MDC_FIXEDMATH
#error "fixed-point math not allowed for fourpoint strategy"
#endif // MDC_FIXEDMATH
    mdc_int_t nlstep;
    mdc_float_t nlevel[10];
#endif  // MDC_FOURPOINT
#ifdef PLL
    mdc_u32_t plt;
#endif
    mdc_u32_t synclow;
    mdc_u32_t synchigh;
    mdc_int_t shstate;
    mdc_int_t shcount;
    mdc_int_t bits[112];
} mdc_decode_unit_t;

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

/**
 * @brief 
 */
typedef struct {
    mdc_decode_unit_t du[MDC_ND];
//  mdc_float_t hyst;
//  mdc_float_t incr;
    mdc_u32_t incru;
#ifdef PLL
    mdc_u32_t zthu;
    mdc_int_t zprev;
    mdc_float_t vprev;
#endif
    mdc_int_t level;
    mdc_int_t good;
    mdc_int_t indouble;
    mdc_u8_t op;
    mdc_u8_t arg;
    mdc_u16_t unitID;
    mdc_u8_t extra0;
    mdc_u8_t extra1;
    mdc_u8_t extra2;
    mdc_u8_t extra3;
    mdc_decoder_callback_t callback;
    void *callback_context;
} mdc_decoder_t;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new mdc_decoder object.
 * @param sampleRate The sampling rate in Hz.
 * @returns mdc_decoder_t* An mdc_decoder object or null if failure.
 */
mdc_decoder_t* mdc_decoder_new(int sampleRate);

/**
 * @brief Process incoming samples using an mdc_decoder object.
 * @param[in] decoder Instance of the mdc_decoder object.
 * @param[out] samples Samples (in format set in mdc_types.h).
 * @param[out] numSamples Count of the number of samples in buffer.
 * @returns int 0 if more samples are needed
 *  -1 if an error occurs
 *  1 if a decoded single packet is available to read (if no callback set)
 *  2 if a decoded double packet is available to read (if no callback set)
 */
int mdc_decoder_process_samples(mdc_decoder_t* decoder, mdc_sample_t* samples, int numSamples);

/**
 * @brief Retrieve last successfully decoded data packet from decoder object.
 * @param[in] decoder Instance of the mdc_decoder object.
 * @param[out] op MDC Opcode.
 * @param[out] arg MDC Argument.
 * @param[out] unitID Unit ID.
 * @returns int -1 for error, 0 otherwise
 */
int mdc_decoder_get_packet(mdc_decoder_t* decoder, mdc_u8_t* op, mdc_u8_t* arg, mdc_u16_t* unitID);

/**
 * @brief Retrieve last successfully decoded double-length packet from decoder object.
 * @param[in] decoder Instance of the mdc_decoder object.
 * @param[out] op MDC Opcode.
 * @param[out] arg MDC Argument.
 * @param[out] unitID Unit ID.
 * @param[out] extra0 1st extra byte.
 * @param[out] extra1 2nd extra byte.
 * @param[out] extra2 3rd extra byte.
 * @param[out] extra3 4th extra byte.
 * @returns int -1 for error, 0 otherwise
 */
int mdc_decoder_get_double_packet(mdc_decoder_t* decoder, mdc_u8_t* op, mdc_u8_t* arg, mdc_u16_t* unitID,
    mdc_u8_t* extra0, mdc_u8_t* extra1, mdc_u8_t* extra2, mdc_u8_t* extra3);

/**
 * @brief Set a callback function to be called upon successful decode
 *  if this is set, the functions mdc_decoder_get_packet and mdc_decoder_get_double_packet
 *  will no longer be functional, instead the callback function is called immediately when
 *  a successful decode happens (from within the context of mdc_decoder_process_samples).
 * @param[in] decoder Instance of the mdc_decoder object.
 * @param callbackFunction 
 * @param context 
 * @returns int -1 for error, 0 otherwise
 */
int mdc_decoder_set_callback(mdc_decoder_t* decoder, mdc_decoder_callback_t callbackFunction, void* context);

#ifdef __cplusplus
}
#endif

#endif // __MDC_DECODE_H__
