// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - MBE Vocoder
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2019-2021 Doug McLain
 *  Copyright (C) 2021 Bryan Biedenkapp, N2PLL
 *
 */
#define _USE_MATH_DEFINES
#include <math.h>

/*
 * AMBE halfrate encoder - Copyright 2016 Max H. Parke KA1RBI
 * Copyright (C) 2021 by Bryan Biedenkapp N2PLL
 *
 * This file is part of OP25 and part of GNU Radio
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "common/Defines.h"
#include "common/edac/AMBEFEC.h"
#include "common/edac/Golay24128.h"
#include "common/Utils.h"
#include "vocoder/MBEEncoder.h"
#include "vocoder/ambe3600x2450_const.h"
#include "vocoder/ambe3600x2400_const.h"

#include <cassert>

using namespace edac;
using namespace vocoder;

#ifdef _MSC_VER
#pragma warning(disable: 4244)
#endif

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

static const short b0_lookup[] = {
    0, 0, 0, 1, 1, 2, 2, 2,
    3, 3, 4, 4, 4, 5, 5, 5,
    6, 6, 7, 7, 7, 8, 8, 8,
    9, 9, 9, 10, 10, 11, 11, 11,
    12, 12, 12, 13, 13, 13, 14, 14,
    14, 15, 15, 15, 16, 16, 16, 17,
    17, 17, 17, 18, 18, 18, 19, 19,
    19, 20, 20, 20, 21, 21, 21, 21,
    22, 22, 22, 23, 23, 23, 24, 24,
    24, 24, 25, 25, 25, 25, 26, 26,
    26, 27, 27, 27, 27, 28, 28, 28,
    29, 29, 29, 29, 30, 30, 30, 30,
    31, 31, 31, 31, 31, 32, 32, 32,
    32, 33, 33, 33, 33, 34, 34, 34,
    34, 35, 35, 35, 35, 36, 36, 36,
    36, 37, 37, 37, 37, 38, 38, 38,
    38, 38, 39, 39, 39, 39, 40, 40,
    40, 40, 40, 41, 41, 41, 41, 42,
    42, 42, 42, 42, 43, 43, 43, 43,
    43, 44, 44, 44, 44, 45, 45, 45,
    45, 45, 46, 46, 46, 46, 46, 47,
    47, 47, 47, 47, 48, 48, 48, 48,
    48, 49, 49, 49, 49, 49, 49, 50,
    50, 50, 50, 50, 51, 51, 51, 51,
    51, 52, 52, 52, 52, 52, 52, 53,
    53, 53, 53, 53, 54, 54, 54, 54,
    54, 54, 55, 55, 55, 55, 55, 56,
    56, 56, 56, 56, 56, 57, 57, 57,
    57, 57, 57, 58, 58, 58, 58, 58,
    58, 59, 59, 59, 59, 59, 59, 60,
    60, 60, 60, 60, 60, 61, 61, 61,
    61, 61, 61, 62, 62, 62, 62, 62,
    62, 63, 63, 63, 63, 63, 63, 63,
    64, 64, 64, 64, 64, 64, 65, 65,
    65, 65, 65, 65, 65, 66, 66, 66,
    66, 66, 66, 67, 67, 67, 67, 67,
    67, 67, 68, 68, 68, 68, 68, 68,
    68, 69, 69, 69, 69, 69, 69, 69,
    70, 70, 70, 70, 70, 70, 70, 71,
    71, 71, 71, 71, 71, 71, 72, 72,
    72, 72, 72, 72, 72, 73, 73, 73,
    73, 73, 73, 73, 73, 74, 74, 74,
    74, 74, 74, 74, 75, 75, 75, 75,
    75, 75, 75, 75, 76, 76, 76, 76,
    76, 76, 76, 76, 77, 77, 77, 77,
    77, 77, 77, 77, 77, 78, 78, 78,
    78, 78, 78, 78, 78, 79, 79, 79,
    79, 79, 79, 79, 79, 80, 80, 80,
    80, 80, 80, 80, 80, 81, 81, 81,
    81, 81, 81, 81, 81, 81, 82, 82,
    82, 82, 82, 82, 82, 82, 83, 83,
    83, 83, 83, 83, 83, 83, 83, 84,
    84, 84, 84, 84, 84, 84, 84, 84,
    85, 85, 85, 85, 85, 85, 85, 85,
    85, 86, 86, 86, 86, 86, 86, 86,
    86, 86, 87, 87, 87, 87, 87, 87,
    87, 87, 87, 88, 88, 88, 88, 88,
    88, 88, 88, 88, 89, 89, 89, 89,
    89, 89, 89, 89, 89, 89, 90, 90,
    90, 90, 90, 90, 90, 90, 90, 90,
    91, 91, 91, 91, 91, 91, 91, 91,
    91, 92, 92, 92, 92, 92, 92, 92,
    92, 92, 92, 93, 93, 93, 93, 93,
    93, 93, 93, 93, 93, 94, 94, 94,
    94, 94, 94, 94, 94, 94, 94, 94,
    95, 95, 95, 95, 95, 95, 95, 95,
    95, 95, 96, 96, 96, 96, 96, 96,
    96, 96, 96, 96, 96, 97, 97, 97,
    97, 97, 97, 97, 97, 97, 97, 98,
    98, 98, 98, 98, 98, 98, 98, 98,
    98, 98, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100,
    100, 101, 101, 101, 101, 101, 101, 101,
    101, 101, 101, 101, 102, 102, 102, 102,
    102, 102, 102, 102, 102, 102, 102, 102,
    103, 103, 103, 103, 103, 103, 103, 103,
    103, 103, 103, 103, 104, 104, 104, 104,
    104, 104, 104, 104, 104, 104, 104, 104,
    105, 105, 105, 105, 105, 105, 105, 105,
    105, 105, 105, 105, 106, 106, 106, 106,
    106, 106, 106, 106, 106, 106, 106, 106,
    107, 107, 107, 107, 107, 107, 107, 107,
    107, 107, 107, 107, 107, 108, 108, 108,
    108, 108, 108, 108, 108, 108, 108, 108,
    108, 109, 109, 109, 109, 109, 109, 109,
    109, 109, 109, 109, 109, 109, 110, 110,
    110, 110, 110, 110, 110, 110, 110, 110,
    110, 110, 110, 111, 111, 111, 111, 111,
    111, 111, 111, 111, 111, 111, 111, 111,
    112, 112, 112, 112, 112, 112, 112, 112,
    112, 112, 112, 112, 112, 112, 113, 113,
    113, 113, 113, 113, 113, 113, 113, 113,
    113, 113, 113, 113, 114, 114, 114, 114,
    114, 114, 114, 114, 114, 114, 114, 114,
    114, 115, 115, 115, 115, 115, 115, 115,
    115, 115, 115, 115, 115, 115, 115, 116,
    116, 116, 116, 116, 116, 116, 116, 116,
    116, 116, 116, 116, 116, 116, 117, 117,
    117, 117, 117, 117, 117, 117, 117, 117,
    117, 117, 117, 117, 118, 118, 118, 118,
    118, 118, 118, 118, 118, 118, 118, 118,
    118, 118, 118, 119, 119, 119, 119, 119,
    119, 119, 119
};

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/**
 * @brief 
 * @param[in] imbe_param 
 * @param b 
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param gainAdjust Gain adjustment.
 */
static void encodeAMBE(const IMBE_PARAM* imbe_param, int b[], mbe_parms* cur_mp, mbe_parms* prev_mp, float gainAdjust)
{
    static const float SQRT_2 = sqrtf(2.0);
    static const int b0_lmax = sizeof(b0_lookup) / sizeof(b0_lookup[0]);
    // int b[9];

    // ref_pitch is Q8_8 in range 19.875 - 123.125
    int b0_i = (imbe_param->ref_pitch >> 5) - 159;
    if (b0_i < 0 || b0_i > b0_lmax) {
        fprintf(stderr, "encode error b0_i %d\n", b0_i);
        return;
    }

    b[0] = b0_lookup[b0_i];
    int L = (int)AmbeLtable[b[0]];

    // adjust b0 until L agrees
    while (L != imbe_param->num_harms) {
        if (L < imbe_param->num_harms)
            b0_i++;
        else if (L > imbe_param->num_harms)
            b0_i--;
        if (b0_i < 0 || b0_i > b0_lmax) {
            fprintf(stderr, "encode error2 b0_i %d\n", b0_i);
            return;
        }
        b[0] = b0_lookup[b0_i];
        L = (int)AmbeLtable[b[0]];
    }

    float m_float2[NUM_HARMS_MAX];
    for (int l = 1; l <= L; l++) {
        m_float2[l - 1] = (float)imbe_param->sa[l - 1];
        m_float2[l - 1] = m_float2[l - 1] * m_float2[l - 1];
    }

    float en_min = 0;
    b[1] = 0;
    int vuv_max = 17;
    for (int n = 0; n < vuv_max; n++) {
        float En = 0;
        for (int l = 1; l <= L; l++) {
            int jl = (int)((float)l * (float)16.0 * AmbeW0table[b[0]]);
            int kl = 12;
            if (l <= 36)
                kl = (l + 2) / 3;

            if (imbe_param->v_uv_dsn[(kl - 1) * 3] != AmbeVuv[n][jl])
                En += m_float2[l - 1];
        }

        if (n == 0)
            en_min = En;
        else if (En < en_min) {
            b[1] = n;
            en_min = En;
        }
    }

    // log spectral amplitudes
    float num_harms_f = (float)imbe_param->num_harms;
    float log_l_2 = 0.5 * log2f(num_harms_f);	// fixme: table lookup
    float log_l_w0 = 0.5 * log2f(num_harms_f * AmbeW0table[b[0]] * 2.0 * M_PI) + 2.289;
    float lsa[NUM_HARMS_MAX];
    float lsa_sum = 0.0;

    for (int i1 = 0; i1 < imbe_param->num_harms; i1++) {
        float sa = (float)imbe_param->sa[i1];
        if (sa < 1) sa = 1.0;
        if (imbe_param->v_uv_dsn[i1])
            lsa[i1] = log_l_2 + log2f(sa);
        else
            lsa[i1] = log_l_w0 + log2f(sa);
        lsa_sum += lsa[i1];
    }

    float gain = lsa_sum / num_harms_f;
    float diff_gain = gain - 0.5 * prev_mp->gamma;

    diff_gain -= gainAdjust;

    float error;
    int error_index;
    int max_dg = 32;
    for (int i1 = 0; i1 < max_dg; i1++) {
        float diff = fabsf(diff_gain - AmbeDg[i1]);
        if ((i1 == 0) || (diff < error)) {
            error = diff;
            error_index = i1;
        }
    }

    b[2] = error_index;

    // prediction residuals
    float l_prev_l = (float)(prev_mp->L) / num_harms_f;
    float tmp_s = 0.0;
    prev_mp->log2Ml[0] = prev_mp->log2Ml[1];
    for (int i1 = 0; i1 < imbe_param->num_harms; i1++) {
        float kl = l_prev_l * (float)(i1 + 1);
        int kl_floor = (int)kl;
        float kl_frac = kl - kl_floor;
        tmp_s += (1.0 - kl_frac) * prev_mp->log2Ml[kl_floor + 0] + kl_frac * prev_mp->log2Ml[kl_floor + 1 + 0];
    }

    float T[NUM_HARMS_MAX];
    for (int i1 = 0; i1 < imbe_param->num_harms; i1++) {
        float kl = l_prev_l * (float)(i1 + 1);
        int kl_floor = (int)kl;
        float kl_frac = kl - kl_floor;
        T[i1] = lsa[i1] - 0.65 * (1.0 - kl_frac) * prev_mp->log2Ml[kl_floor + 0]	\
            - 0.65 * kl_frac * prev_mp->log2Ml[kl_floor + 1 + 0];
    }

    // DCT
    const int* J = AmbeLmprbl[imbe_param->num_harms];
    float* c[4];
    int acc = 0;
    for (int i = 0; i < 4; i++) {
        c[i] = &T[acc];
        acc += J[i];
    }

    float C[4][17];
    for (int i = 1; i <= 4; i++) {
        for (int k = 1; k <= J[i - 1]; k++) {
            float s = 0.0;
            for (int j = 1; j <= J[i - 1]; j++) {
                //fixme: lut?
                s += (c[i - 1][j - 1] * cosf((M_PI * (((float)k) - 1.0) * (((float)j) - 0.5)) / (float)J[i - 1]));
            }
            C[i - 1][k - 1] = s / (float)J[i - 1];
        }
    }

    float R[8];
    R[0] = C[0][0] + SQRT_2 * C[0][1];
    R[1] = C[0][0] - SQRT_2 * C[0][1];
    R[2] = C[1][0] + SQRT_2 * C[1][1];
    R[3] = C[1][0] - SQRT_2 * C[1][1];
    R[4] = C[2][0] + SQRT_2 * C[2][1];
    R[5] = C[2][0] - SQRT_2 * C[2][1];
    R[6] = C[3][0] + SQRT_2 * C[3][1];
    R[7] = C[3][0] - SQRT_2 * C[3][1];

    // encode PRBA
    float G[8];
    for (int m = 1; m <= 8; m++) {
        G[m - 1] = 0.0;
        for (int i = 1; i <= 8; i++) {
            //fixme: lut?
            G[m - 1] += (R[i - 1] * cosf((M_PI * (((float)m) - 1.0) * (((float)i) - 0.5)) / 8.0));
        }
        G[m - 1] /= 8.0;
    }

    for (int i = 0; i < 512; i++) {
        float err = 0.0;
        float diff;

        diff = G[1] - AmbePRBA24[i][0];
        err += (diff * diff);
        diff = G[2] - AmbePRBA24[i][1];
        err += (diff * diff);
        diff = G[3] - AmbePRBA24[i][2];
        err += (diff * diff);

        if (i == 0 || err < error) {
            error = err;
            error_index = i;
        }
    }

    b[3] = error_index;

    // PRBA58
    for (int i = 0; i < 128; i++) {
        float err = 0.0;
        float diff;

        diff = G[4] - AmbePRBA58[i][0];
        err += (diff * diff);
        diff = G[5] - AmbePRBA58[i][1];
        err += (diff * diff);
        diff = G[6] - AmbePRBA58[i][2];
        err += (diff * diff);
        diff = G[7] - AmbePRBA58[i][3];
        err += (diff * diff);

        if (i == 0 || err < error) {
            error = err;
            error_index = i;
        }
    }

    b[4] = error_index;

    // higher order coeffs b5
    int ii = 1;
    if (J[ii - 1] <= 2) {
        b[4 + ii] = 0.0;
    }
    else {
        int max_5 = 32;
        for (int n = 0; n < max_5; n++) {
            float err = 0.0;
            float diff;
            for (int j = 1; j <= J[ii - 1] - 2 && j <= 4; j++) {
                diff = AmbeHOCb5[n][j - 1] - C[ii - 1][j + 2 - 1];
                err += (diff * diff);
            }
            if (n == 0 || err < error) {
                error = err;
                error_index = n;
            }
        }
        b[4 + ii] = error_index;
    }

    // higher order coeffs b6
    ii = 2;
    if (J[ii - 1] <= 2) {
        b[4 + ii] = 0.0;
    }
    else {
        for (int n = 0; n < 16; n++) {
            float err = 0.0;
            float diff;
            for (int j = 1; j <= J[ii - 1] - 2 && j <= 4; j++) {
                diff = AmbeHOCb6[n][j - 1] - C[ii - 1][j + 2 - 1];
                err += (diff * diff);
            }
            if (n == 0 || err < error) {
                error = err;
                error_index = n;
            }
        }
        b[4 + ii] = error_index;
    }

    // higher order coeffs b7
    ii = 3;
    if (J[ii - 1] <= 2) {
        b[4 + ii] = 0.0;
    }
    else {
        for (int n = 0; n < 16; n++) {
            float err = 0.0;
            float diff;
            for (int j = 1; j <= J[ii - 1] - 2 && j <= 4; j++) {
                diff = AmbeHOCb7[n][j - 1] - C[ii - 1][j + 2 - 1];
                err += (diff * diff);
            }
            if (n == 0 || err < error) {
                error = err;
                error_index = n;
            }
        }
        b[4 + ii] = error_index;
    }

    // higher order coeffs b8
    ii = 4;
    if (J[ii - 1] <= 2) {
        b[4 + ii] = 0.0;
    }
    else {
        int max_8 = 8;
        for (int n = 0; n < max_8; n++) {
            float err = 0.0;
            float diff;
            for (int j = 1; j <= J[ii - 1] - 2 && j <= 4; j++) {
                diff = AmbeHOCb8[n][j - 1] - C[ii - 1][j + 2 - 1];
                err += (diff * diff);
            }
            if (n == 0 || err < error) {
                error = err;
                error_index = n;
            }
        }
        b[4 + ii] = error_index;
    }

    mbe_dequantizeAmbe2250Parms(cur_mp, prev_mp, b);
    mbe_moveMbeParms(cur_mp, prev_mp);
}

/**
 * @brief 
 * @param bits 
 * @param b 
 */
static void encode49bit(uint8_t bits[49], const int b[9])
{
    bits[0] = (b[0] >> 6) & 1;
    bits[1] = (b[0] >> 5) & 1;
    bits[2] = (b[0] >> 4) & 1;
    bits[3] = (b[0] >> 3) & 1;
    bits[4] = (b[1] >> 4) & 1;
    bits[5] = (b[1] >> 3) & 1;
    bits[6] = (b[1] >> 2) & 1;
    bits[7] = (b[1] >> 1) & 1;
    bits[8] = (b[2] >> 4) & 1;
    bits[9] = (b[2] >> 3) & 1;
    bits[10] = (b[2] >> 2) & 1;
    bits[11] = (b[2] >> 1) & 1;
    bits[12] = (b[3] >> 8) & 1;
    bits[13] = (b[3] >> 7) & 1;
    bits[14] = (b[3] >> 6) & 1;
    bits[15] = (b[3] >> 5) & 1;
    bits[16] = (b[3] >> 4) & 1;
    bits[17] = (b[3] >> 3) & 1;
    bits[18] = (b[3] >> 2) & 1;
    bits[19] = (b[3] >> 1) & 1;
    bits[20] = (b[4] >> 6) & 1;
    bits[21] = (b[4] >> 5) & 1;
    bits[22] = (b[4] >> 4) & 1;
    bits[23] = (b[4] >> 3) & 1;
    bits[24] = (b[5] >> 4) & 1;
    bits[25] = (b[5] >> 3) & 1;
    bits[26] = (b[5] >> 2) & 1;
    bits[27] = (b[5] >> 1) & 1;
    bits[28] = (b[6] >> 3) & 1;
    bits[29] = (b[6] >> 2) & 1;
    bits[30] = (b[6] >> 1) & 1;
    bits[31] = (b[7] >> 3) & 1;
    bits[32] = (b[7] >> 2) & 1;
    bits[33] = (b[7] >> 1) & 1;
    bits[34] = (b[8] >> 2) & 1;
    bits[35] = b[1] & 1;
    bits[36] = b[2] & 1;
    bits[37] = (b[0] >> 2) & 1;
    bits[38] = (b[0] >> 1) & 1;
    bits[39] = b[0] & 1;
    bits[40] = b[3] & 1;
    bits[41] = (b[4] >> 2) & 1;
    bits[42] = (b[4] >> 1) & 1;
    bits[43] = b[4] & 1;
    bits[44] = b[5] & 1;
    bits[45] = b[6] & 1;
    bits[46] = b[7] & 1;
    bits[47] = (b[8] >> 1) & 1;
    bits[48] = b[8] & 1;
}

/**
 * @brief 
 * @param[in] in 
 * @param out 
 */
static void encodeDmrAMBE(const uint8_t* in, uint8_t* out)
{
    unsigned int aOrig = 0U;
    unsigned int bOrig = 0U;
    unsigned int cOrig = 0U;

    unsigned int MASK = 0x000800U;
    for (unsigned int i = 0U; i < 12U; i++, MASK >>= 1) {
        unsigned int n1 = i;
        unsigned int n2 = i + 12U;
        if (READ_BIT(in, n1))
            aOrig |= MASK;
        if (READ_BIT(in, n2))
            bOrig |= MASK;
    }

    MASK = 0x1000000U;
    for (unsigned int i = 0U; i < 25U; i++, MASK >>= 1) {
        unsigned int n = i + 24U;
        if (READ_BIT(in, n))
            cOrig |= MASK;
    }

    unsigned int a = Golay24128::encode24128(aOrig);

    // The PRNG
    unsigned int p = PRNG_TABLE[aOrig] >> 1;

    unsigned int b = Golay24128::encode23127(bOrig) >> 1;
    b ^= p;

    MASK = 0x800000U;
    for (unsigned int i = 0U; i < 24U; i++, MASK >>= 1) {
        unsigned int aPos = AMBE_A_TABLE[i];
        WRITE_BIT(out, aPos, a & MASK);
    }

    MASK = 0x400000U;
    for (unsigned int i = 0U; i < 23U; i++, MASK >>= 1) {
        unsigned int bPos = AMBE_B_TABLE[i];
        WRITE_BIT(out, bPos, b & MASK);
    }

    MASK = 0x1000000U;
    for (unsigned int i = 0U; i < 25U; i++, MASK >>= 1) {
        unsigned int cPos = AMBE_C_TABLE[i];
        WRITE_BIT(out, cPos, cOrig & MASK);
    }
}

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBEEncoder class. */
MBEEncoder::MBEEncoder(MBE_ENCODER_MODE mode) :
    m_mbeMode(mode),
    m_gainAdjust(0.0f)
{
    mbe_parms enh_mp;
    mbe_initMbeParms(&m_curMBEParms, &m_prevMBEParms, &enh_mp);
}

/* Encodes the given MBE bits to deinterleaved MBE bits using the decoder mode. */

void MBEEncoder::encodeBits(uint8_t* bits, uint8_t* codeword)
{
    assert(bits != nullptr);
    assert(codeword != nullptr);

    int32_t errs = 0;
    float samples[160U];
    ::memset(samples, 0x00U, 160U * sizeof(float));

    switch (m_mbeMode)
    {
    case ENCODE_DMR_AMBE:
    {
        // build 49-bit AMBE bytes
        uint8_t rawAmbe[9U];
        ::memset(rawAmbe, 0x00U, 9U);

        for (int i = 0; i < 9; ++i) {
            for (int j = 0; j < 8; ++j) {
                rawAmbe[i] |= (bits[(i * 8) + j] << (7 - j));
            }
        }

        // build DMR AMBE bytes
        uint8_t dmrAMBE[9U];
        ::memset(dmrAMBE, 0x00U, 9U);

        encodeDmrAMBE(rawAmbe, dmrAMBE);
        ::memcpy(codeword, dmrAMBE, 9U);
    }
    break;

    case ENCODE_88BIT_IMBE:
    {
        uint8_t rawImbe[11U];
        ::memset(rawImbe, 0x00U, 11U);

        for (int i = 0; i < 11; ++i) {
            for (int j = 0; j < 8; ++j) {
                rawImbe[i] |= (bits[(i * 8) + j] << (7 - j));
            }
        }
    
        ::memcpy(codeword, rawImbe, 11U);
    }
    break;
    }
}

/* Encodes the given PCM samples using the encoder mode to MBE codewords. */

void MBEEncoder::encode(int16_t* samples, uint8_t* codeword)
{
    assert(samples != nullptr);
    assert(codeword != nullptr);

    int16_t frame_vector[8];	// result ignored

    // first do speech analysis to generate mbe model parameters
    m_vocoder.imbe_encode(frame_vector, samples);
    if (m_mbeMode == ENCODE_88BIT_IMBE) {
        if (m_gainAdjust >= 1.0f) {
            m_vocoder.set_gain_adjust(m_gainAdjust);
        }

        uint32_t offset = 0U;
        int16_t mask = 0x0800;

        for (uint32_t i = 0U; i < 12U; i++, mask >>= 1, offset++)
            WRITE_BIT(codeword, offset, (frame_vector[0U] & mask) != 0);

        mask = 0x0800;
        for (uint32_t i = 0U; i < 12U; i++, mask >>= 1, offset++)
            WRITE_BIT(codeword, offset, (frame_vector[1U] & mask) != 0);

        mask = 0x0800;
        for (uint32_t i = 0U; i < 12U; i++, mask >>= 1, offset++)
            WRITE_BIT(codeword, offset, (frame_vector[2U] & mask) != 0);

        mask = 0x0800;
        for (uint32_t i = 0U; i < 12U; i++, mask >>= 1, offset++)
            WRITE_BIT(codeword, offset, (frame_vector[3U] & mask) != 0);

        mask = 0x0400;
        for (uint32_t i = 0U; i < 11U; i++, mask >>= 1, offset++)
            WRITE_BIT(codeword, offset, (frame_vector[4U] & mask) != 0);

        mask = 0x0400;
        for (uint32_t i = 0U; i < 11U; i++, mask >>= 1, offset++)
            WRITE_BIT(codeword, offset, (frame_vector[5U] & mask) != 0);

        mask = 0x0400;
        for (uint32_t i = 0U; i < 11U; i++, mask >>= 1, offset++)
            WRITE_BIT(codeword, offset, (frame_vector[6U] & mask) != 0);

        mask = 0x0040;
        for (uint32_t i = 0U; i < 7U; i++, mask >>= 1, offset++)
            WRITE_BIT(codeword, offset, (frame_vector[7U] & mask) != 0);
    }
    else {
        int b[9];

        // halfrate audio encoding - output rate is 2450 (49 bits)
        encodeAMBE(m_vocoder.param(), b, &m_curMBEParms, &m_prevMBEParms, m_gainAdjust);

        uint8_t bits[49U];
        ::memset(bits, 0x00U, 49U);

        encode49bit(bits, b);

        // build 49-bit AMBE bytes
        uint8_t rawAmbe[9U];
        ::memset(rawAmbe, 0x00U, 9U);

        for (int i = 0; i < 9; ++i) {
            for (int j = 0; j < 8; ++j) {
                rawAmbe[i] |= (bits[(i * 8) + j] << (7 - j));
            }
        }

        // build DMR AMBE bytes
        uint8_t dmrAMBE[9U];
        ::memset(dmrAMBE, 0x00U, 9U);

        encodeDmrAMBE(rawAmbe, dmrAMBE);
        ::memcpy(codeword, dmrAMBE, 9U);
    }
}
