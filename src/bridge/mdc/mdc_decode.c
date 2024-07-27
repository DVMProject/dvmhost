// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 */
/*-
 * mdc_decode.c
 *   Decodes a specific format of 1200 BPS XOR-precoded MSK data burst
 *   from input audio samples.
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

#include <stdlib.h>
#include "mdc_decode.h"

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* Create a new mdc_decoder object. */

mdc_decoder_t* mdc_decoder_new(int sampleRate)
{
    mdc_decoder_t* decoder;

    decoder = (mdc_decoder_t*)malloc(sizeof(mdc_decoder_t));
    if(!decoder)
        return (mdc_decoder_t*)0L;

    if (sampleRate == 8000) {
        decoder->incru = 644245094;
    }
    else if (sampleRate == 16000) {
        decoder->incru = 322122547;
    }
    else if (sampleRate == 22050) {
        decoder->incru = 233739716;
    }
    else if (sampleRate == 32000) {
        decoder->incru = 161061274;
    }
    else if (sampleRate == 44100) {
        decoder->incru = 116869858;
    }
    else if (sampleRate == 48000) {
        decoder->incru = 107374182;
    }
    else {
        // WARNING: lower precision than above
        decoder->incru = 1200 * 2 * (0x80000000 / sampleRate);
    }

    decoder->good = 0;
    decoder->indouble = 0;
    decoder->level = 0;

    for(mdc_int_t i = 0; i < MDC_ND; i++)
    {
        decoder->du[i].thu = i * 2 * (0x80000000 / MDC_ND);
        decoder->du[i].xorb = 0;
        decoder->du[i].invert = 0;
        decoder->du[i].shstate = -1;
        decoder->du[i].shcount = 0;
    #ifdef MDC_FOURPOINT
        decoder->du[i].nlstep = i;
    #endif
    }

    decoder->callback = (mdc_decoder_callback_t)0L;

    return decoder;
}

/* */

static void _clearbits(mdc_decoder_t* decoder, mdc_int_t x)
{
    for (mdc_int_t i = 0; i < 112; i++)
        decoder->du[x].bits[i] = 0;
}

#ifdef MDC_ECC

/* */

static void _gofix(mdc_u8_t *data)
{
    int csr[7];
    int fixi, fixj;
    
    int syn = 0;
    for(int i = 0; i < 7; i++)
        csr[i] = 0;

    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j <= 7; j++)
        {
            for (int k = 6; k > 0; k--)
                csr[k] = csr[k-1];

            csr[0] = (data[i] >> j) & 0x01;
            int b = csr[0] + csr[2] + csr[5] + csr[6];
            syn <<= 1;
            if ((b & 0x01) ^ ((data[i+7] >> j) & 0x01)) {
                syn |= 1;
            }

            int ec = 0;
            if (syn & 0x80) ++ec;
            if (syn & 0x20) ++ec;
            if (syn & 0x04) ++ec;
            if (syn & 0x02) ++ec;
            if (ec >= 3) {
                syn ^= 0xa6;
                fixi = i;
                fixj = j-7;
                if(fixj < 0) {
                    --fixi;
                    fixj += 8;
                }

                if (fixi >= 0)
                    data[fixi] ^= 1 << fixj; // flip
            }
        }
    }
}
#endif

/* */

static void _procbits(mdc_decoder_t* decoder, int x)
{
    mdc_int_t lbits[112];
    mdc_int_t lbc = 0;
    mdc_u8_t data[14];
    mdc_u16_t ccrc;
    mdc_u16_t rcrc;

    for (mdc_int_t i = 0; i < 16; i++) {
        for (mdc_int_t j = 0; j < 7; j++) {
            mdc_int_t k = (j * 16) + i;
            lbits[lbc] = decoder->du[x].bits[k];
            ++lbc;
        }
    }

    for (mdc_int_t i = 0; i < 14; i++) {
        data[i] = 0;
        for (mdc_int_t j = 0; j < 8; j++) {
            mdc_int_t k = (i * 8) + j;

            if (lbits[k])
                data[i] |= 1<<j;
        }
    }

#ifdef MDC_ECC
    _gofix(data);
#endif

    ccrc = _docrc(data, 4);
    rcrc = data[5] << 8 | data[4];

    if (ccrc == rcrc) {
        if (decoder->du[x].shstate == 2) {
            decoder->extra0 = data[0];
            decoder->extra1 = data[1];
            decoder->extra2 = data[2];
            decoder->extra3 = data[3];

            for(mdc_int_t k = 0; k < MDC_ND; k++)
                decoder->du[k].shstate = -1;

            decoder->good = 2;
            decoder->indouble = 0;

        }
        else {
            if (!decoder->indouble) {
                decoder->good = 1;
                decoder->op = data[0];
                decoder->arg = data[1];
                decoder->unitID = (data[2] << 8) | data[3];

                switch (data[0])
                {
                /* list of opcode that mean 'double packet' */
                case OP_DOUBLE_PACKET_TYPE1:
                case OP_DOUBLE_PACKET_TYPE2:
                    decoder->good = 0;
                    decoder->indouble = 1;
                    decoder->du[x].shstate = 2;
                    decoder->du[x].shcount = 0;
                    _clearbits(decoder, x);
                    break;
                default:
                    for (mdc_int_t k = 0; k < MDC_ND; k++)
                        decoder->du[k].shstate = -1;	// only in the single-packet case, double keeps rest going
                    break;
                }
            }
            else {
                // any subsequent good decoder allowed to attempt second half
                decoder->du[x].shstate = 2;
                decoder->du[x].shcount = 0;
                _clearbits(decoder, x);
            }
        }
    }
    else {
        decoder->du[x].shstate = -1;
    }

    if (decoder->good) {
        if (decoder->callback) {
            (decoder->callback)((int)decoder->good, (mdc_u8_t)decoder->op, (mdc_u8_t)decoder->arg, (mdc_u16_t)decoder->unitID,
                (mdc_u8_t)decoder->extra0, (mdc_u8_t)decoder->extra1, (mdc_u8_t)decoder->extra2, (mdc_u8_t)decoder->extra3, 
                decoder->callback_context);
            decoder->good = 0;
        }
    }
}

/* */

static int _onebits(mdc_u32_t n)
{
    int i = 0;
    while(n) {
        ++i;
        n &= (n-1);
    }

    return i;
}

/* */

static void _shiftin(mdc_decoder_t* decoder, int x)
{
    int bit = decoder->du[x].xorb;
    int gcount;

    switch (decoder->du[x].shstate)
    {
    case -1:
        decoder->du[x].synchigh = 0;
        decoder->du[x].synclow = 0;
        decoder->du[x].shstate = 0;
        // deliberately fall through
    case 0:
        decoder->du[x].synchigh <<= 1;
        if (decoder->du[x].synclow & 0x80000000)
            decoder->du[x].synchigh |= 1;
        decoder->du[x].synclow <<= 1;
        if (bit)
            decoder->du[x].synclow |= 1;

        gcount = _onebits(0x000000ff & (0x00000007 ^ decoder->du[x].synchigh));
        gcount += _onebits(0x092a446f ^ decoder->du[x].synclow);

        if (gcount <= MDC_GDTHRESH) {
            decoder->du[x].shstate = 1;
            decoder->du[x].shcount = 0;
            _clearbits(decoder, x);
        }
        else if (gcount >= (40 - MDC_GDTHRESH)) {
            decoder->du[x].shstate = 1;
            decoder->du[x].shcount = 0;
            decoder->du[x].xorb = !(decoder->du[x].xorb);
            decoder->du[x].invert = !(decoder->du[x].invert);
            _clearbits(decoder, x);
        }
        return;
    case 1:
    case 2:
        decoder->du[x].bits[decoder->du[x].shcount] = bit;
        decoder->du[x].shcount++;
        if (decoder->du[x].shcount > 111) {
            _procbits(decoder, x);
        }
        return;
    default:
        return;
    }
}

#ifdef MDC_FOURPOINT

/* */

static void _nlproc(mdc_decoder_t* decoder, int x)
{
    mdc_float_t vnow;
    mdc_float_t vpast;

    switch (decoder->du[x].nlstep)
    {
    case 3:
        vnow = ((-0.60 * decoder->du[x].nlevel[3]) + (.97 * decoder->du[x].nlevel[1]));
        vpast = ((-0.60 * decoder->du[x].nlevel[7]) + (.97 * decoder->du[x].nlevel[9]));
        break;
    case 8:
        vnow = ((-0.60 * decoder->du[x].nlevel[8]) + (.97 * decoder->du[x].nlevel[6]));
        vpast = ((-0.60 * decoder->du[x].nlevel[2]) + (.97 * decoder->du[x].nlevel[4]));
        break;
    default:
        return;
    }

    decoder->du[x].xorb = (vnow > vpast) ? 1 : 0;
    if (decoder->du[x].invert)
        decoder->du[x].xorb = !(decoder->du[x].xorb);
    _shiftin(decoder, x);
}

#endif

/* Process incoming samples using an mdc_decoder object. */

int mdc_decoder_process_samples(mdc_decoder_t* decoder, mdc_sample_t* samples, int numSamples)
{
    mdc_sample_t sample;
#ifndef MDC_FIXEDMATH
    mdc_float_t value;
#else
    mdc_int_t value;
#endif

    if (!decoder)
        return -1;

    for (mdc_int_t i = 0; i < numSamples; i++) {
        sample = samples[i];

#ifdef MDC_FIXEDMATH
#if defined(MDC_SAMPLE_FORMAT_U8)
        value = ((mdc_int_t)sample) - 127;
#elif defined(MDC_SAMPLE_FORMAT_U16)
        value = ((mdc_int_t)sample) - 32767;
#elif defined(MDC_SAMPLE_FORMAT_S16)
        value = (mdc_int_t) sample;
#elif defined(MDC_SAMPLE_FORMAT_FLOAT)
#error "fixed-point math not allowed with float sample format"
#else
#error "no known sample format set"
#endif // sample format
#endif // is MDC_FIXEDMATH

#ifndef MDC_FIXEDMATH
#if defined(MDC_SAMPLE_FORMAT_U8)
        value = (((mdc_float_t)sample) - 128.0)/256;
#elif defined(MDC_SAMPLE_FORMAT_U16)
        value = (((mdc_float_t)sample) - 32768.0)/65536.0;
#elif defined(MDC_SAMPLE_FORMAT_S16)
        value = ((mdc_float_t)sample) / 65536.0;
#elif defined(MDC_SAMPLE_FORMAT_FLOAT)
        value = sample;
#else
#error "no known sample format set"
#endif // sample format
#endif // not MDC_FIXEDMATH

#if defined(MDC_ONEPOINT)
        for (mdc_int_t j = 0; j < MDC_ND; j++) {
            mdc_u32_t lthu = decoder->du[j].thu;
            decoder->du[j].thu += decoder->incru;

            // wrapped
            if (decoder->du[j].thu < lthu) {
                if (value > 0)
                    decoder->du[j].xorb = 1;
                else
                    decoder->du[j].xorb = 0;
                if (decoder->du[j].invert)
                    decoder->du[j].xorb = !(decoder->du[j].xorb);
                _shiftin(decoder, j);
            }
        }
#elif defined(MDC_FOURPOINT)
#ifdef MDC_FIXEDMATH
#error "fixed-point math not allowed for fourpoint strategy"
#endif
        for (mdc_int_t j = 0; j < MDC_ND; j++)
        {
            //decoder->du[j].th += (5.0 * decoder->incr);
            mdc_u32_t lthu = decoder->du[j].thu;
            decoder->du[j].thu += 5 * decoder->incru;
        //  if (decoder->du[j].th >= TWOPI)
            // wrapped
            if (decoder->du[j].thu < lthu) {
                decoder->du[j].nlstep++;
                if(decoder->du[j].nlstep > 9)
                    decoder->du[j].nlstep = 0;
                decoder->du[j].nlevel[decoder->du[j].nlstep] = value;

                _nlproc(decoder, j);

                //decoder->du[j].th -= TWOPI;
            }
        }
#else
#error "no decode strategy chosen"
#endif
    }

    if (decoder->good)
        return decoder->good;

    return 0;
}

/* Retrieve last successfully decoded data packet from decoder object. */

int mdc_decoder_get_packet(mdc_decoder_t* decoder, mdc_u8_t* op, mdc_u8_t* arg, mdc_u16_t* unitID)
{
    if (!decoder)
        return -1;

    if (decoder->good != 1)
        return -1;

    if (op)
        *op = decoder->op;
    if (arg)
        *arg = decoder->arg;
    if (unitID)
        *unitID = decoder->unitID;

    decoder->good = 0;
    return 0;
}

/* Retrieve last successfully decoded double-length packet from decoder object. */

int mdc_decoder_get_double_packet(mdc_decoder_t* decoder, mdc_u8_t* op, mdc_u8_t* arg, mdc_u16_t* unitID,
    mdc_u8_t* extra0, mdc_u8_t* extra1, mdc_u8_t* extra2, mdc_u8_t* extra3)
{
    if (!decoder)
        return -1;

    if (decoder->good != 2)
        return -1;

    if (op)
        *op = decoder->op;
    if (arg)
        *arg = decoder->arg;
    if (unitID)
        *unitID = decoder->unitID;

    if (extra0)
        *extra0 = decoder->extra0;
    if (extra1)
        *extra1 = decoder->extra1;
    if (extra2)
        *extra2 = decoder->extra2;
    if (extra3)
        *extra3 = decoder->extra3;

    decoder->good = 0;
    return 0;
}

/* Set a callback function to be called upon successful decode. */

int mdc_decoder_set_callback(mdc_decoder_t* decoder, mdc_decoder_callback_t callbackFunction, void* context)
{
    if (!decoder)
        return -1;

    decoder->callback = callbackFunction;
    decoder->callback_context = context;

    return 0;
}
