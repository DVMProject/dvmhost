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

#include "vocoder/imbe/typedef.h"
#include "vocoder/imbe/globals.h"
#include "vocoder/imbe/imbe.h"
#include "vocoder/imbe/basic_op.h"
#include "vocoder/imbe/aux_sub.h"
#include "vocoder/imbe/math_sub.h"
#include "vocoder/imbe/tbls.h"
#include "vocoder/imbe/pitch_est.h"
#include "vocoder/imbe/imbe_vocoder.h"

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

/** Number of half-sample pitch candidates from 20 through 122 samples. */
static const Word16 PITCH_CANDIDATE_COUNT = 205;
/** Twice the minimum supported pitch, used to convert candidate indices. */
static const Word16 PITCH_INDEX_OFFSET = 40;
/** Initial prediction-error denominator for the period-20 candidate. */
static const Word16 PITCH_DEN_START_Q1_15 = 0x705C;

/** Maximum three-frame error for accepting an unambiguous pitch submultiple. */
static const Word16 SUBMULTIPLE_ERROR_THRESHOLD_Q4_12 = 0x019A;

/* Returns the lower index of the allowed 20-percent pitch-tracking range. */
static Word16 pitchMinIndex(Word16 index)
{
    const Word16 pitch = index + PITCH_INDEX_OFFSET;
    const Word16 minimum = ((4 * pitch + 4) / 5) - PITCH_INDEX_OFFSET;
    return minimum > 0 ? minimum : 0;
}

/* Returns the upper index of the allowed 20-percent pitch-tracking range. */
static Word16 pitchMaxIndex(Word16 index)
{
    const Word16 pitch = index + PITCH_INDEX_OFFSET;
    const Word16 maximum = ((6 * pitch) / 5) - PITCH_INDEX_OFFSET;
    return maximum < PITCH_CANDIDATE_COUNT ? maximum : PITCH_CANDIDATE_COUNT - 1;
}

// ---------------------------------------------------------------------------
//  Private Class Members
// ---------------------------------------------------------------------------

void imbe_vocoder::pitch_est_init(void)
{
    prev_pitch = prev_prev_pitch = 160; // 100
    prev_e_p = prev_prev_e_p = 0;
}

Word32 imbe_vocoder::autocorr(Word16* sigin, Word16 shift, Word16 scale_shift)
{
    Word32 L_sum;
    Word16 i;

    L_sum = 0;
    for (i = 0; i < PITCH_EST_FRAME - shift; i++)
        L_sum = L_add(L_sum, L_shr(L_mult(sigin[i], sigin[i + shift]), scale_shift));

    return L_sum;
}

void imbe_vocoder::e_p(Word16* sigin, Word16* res_buf)
{
    Word16 i, j, den_part_acc, tmp;
    Word32 L_sum, L_num, L_den, L_e0, L_tmp;
    Word16 sig_wndwed[PITCH_EST_FRAME];
    Word32 corr[261];
    Word16 index_beg, index_step;
    Word16 scale_shift;


    // Windowing input signal s * wi^2
    for (i = 0; i < PITCH_EST_FRAME; i++)
        sig_wndwed[i] = mult_r(sigin[i], wi[i]);

    L_sum = 0;
    for (i = 0; i < PITCH_EST_FRAME; i++)
        L_sum = L_add(L_sum, L_mpy_ls(L_mult(sigin[i], sigin[i]), wi[i]));     // sum(s^2 * wi^2)

    // Check for the overflow
    if (L_sum == MAX_32) {
        // Recalculate with scaling
        L_sum = 0;
        for (i = 0; i < PITCH_EST_FRAME; i++)
            L_sum = L_add(L_sum, L_mpy_ls(L_shr(L_mult(sigin[i], sigin[i]), 5), wi[i]));
        scale_shift = 5;
    }
    else
        scale_shift = 0;

    L_e0 = 0;
    for (i = 0; i < PITCH_EST_FRAME; i++)
        L_e0 = L_add(L_e0, L_shr(L_mult(sig_wndwed[i], sig_wndwed[i]), scale_shift));              // sum(s^2 * wi^4) 

    // Calculate correlation for time shift in range 20...150 with step 0.5
    // For integer shifts
    for (tmp = 20, i = 0; tmp <= 150; tmp++, i += 2)
        corr[i] = autocorr(sig_wndwed, tmp, scale_shift);
    // For intermediate shifts
    for (i = 1; i < 260; i += 2)
        corr[i] = L_shr(L_add(corr[i - 1], corr[i + 1]), 1);


    // variable to calculate 1 - P * sum(wi ^4) in denominator
    den_part_acc = PITCH_DEN_START_Q1_15;

    index_step = PITCH_INDEX_OFFSET; // also represents the pitch in Q15.1 format
    index_beg = 0;
    L_e0 = L_shr(L_e0, 7);         // divide by 64 to compensate wi scaling
    // p = 20...122 by step 0.5
    for (i = 0; i < PITCH_CANDIDATE_COUNT; i++) {

        // Calculate sum( corr ( n * p) )
        L_tmp = 0;
        j = index_beg;
        while (j <= 260) {
            L_tmp = L_add(L_tmp, corr[j]);
            j += index_step;
        }

        L_tmp = L_shr(L_tmp, 6);       // compensate wi scaling 
        L_tmp = L_add(L_tmp, L_e0);    // For n = 0 
        L_tmp = L_tmp * index_step;
        L_num = L_sub(L_sum, L_tmp);

        index_beg++;
        index_step++;

        L_den = L_mpy_ls(L_sum, den_part_acc);

        if (L_num < L_den && L_den != 0) {
            //res_buf[i] = (Word16)((double)L_num/(double)L_den * 4096.);  // Q4.12

            if (L_num <= 0)
                res_buf[i] = 0;
            else {
                tmp = norm_l(L_den);
                tmp = div_s(extract_h(L_shl(L_num, tmp)), extract_h(L_shl(L_den, tmp)));
                res_buf[i] = shr(tmp, 3);  // convert to Q4.12
            }
        }
        else if (L_num >= L_den) {
            res_buf[i] = CNST_1_00_Q4_12;
            //res_buf[i] = (Word16)((double)L_num/(double)L_den * 4096.);  // Q4.12
        }
        else
            res_buf[i] = CNST_1_00_Q4_12;

        den_part_acc = sub(den_part_acc, CNST_0_0031_Q1_15);
    }
}

void imbe_vocoder::pitch_est(IMBE_PARAM* imbe_param, Word16* frames_buf)
{
    Word16 e_p_arr0[PITCH_CANDIDATE_COUNT], e_p_arr1[PITCH_CANDIDATE_COUNT];
    Word16 e_p_arr2[PITCH_CANDIDATE_COUNT], e1p1_e2p2_est_save[PITCH_CANDIDATE_COUNT];
    Word16 min_index, max_index, p, i, p_index;
    UWord16 tmp = 0, p_fp;
    UWord32 UL_tmp;
    Word16 e_p_cur, pb, pf, ceb, s_tmp;
    Word16 cef_est, cef, p0_est, p0, p1, p2, p1_max_index, p2_max_index, e1p1_e2p2_est, e1p1_e2p2;
    Word16 e_p_arr2_min[PITCH_CANDIDATE_COUNT];

    // Calculate E(p) function for current and two future frames
    e_p(&frames_buf[0], e_p_arr0);

    // Look-Back Pitch Tracking
    min_index = pitchMinIndex(prev_pitch);
    max_index = pitchMaxIndex(prev_pitch);

    p = pb = min_index;
    e_p_cur = e_p_arr0[min_index];
    while (++p <= max_index)
        if (e_p_arr0[p] < e_p_cur) {
            e_p_cur = e_p_arr0[p];
            pb = p;
        }
    ceb = add(e_p_cur, add(prev_e_p, prev_prev_e_p));


    if (ceb <= CNST_0_48_Q4_12) {
        prev_prev_pitch = prev_pitch;
        prev_pitch = pb;
        prev_prev_e_p = prev_e_p;
        prev_e_p = e_p_arr0[pb];

        imbe_param->pitch = pb + PITCH_INDEX_OFFSET;  // Result in Q15.1 format
        imbe_param->e_p = prev_e_p;
        return;
    }


    // Look-Ahead Pitch Tracking
    e_p(&frames_buf[FRAME], e_p_arr1);
    e_p(&frames_buf[2 * FRAME], e_p_arr2);

    p0_est = p0 = 0;
    cef_est = e_p_arr0[p0] + e_p_arr1[p0] + e_p_arr2[p0];
    e1p1_e2p2 = 1;

    p1 = 0;
    while (p1 < PITCH_CANDIDATE_COUNT) {
        p2 = pitchMinIndex(p1);
        p2_max_index = pitchMaxIndex(p1);
        s_tmp = e_p_arr2[p1];
        while (p2 <= p2_max_index) {
            if (e_p_arr2[p2] < s_tmp)
                s_tmp = e_p_arr2[p2];
            p2++;
        }
        e_p_arr2_min[p1] = s_tmp;
        p1++;
    }
    while (p0 < PITCH_CANDIDATE_COUNT) {
        e1p1_e2p2_est = e_p_arr1[p0] + e_p_arr2_min[p0];
        p1 = pitchMinIndex(p0);
        p1_max_index = pitchMaxIndex(p0);
        while (p1 <= p1_max_index) {
            if (add(e_p_arr1[p1], e_p_arr2_min[p1]) < e1p1_e2p2_est)
                e1p1_e2p2_est = add(e_p_arr1[p1], e_p_arr2_min[p1]);
            p1++;
        }
        e1p1_e2p2_est_save[p0] = e1p1_e2p2_est;
        cef = add(e_p_arr0[p0], e1p1_e2p2_est);
        if (cef < cef_est) {
            cef_est = cef;
            p0_est = p0;
        }
        p0++;
    }

    pf = p0_est;
    // Sub-multiples analysis
    if (pf >= 44)  // Check if Sub-multiples are possible
    {
        if (pf < 86)
            i = 1;
        else if (pf < 128)
            i = 2;
        else if (pf < 170)
            i = 3;
        else
            i = 4;

        p_fp = (pf + PITCH_INDEX_OFFSET) << 8; // Convert pitch estimation from array index to unsigned Q7.19 format

        while (i--) {
            switch (i) {
            case 0:
                tmp = p_fp >> 1;                     // P_est/2
                break;

            case 1:
                UL_tmp = (UWord32)p_fp * 0x5555;     // P_est/3
                tmp = UL_tmp >> 16;
                break;

            case 2:
                tmp = p_fp >> 2;                     // P_est/4
                break;

            case 3:
                UL_tmp = (UWord32)p_fp * 0x3333;     // P_est/5
                tmp = UL_tmp >> 16;
                break;
            }

            p_index = ((tmp + 0x0080) >> 8) - PITCH_INDEX_OFFSET; // Convert fixed-point pitch to an array index
            cef = add(e_p_arr0[p_index], e1p1_e2p2_est_save[p_index]);

            if (cef <= CNST_0_85_Q4_12 && mult_r(cef, CNST_0_5882_Q1_15) <= cef_est)  // 1/1.7 = 0.5882
            {
                pf = p_index;
                break;
            }

            if (cef <= CNST_0_4_Q4_12 && mult_r(cef, CNST_0_2857_Q1_15) <= cef_est)   // 1/3.5 = 0.2857
            {
                pf = p_index;
                break;
            }

            if (cef <= SUBMULTIPLE_ERROR_THRESHOLD_Q4_12) {
                pf = p_index;
                break;
            }
        }
    }

    cef = add(e_p_arr0[pf], e1p1_e2p2_est_save[pf]);

    if (ceb <= cef)
        p = pb;
    else
        p = pf;

    prev_prev_pitch = prev_pitch;
    prev_pitch = p;
    prev_prev_e_p = prev_e_p;
    prev_e_p = e_p_arr0[p];


    imbe_param->pitch = p + PITCH_INDEX_OFFSET;  // Result in Q15.1 format
    imbe_param->e_p = prev_e_p;
}
