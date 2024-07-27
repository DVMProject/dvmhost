/*
 * Project 25 IMBE Encoder/Decoder Fixed-Point implementation
 * Developed by Pavel Yazev E-mail: pyazev@gmail.com
 * Version 1.0 (c) Copyright 2009
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * The software is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include "vocoder/imbe/imbe_vocoder.h"

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

imbe_vocoder::imbe_vocoder(void) :
    prev_pitch(0),
    prev_prev_pitch(0),
    prev_e_p(0),
    prev_prev_e_p(0),
    seed(1),
    num_harms_prev1(0),
    num_harms_prev2(0),
    num_harms_prev3(0),
    fund_freq_prev(0),
    th_max(0),
    dc_rmv_mem(0),
    d_gain_adjust(0)
{
    memset(wr_array, 0, sizeof(wr_array));
    memset(wi_array, 0, sizeof(wi_array));
    memset(pitch_est_buf, 0, sizeof(pitch_est_buf));
    memset(pitch_ref_buf, 0, sizeof(pitch_ref_buf));
    memset(pe_lpf_mem, 0, sizeof(pe_lpf_mem));
    memset(fft_buf, 0, sizeof(fft_buf));
    memset(sa_prev1, 0, sizeof(sa_prev1));
    memset(sa_prev2, 0, sizeof(sa_prev2));
    memset(uv_mem, 0, sizeof(uv_mem));
    memset(ph_mem, 0, sizeof(ph_mem));
    memset(vu_dsn_prev, 0, sizeof(vu_dsn_prev));
    memset(sa_prev3, 0, sizeof(sa_prev3));
    memset(v_uv_dsn, 0, sizeof(v_uv_dsn));

    memset(&my_imbe_param, 0, sizeof(IMBE_PARAM));

    decode_init(&my_imbe_param);
    encode_init();
}
