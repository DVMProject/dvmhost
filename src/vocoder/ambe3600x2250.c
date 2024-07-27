// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - MBE Vocoder
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 */

/*
 * Copyright (C) 2010 mbelib Author
 * GPG Key ID: 0xEA5EFE2C (9E7A 5527 9CDC EBF7 BF1B  D772 4F98 E863 EA5E FE2C)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "vocoder/mbe.h"

#ifdef _MSC_VER
#pragma warning(disable: 4244)
#pragma warning(disable: 4305)
#endif
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

// ---------------------------------------------------------------------------
//  Externs
// ---------------------------------------------------------------------------

/*
** these are all declared by include in ambe3600x2450.c
*/
extern const float AmbeW0table[120];
extern const float AmbeLtable[120];
extern const int AmbeVuv[32][8];
extern const int AmbeLmprbl[57][4];
extern const float AmbeDg[32];
extern const float AmbePRBA24[512][3];
extern const float AmbePRBA58[128][4];
extern const float AmbeHOCb5[32][4];
extern const float AmbeHOCb6[16][4];
extern const float AmbeHOCb7[16][4];
extern const float AmbeHOCb8[8][4];

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------

/* */

int mbe_dequantizeAmbe2250Parms(mbe_parms* cur_mp, mbe_parms* prev_mp, const int* b)
{
    int ji, i, j, k, l, L, m, am, ak;
    int intkl[57];
    int b0, b1, b2, b3, b4, b5, b6, b7, b8;
    float f0, Cik[5][18], flokl[57], deltal[57];
    float Sum42, Sum43, Tl[57], Gm[9], Ri[9], sum, c1, c2;
    //char tmpstr[13];
    int silence;
    int Ji[5], jl;
    float deltaGamma, BigGamma;
    float unvc, rconst;

    b0 = b[0];
    b1 = b[1];
    b2 = b[2];
    b3 = b[3];
    b4 = b[4];
    b5 = b[5];
    b6 = b[6];
    b7 = b[7];
    b8 = b[8];

    silence = 0;

    // copy repeat from prev_mp
    cur_mp->repeat = prev_mp->repeat;

    if ((b0 >= 120) && (b0 <= 123)) {
#ifdef AMBE_DEBUG
        fprintf(stderr, "MBE: AMBE: Erasure Frame");
#endif
        return (2);
    }
    else if ((b0 == 124) || (b0 == 125)) {
#ifdef AMBE_DEBUG
        fprintf(stderr, "MBE: AMBE: Silence Frame");
#endif
        silence = 1;
        cur_mp->w0 = ((float)2 * M_PI) / (float)32;
        f0 = (float)1 / (float)32;
        L = 14;
        cur_mp->L = 14;
        for (l = 1; l <= L; l++)
        {
            cur_mp->Vl[l] = 0;
        }
    }
    else if ((b0 == 126) || (b0 == 127)) {
#ifdef AMBE_DEBUG
        fprintf(stderr, "MBE: AMBE: Tone Frame");
#endif
        return (3);
    }

    if (silence == 0) {
        // w0 from specification document
        f0 = AmbeW0table[b0];
        cur_mp->w0 = f0 * (float)2 * M_PI;
        // w0 from patent filings
        //f0 = powf (2, ((float) b0 + (float) 195.626) / -(float) 45.368);
        //cur_mp->w0 = f0 * (float) 2 *M_PI;
    }

    unvc = (float)0.2046 / sqrtf(cur_mp->w0);
    //unvc = (float) 1;
    //unvc = (float) 0.2046 / sqrtf (f0);

    // decode L
    if (silence == 0) {
        // L from specification document 
        // lookup L in tabl3
        L = AmbeLtable[b0];
        // L formula form patent filings
        //L=(int)((float)0.4627 / f0);
        cur_mp->L = L;
    }

    // decode V/UV parameters
    for (l = 1; l <= L; l++) {
        // jl from specification document
        jl = (int)((float)l * (float)16.0 * f0);
        // jl from patent filings?
        //jl = (int)(((float)l * (float)16.0 * f0) + 0.25);

        if (silence == 0) {
            cur_mp->Vl[l] = AmbeVuv[b1][jl];
        }
#ifdef AMBE_DEBUG
        fprintf(stderr, "MBE: AMBE: jl[%i]:%i Vl[%i]:%i", l, jl, l, cur_mp->Vl[l]);
#endif
    }

#ifdef AMBE_DEBUG
    fprintf(stderr, "MBE: AMBE: b0:%i w0:%f L:%i b1:%i", b0, cur_mp->w0, L, b1);
#endif

    deltaGamma = AmbeDg[b2];
    cur_mp->gamma = deltaGamma + ((float)0.5 * prev_mp->gamma);

#ifdef AMBE_DEBUG
    fprintf(stderr, "MBE: AMBE: b2: %i, deltaGamma: %f gamma: %f gamma-1: %f", b2, deltaGamma, cur_mp->gamma, prev_mp->gamma);
#endif

    // decode PRBA vectors
    Gm[1] = 0;
    Gm[2] = AmbePRBA24[b3][0];
    Gm[3] = AmbePRBA24[b3][1];
    Gm[4] = AmbePRBA24[b3][2];

    Gm[5] = AmbePRBA58[b4][0];
    Gm[6] = AmbePRBA58[b4][1];
    Gm[7] = AmbePRBA58[b4][2];
    Gm[8] = AmbePRBA58[b4][3];

#ifdef AMBE_DEBUG
    fprintf(stderr, "MBE: AMBE: b3: %i Gm[2]: %f Gm[3]: %f Gm[4]: %f b4: %i Gm[5]: %f Gm[6]: %f Gm[7]: %f Gm[8]: %f", b3, Gm[2], Gm[3], Gm[4], b4, Gm[5], Gm[6], Gm[7], Gm[8]);
#endif

    // compute Ri
    for (i = 1; i <= 8; i++) {
        sum = 0;
        for (m = 1; m <= 8; m++) {
            if (m == 1) {
                am = 1;
            }
            else {
                am = 2;
            }
            sum = sum + ((float)am * Gm[m] * cosf((M_PI * (float)(m - 1) * ((float)i - (float)0.5)) / (float)8));
        }

        Ri[i] = sum;
#ifdef AMBE_DEBUG
        fprintf(stderr, "MBE: AMBE: R%i: %f", i, Ri[i]);
#endif
    }

    // generate first to elements of each Ci,k block from PRBA vector
    rconst = ((float)1 / ((float)2 * M_SQRT2));
    Cik[1][1] = (float)0.5 * (Ri[1] + Ri[2]);
    Cik[1][2] = rconst * (Ri[1] - Ri[2]);
    Cik[2][1] = (float)0.5 * (Ri[3] + Ri[4]);
    Cik[2][2] = rconst * (Ri[3] - Ri[4]);
    Cik[3][1] = (float)0.5 * (Ri[5] + Ri[6]);
    Cik[3][2] = rconst * (Ri[5] - Ri[6]);
    Cik[4][1] = (float)0.5 * (Ri[7] + Ri[8]);
    Cik[4][2] = rconst * (Ri[7] - Ri[8]);

    // decode HOC

    // lookup Ji
    Ji[1] = AmbeLmprbl[L][0];
    Ji[2] = AmbeLmprbl[L][1];
    Ji[3] = AmbeLmprbl[L][2];
    Ji[4] = AmbeLmprbl[L][3];

#ifdef AMBE_DEBUG
    fprintf(stderr, "MBE: AMBE: Ji[1]: %i Ji[2]: %i Ji[3]: %i Ji[4]: %i", Ji[1], Ji[2], Ji[3], Ji[4]);
    fprintf(stderr, "MBE: AMBE: b5: %i b6: %i b7: %i b8: %i", b5, b6, b7, b8);
#endif

    // Load Ci,k with the values from the HOC tables
    // there appear to be a couple typos in eq. 37 so we will just do what makes sense
    // (3 <= k <= Ji and k<=6)
    for (k = 3; k <= Ji[1]; k++) {
        if (k > 6) {
            Cik[1][k] = 0;
        }
        else {
            Cik[1][k] = AmbeHOCb5[b5][k - 3];
#ifdef AMBE_DEBUG
            fprintf(stderr, "MBE: AMBE: C1,%i: %f", k, Cik[1][k]);
#endif
        }
    }

    for (k = 3; k <= Ji[2]; k++) {
        if (k > 6) {
            Cik[2][k] = 0;
        }
        else {
            Cik[2][k] = AmbeHOCb6[b6][k - 3];
#ifdef AMBE_DEBUG
            fprintf(stderr, "MBE: AMBE: C2,%i: %f", k, Cik[2][k]);
#endif
        }
    }

    for (k = 3; k <= Ji[3]; k++) {
        if (k > 6) {
            Cik[3][k] = 0;
        }
        else {
            Cik[3][k] = AmbeHOCb7[b7][k - 3];
#ifdef AMBE_DEBUG
            fprintf(stderr, "MBE: AMBE: C3,%i: %f", k, Cik[3][k]);
#endif
        }
    }

    for (k = 3; k <= Ji[4]; k++) {
        if (k > 6) {
            Cik[4][k] = 0;
        }
        else {
            Cik[4][k] = AmbeHOCb8[b8][k - 3];
#ifdef AMBE_DEBUG
            fprintf(stderr, "MBE: AMBE: C4,%i: %f", k, Cik[4][k]);
#endif
        }
    }

    // inverse DCT each Ci,k to give ci,j (Tl)
    l = 1;
    for (i = 1; i <= 4; i++) {
        ji = Ji[i];
        for (j = 1; j <= ji; j++) {
            sum = 0;
            for (k = 1; k <= ji; k++) {
                if (k == 1) {
                    ak = 1;
                }
                else {
                    ak = 2;
                }
#ifdef AMBE_DEBUG
                fprintf(stderr, "MBE: AMBE: %i Cik[%i][%i]: %f", j, i, k, Cik[i][k]);
#endif
                sum = sum + ((float)ak * Cik[i][k] * cosf((M_PI * (float)(k - 1) * ((float)j - (float)0.5)) / (float)ji));
            }
            Tl[l] = sum;
#ifdef AMBE_DEBUG
            fprintf(stderr, "MBE: AMBE: Tl[%i]: %f", l, Tl[l]);
#endif
            l++;
        }
    }

    // determine log2Ml by applying ci,j to previous log2Ml

    // fix for when L > L(-1)
    if (cur_mp->L > prev_mp->L) {
        for (l = (prev_mp->L) + 1; l <= cur_mp->L; l++) {
            prev_mp->Ml[l] = prev_mp->Ml[prev_mp->L];
            prev_mp->log2Ml[l] = prev_mp->log2Ml[prev_mp->L];
        }
    }
    prev_mp->log2Ml[0] = prev_mp->log2Ml[1];
    prev_mp->Ml[0] = prev_mp->Ml[1];

    // Part 1
    Sum43 = 0;
    for (l = 1; l <= cur_mp->L; l++) {
        // eq. 40
        flokl[l] = ((float)prev_mp->L / (float)cur_mp->L) * (float)l;
        intkl[l] = (int)(flokl[l]);
#ifdef AMBE_DEBUG
        fprintf(stderr, "MBE: AMBE: flok%i: %f, intk%i: %i", l, flokl[l], l, intkl[l]);
#endif
        // eq. 41
        deltal[l] = flokl[l] - (float)intkl[l];
#ifdef AMBE_DEBUG
        fprintf(stderr, "MBE: AMBE: delta%i: %f", l, deltal[l]);
#endif
        // eq 43
        Sum43 = Sum43 + ((((float)1 - deltal[l]) * prev_mp->log2Ml[intkl[l]]) + (deltal[l] * prev_mp->log2Ml[intkl[l] + 1]));
    }

    Sum43 = (((float)0.65 / (float)cur_mp->L) * Sum43);
#ifdef AMBE_DEBUG
    fprintf(stderr, "MBE: AMBE: Sum43: %f", Sum43);
#endif

    // Part 2
    Sum42 = 0;
    for (l = 1; l <= cur_mp->L; l++) {
        Sum42 += Tl[l];
    }

    Sum42 = Sum42 / (float)cur_mp->L;
    BigGamma = cur_mp->gamma - ((float)0.5 * (log((float)cur_mp->L) / log((float)2))) - Sum42;
    //BigGamma=cur_mp->gamma - ((float)0.5 * log((float)cur_mp->L)) - Sum42;

    // Part 3
    for (l = 1; l <= cur_mp->L; l++) {
        c1 = ((float)0.65 * ((float)1 - deltal[l]) * prev_mp->log2Ml[intkl[l]]);
        c2 = ((float)0.65 * deltal[l] * prev_mp->log2Ml[intkl[l] + 1]);
        cur_mp->log2Ml[l] = Tl[l] + c1 + c2 - Sum43 + BigGamma;
        // inverse log to generate spectral amplitudes
        if (cur_mp->Vl[l] == 1) {
            cur_mp->Ml[l] = exp((float)0.693 * cur_mp->log2Ml[l]);
        }
        else {
            cur_mp->Ml[l] = unvc * exp((float)0.693 * cur_mp->log2Ml[l]);
        }
#ifdef AMBE_DEBUG
        fprintf(stderr, "MBE: AMBE: flokl[%i]: %f, intkl[%i]: %i", l, flokl[l], l, intkl[l]);
        fprintf(stderr, "MBE: AMBE: deltal[%i]: %f", l, deltal[l]);
        fprintf(stderr, "MBE: AMBE: prev_mp->log2Ml[%i]: %f", l, prev_mp->log2Ml[intkl[l]]);
        fprintf(stderr, "MBE: AMBE: BigGamma: %f c1: %f c2: %f Sum43: %f Tl[%i]: %f log2Ml[%i]: %f Ml[%i]: %f", BigGamma, c1, c2, Sum43, l, Tl[l], l, cur_mp->log2Ml[l], l, cur_mp->Ml[l]);
#endif
    }

    return (0);
}

/* */

int mbe_dequantizeAmbeTone(mbe_tone* tone, const int* u)
{
    int bitchk1, bitchk2;
    int AD, ID1, ID2, ID3, ID4;
    bitchk1 = (u[0] >> 6) & 0x3f;
    bitchk2 = (u[3] & 0xf);

    if ((bitchk1 != 63) || (bitchk2 != 0))
        return -1; // Not a valid tone frame

    AD = ((u[0] & 0x3f) << 1) + ((u[3] >> 4) & 0x1);
    ID1 = ((u[1] & 0xfff) >> 4);
    ID2 = ((u[1] & 0xf) << 4) + ((u[2] >> 7) & 0xf);
    ID3 = ((u[2] & 0x7f) << 1) + ((u[3] >> 13) & 0x1);
    ID4 = ((u[3] & 0x1fe0) >> 5);

    if ((ID1 == ID2) && (ID1 == ID3) && (ID1 == ID4) &&
        (((ID1 >= 5) && (ID1 <= 122)) || ((ID1 >= 128) && (ID1 <= 163)) || (ID1 == 255))) {
        if (tone->ID == ID1) {
            tone->AD = AD;
        }
        else {
            tone->n = 0;
            tone->ID = ID1;
            tone->AD = AD;
        }
        return 0; // valid in-range tone frequency 
    }

    return -1;
}
