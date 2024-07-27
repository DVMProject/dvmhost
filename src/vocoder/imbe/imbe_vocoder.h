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
#ifndef __IMBE_VOCODER_H__
#define __IMBE_VOCODER_H__

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

#include <stdint.h>
#include <string.h>

#include "vocoder/imbe/imbe.h"
#include "vocoder/imbe/basic_op.h"
#include "vocoder/imbe/math_sub.h"

// ---------------------------------------------------------------------------
//  Class Declaration
// 
// ---------------------------------------------------------------------------

class imbe_vocoder {
public:
    imbe_vocoder(void);
    ~imbe_vocoder() { }

    // imbe_encode compresses 160 samples (in unsigned int format)
    // outputs u[] vectors as frame_vector[]
    void imbe_encode(int16_t *frame_vector, int16_t *snd)
    {
        encode(&my_imbe_param, frame_vector, snd);
    }
    
    // imbe_decode decodes IMBE codewords (frame_vector),
    // outputs the resulting 160 audio samples (snd)
    void imbe_decode(int16_t *frame_vector, int16_t *snd)
    {
        decode(&my_imbe_param, frame_vector, snd);
    }
    
    // hack to enable ambe encoder read access to speech parameters
    const IMBE_PARAM* param(void) { return &my_imbe_param; }
    void set_gain_adjust(float gain_adjust) { d_gain_adjust = gain_adjust; }

private:
    IMBE_PARAM my_imbe_param;

    /* data items originally static (moved from individual c++ sources) */
    Word16 prev_pitch, prev_prev_pitch, prev_e_p, prev_prev_e_p;
    UWord32 seed;
    Word16 num_harms_prev1;
    Word32 sa_prev1[NUM_HARMS_MAX + 2];
    Word16 num_harms_prev2;
    Word32 sa_prev2[NUM_HARMS_MAX + 2];
    Word16 uv_mem[105];
    UWord32 ph_mem[NUM_HARMS_MAX];
    Word16 num_harms_prev3;
    Word32 fund_freq_prev;
    Word16 vu_dsn_prev[NUM_HARMS_MAX];
    Word16 sa_prev3[NUM_HARMS_MAX];
    Word32 th_max;
    Word16 v_uv_dsn[NUM_BANDS_MAX];
    Word16 wr_array[FFTLENGTH / 2 + 1];
    Word16 wi_array[FFTLENGTH / 2 + 1];
    Word16 pitch_est_buf[PITCH_EST_BUF_SIZE];
    Word16 pitch_ref_buf[PITCH_EST_BUF_SIZE];
    Word32 dc_rmv_mem;
    Cmplx16 fft_buf[FFTLENGTH];
    Word16 pe_lpf_mem[PE_LPF_ORD];
    float d_gain_adjust;

    /* member functions */
    void idct(Word16 *in, Word16 m_lim, Word16 i_lim, Word16 *out);
    void dct(Word16 *in, Word16 m_lim, Word16 i_lim, Word16 *out);
    void fft_init(void);
    void fft(Word16 *datam1, Word16 nn, Word16 isign);
    void encode(IMBE_PARAM *imbe_param, Word16 *frame_vector, Word16 *snd);
    void pitch_est_init(void);
    Word32 autocorr(Word16 *sigin, Word16 shift, Word16 scale_shift);
    void e_p(Word16 *sigin, Word16 *res_buf);
    void pitch_est(IMBE_PARAM *imbe_param, Word16 *frames_buf);
    void sa_decode_init(void);
    void sa_decode(IMBE_PARAM *imbe_param);
    void sa_encode_init(void);
    void sa_encode(IMBE_PARAM *imbe_param);
    void uv_synt_init(void);
    void uv_synt(IMBE_PARAM *imbe_param, Word16 *snd);
    void v_synt_init(void);
    void v_synt(IMBE_PARAM *imbe_param, Word16 *snd);
    void pitch_ref_init(void);
    Word16 voiced_sa_calc(Word32 num, Word16 den);
    Word16 unvoiced_sa_calc(Word32 num, Word16 den);
    void v_uv_det(IMBE_PARAM *imbe_param, Cmplx16 *fft_buf);
    void decode_init(IMBE_PARAM *imbe_param);
    void decode(IMBE_PARAM *imbe_param, Word16 *frame_vector, Word16 *snd);
    void encode_init(void);
};

#endif // __IMBE_VOCODER_H__
