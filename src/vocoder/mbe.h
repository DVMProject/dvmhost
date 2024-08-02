// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - MBE Vocoder
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 */
/**
 * @file mbe.h
 * @ingroup vocoder
 * @file mbe.c
 * @ingroup vocoder
 * @file ecc.c
 * @ingroup vocoder
 * @file imbe7200x4200.c
 * @ingroup vocoder
 * @file ambe3600x2450.c
 * @ingroup vocoder
 * @file ambe3600x2250.c
 * @ingroup vocoder
 */
#if !defined(__MBE_H__)
#define __MBE_H__

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

#ifdef __cplusplus
extern "C" {
#endif

// taken from GCC /usr/include/math.h
#ifndef M_E
#define M_E            2.7182818284590452354   /* e */
#endif
#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif
#ifndef M_SQRT2
#define M_SQRT2        1.41421356237309504880  /* sqrt(2) */
#endif

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

struct mbe_parameters
{
    float w0;
    int L;
    int K;
    int Vl[57];
    float Ml[57];
    float log2Ml[57];
    float PHIl[57];
    float PSIl[57];
    float gamma;
    int un;
    int repeat;
};

typedef struct mbe_parameters mbe_parms;

// ---------------------------------------------------------------------------
//  Structure Declaration
// ---------------------------------------------------------------------------

struct mbe_tones
{
    int ID;
    int AD;
    int n;
};

typedef struct mbe_tones mbe_tone;

// ---------------------------------------------------------------------------
//  Global Functions
// ---------------------------------------------------------------------------
/*
** Prototypes from ecc.c
*/
/**
 * @brief Helper to check MBE block golay.
 * @param block 
 */
void mbe_checkGolayBlock(long int* block);
/**
 * @brief Helper to check Golay (23,12).
 * @param[in] in 
 * @param[out] out 
 * @returns int 
 */
int mbe_golay2312(char* in, char* out);
/**
 * @brief Helper to check Hamming (15,11).
 * @param[in] in 
 * @param[out] out 
 * @returns int
 */
int mbe_hamming1511(char* in, char* out);
/**
 * @brief Helper to check Hamming (15,11).
 * @param[in] in
 * @param[out] out 
 * @returns int
 */
int mbe_7100x4400Hamming1511(char* in, char* out);

/*
** Prototypes from ambe3600x2400.c
*/
/**
 * @brief 
 * @param ambe_fr AMBE frames.
 * @returns int 
 */
int mbe_eccAmbe3600x2400C0(char ambe_fr[4][24]);
/**
 * @brief 
 * @param ambe_fr AMBE frames.
 * @param ambe_d AMBE data.
 * @returns int 
 */
int mbe_eccAmbe3600x2400Data(char ambe_fr[4][24], char* ambe_d);
/**
 * @brief 
 * @param ambe_d 
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @returns int 
 */
int mbe_decodeAmbe2400Parms(char* ambe_d, mbe_parms* cur_mp, mbe_parms* prev_mp);
/**
 * @brief 
 * @param ambe_fr AMBE frames.
 * @returns int 
 */
void mbe_demodulateAmbe3600x2400Data(char ambe_fr[4][24]);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in float samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processAmbe2400DataF(float* aout_buf, int* errs, int* errs2, char* err_str, char ambe_d[49], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in short samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int
 */
void mbe_processAmbe2400Data(short* aout_buf, int* errs, int* errs2, char* err_str, char ambe_d[49], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in float samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param ambe_fr AMBE frames.
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int
 */
void mbe_processAmbe3600x2400FrameF(float* aout_buf, int* errs, int* errs2, char* err_str, char ambe_fr[4][24], char ambe_d[49], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in short samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param ambe_fr AMBE frames.
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processAmbe3600x2400Frame(short* aout_buf, int* errs, int* errs2, char* err_str, char ambe_fr[4][24], char ambe_d[49], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);

/*
** Prototypes from ambe3600x2450.c
*/
/**
 * @brief 
 * @param ambe_fr AMBE frames.
 * @returns int 
 */
int mbe_eccAmbe3600x2450C0(char ambe_fr[4][24]);
/**
 * @brief 
 * @param ambe_fr AMBE frames.
 * @param ambe_d AMBE data.
 * @returns int 
 */
int mbe_eccAmbe3600x2450Data(char ambe_fr[4][24], char* ambe_d);
/**
 * @brief 
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @returns int 
 */
int mbe_decodeAmbe2450Parms(char* ambe_d, mbe_parms* cur_mp, mbe_parms* prev_mp);
/**
 * @brief 
 * @param ambe_fr AMBE frames.
 * @returns int 
 */
void mbe_demodulateAmbe3600x2450Data(char ambe_fr[4][24]);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in float samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processAmbe2450DataF(float* aout_buf, int* errs, int* errs2, char* err_str, char ambe_d[49], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in short samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processAmbe2450Data(short* aout_buf, int* errs, int* errs2, char* err_str, char ambe_d[49], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief
 * @param[out] aout_buf Audio Output (in float samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param ambe_fr AMBE frames.
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processAmbe3600x2450FrameF(float* aout_buf, int* errs, int* errs2, char* err_str, char ambe_fr[4][24], char ambe_d[49], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief
 * @param[out] aout_buf Audio Output (in short samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param ambe_fr AMBE frames.
 * @param ambe_d AMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processAmbe3600x2450Frame(short* aout_buf, int* errs, int* errs2, char* err_str, char ambe_fr[4][24], char ambe_d[49], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);

/*
** Prototypes from ambe3600x2250.c
*/
/**
 * @brief 
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param b 
 * @returns int 
 */
int mbe_dequantizeAmbe2250Parms(mbe_parms* cur_mp, mbe_parms* prev_mp, const int* b);
/**
 * @brief 
 * @param tone 
 * @param u 
 * @returns int 
 */
int mbe_dequantizeAmbeTone(mbe_tone* tone, const int* u);

/*
** Prototypes from imbe7200x4400.c
*/
/**
 * @brief 
 * @param imbe_fr IMBE frames.
 * @returns int 
 */
int mbe_eccImbe7200x4400C0(char imbe_fr[8][23]);
/**
 * @brief 
 * @param imbe_fr IMBE frames.
 * @param imbe_d IMBE data.
 * @returns int 
 */
int mbe_eccImbe7200x4400Data(char imbe_fr[8][23], char* imbe_d);
/**
 * @brief 
 * @param imbe_d IMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @returns int 
 */
int mbe_decodeImbe4400Parms(char* imbe_d, mbe_parms* cur_mp, mbe_parms* prev_mp);
/**
 * @brief 
 * @param imbe_fr IMBE frames.
 * @returns int 
 */
void mbe_demodulateImbe7200x4400Data(char imbe_fr[8][23]);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in float samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param imbe_d IMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processImbe4400DataF(float* aout_buf, int* errs, int* errs2, char* err_str, char imbe_d[88], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in short samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param imbe_d IMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processImbe4400Data(short* aout_buf, int* errs, int* errs2, char* err_str, char imbe_d[88], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in float samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param imbe_fr IMBE frames.
 * @param imbe_d IMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processImbe7200x4400FrameF(float* aout_buf, int* errs, int* errs2, char* err_str, char imbe_fr[8][23], char imbe_d[88], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in short samples)
 * @param errs 
 * @param errs2 
 * @param err_str 
 * @param imbe_fr IMBE frames.
 * @param imbe_d IMBE data.
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 * @param uvquality 
 * @returns int 
 */
void mbe_processImbe7200x4400Frame(short* aout_buf, int* errs, int* errs2, char* err_str, char imbe_fr[8][23], char imbe_d[88], mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced, int uvquality);

/*
** Prototypes from mbe.c
*/
/**
 * @brief 
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 */
void mbe_moveMbeParms(mbe_parms* cur_mp, mbe_parms* prev_mp);
/**
 * @brief 
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 */
void mbe_useLastMbeParms(mbe_parms* cur_mp, mbe_parms* prev_mp);
/**
 * @brief 
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param prev_mp_enhanced Previous MBE parameters.
 */
void mbe_initMbeParms(mbe_parms* cur_mp, mbe_parms* prev_mp, mbe_parms* prev_mp_enhanced);
/**
 * @brief 
 * @param cur_mp Current MBE parameters.
 */
void mbe_spectralAmpEnhance(mbe_parms* cur_mp);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in float samples)
 */
void mbe_synthesizeSilenceF(float* aout_buf);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in short samples)
 */
void mbe_synthesizeSilence(short* aout_buf);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in float samples)
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param uvquality 
 */
void mbe_synthesizeSpeechF(float* aout_buf, mbe_parms* cur_mp, mbe_parms* prev_mp, int uvquality);
/**
 * @brief 
 * @param[out] aout_buf Audio Output (in short samples)
 * @param cur_mp Current MBE parameters.
 * @param prev_mp Previous MBE parameters.
 * @param uvquality 
 */
void mbe_synthesizeSpeech(short* aout_buf, mbe_parms* cur_mp, mbe_parms* prev_mp, int uvquality);
/**
 * @brief 
 * @param[in] float_buf Audio (in short samples)
 * @param[out] aout_buf Audio (in short samples)
 */
void mbe_floatToShort(float* float_buf, short* aout_buf);

#ifdef __cplusplus
}
#endif

#endif // __MBE_H__
