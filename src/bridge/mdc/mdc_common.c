// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (c) 2011 Matthew Kaufman
 *
 */
/*-
 * mdc_common.c
 * Author: Matthew Kaufman (matthew@eeph.com)
 *
 * Copyright (c) 2011  Matthew Kaufman  All rights reserved.
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
#include "mdc/mdc_types.h"

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* */

mdc_u16_t _flip(mdc_u16_t crc, mdc_int_t bitnum)
{
    mdc_u16_t crcout, j;

    j = 1;
    crcout = 0;

    for (mdc_u16_t i = 1 << (bitnum - 1); i; i >>= 1) {
        if (crc & i)
             crcout |= j;
        j <<= 1;
    }

    return crcout;
}

/* */

mdc_u16_t _docrc(mdc_u8_t *p, int len)
{
    mdc_int_t bit;
    mdc_u16_t crc = 0x0000;

    for (mdc_int_t i = 0; i < len; i++) {
        mdc_u16_t c = (mdc_u16_t)*p++;
        c = _flip(c, 8);

        for (mdc_int_t j = 0x80; j; j >>= 1) {
            bit = crc & 0x8000;
            crc <<= 1;
            if (c & j)
                bit ^= 0x8000;
            if (bit)
                crc ^= 0x1021;
        }
    }

    crc = _flip(crc, 16);
    crc ^= 0xffff;
    crc &= 0xFFFF;

    return crc;
}
