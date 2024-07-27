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
#include <iostream>
#include <string.h>
#include <math.h>

#include "common/edac/Golay24128.h"
#include "vocoder/MBEDecoder.h"

using namespace edac;
using namespace vocoder;

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

const int MBEDecoder::dW[72] = { 0,0,3,2,1,1,0,0,1,1,0,0,3,2,1,1,3,2,1,1,0,0,3,2,0,0,3,2,1,1,0,0,1,1,0,0,3,2,1,1,3,2,1,1,0,0,3,2,0,0,3,2,1,1,0,0,1,1,0,0,3,2,1,1,3,3,2,1,0,0,3,3, };

const int MBEDecoder::dX[72] = { 10,22,11,9,10,22,11,23,8,20,9,21,10,8,9,21,8,6,7,19,8,20,9,7,6,18,7,5,6,18,7,19,4,16,5,17,6,4,5,17,4,2,3,15,4,16,5,3,2,14,3,1,2,14,3,15,0,12,1,13,2,0,1,13,0,12,10,11,0,12,1,13, };

const int MBEDecoder::rW[36] = {
    0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 2,
    0, 2, 0, 2, 0, 2,
    0, 2, 0, 2, 0, 2
};

const int MBEDecoder::rX[36] = {
    23, 10, 22, 9, 21, 8,
    20, 7, 19, 6, 18, 5,
    17, 4, 16, 3, 15, 2,
    14, 1, 13, 0, 12, 10,
    11, 9, 10, 8, 9, 7,
    8, 6, 7, 5, 6, 4
};

// bit 0
const int MBEDecoder::rY[36] = {
    0, 2, 0, 2, 0, 2,
    0, 2, 0, 3, 0, 3,
    1, 3, 1, 3, 1, 3,
    1, 3, 1, 3, 1, 3,
    1, 3, 1, 3, 1, 3,
    1, 3, 1, 3, 1, 3
};

const int MBEDecoder::rZ[36] = {
    5, 3, 4, 2, 3, 1,
    2, 0, 1, 13, 0, 12,
    22, 11, 21, 10, 20, 9,
    19, 8, 18, 7, 17, 6,
    16, 5, 15, 4, 14, 3,
    13, 2, 12, 1, 11, 0
};

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/* Initializes a new instance of the MBEDecoder class. */

MBEDecoder::MBEDecoder(MBE_DECODER_MODE mode) :
    m_mbelibParms(NULL),
    m_mbeMode(mode),
    m_gainAdjust(1.0f)
{
    m_mbelibParms = new mbelibParms();
    mbe_initMbeParms(m_mbelibParms->m_cur_mp, m_mbelibParms->m_prev_mp, m_mbelibParms->m_prev_mp_enhanced);

    ::memset(gainMaxBuf, 0, sizeof(float) * 200);
    gainMaxBufPtr = gainMaxBuf;
    gainMaxIdx = 0;
}

/* Finalizes a instance of the MBEDecoder class. */

MBEDecoder::~MBEDecoder()
{
    delete m_mbelibParms;
}

/* Decodes the given MBE codewords to deinterleaved MBE bits using the decoder mode. */

int32_t MBEDecoder::decodeBits(uint8_t* codeword, char* mbeBits)
{
    int32_t errs = 0;
    float samples[160U];
    ::memset(samples, 0x00U, 160U);

    switch (m_mbeMode)
    {
    case DECODE_DMR_AMBE:
    {
        char ambe_d[49U];
        char ambe_fr[4][24];
        ::memset(ambe_d, 0x00U, 49U);
        ::memset(ambe_fr, 0x00U, 96U);

        const int* w, *x, *y, *z;

        w = rW;
        x = rX;
        y = rY;
        z = rZ;

        for (int i = 0; i < 9; ++i) {
            for (int j = 0; j < 8; j += 2) {
                ambe_fr[*y][*z] = (1 & (codeword[i] >> (7 - (j + 1))));
                ambe_fr[*w][*x] = (1 & (codeword[i] >> (7 - j)));
                w++;
                x++;
                y++;
                z++;
            }
        }

        errs = mbe_eccAmbe3600x2450C0(ambe_fr);
        mbe_demodulateAmbe3600x2450Data(ambe_fr);

        errs += mbe_eccAmbe3600x2450Data(ambe_fr, ambe_d);

        ::memcpy(mbeBits, ambe_d, 49U);
    }
    break;

    case DECODE_88BIT_IMBE:
    {
        char imbe_d[88U];
        ::memset(imbe_d, 0x00U, 88U);

        for (int i = 0; i < 11; ++i) {
            for (int j = 0; j < 8; j++) {
                imbe_d[j + (8 * i)] = (1 & (codeword[i] >> (7 - j)));
            }
        }

        ::memcpy(mbeBits, imbe_d, 88U);
    }
    break;
    }

    return errs;
}

/* Decodes the given MBE codewords to PCM samples using the decoder mode. */

int32_t MBEDecoder::decodeF(uint8_t* codeword, float samples[])
{
    int32_t errs = 0;
    switch (m_mbeMode)
    {
    case DECODE_DMR_AMBE:
    {
        char ambe_d[49U];
        char ambe_fr[4][24];
        ::memset(ambe_d, 0x00U, 49U);
        ::memset(ambe_fr, 0x00U, 96U);

        const int* w, *x, *y, *z;

        w = rW;
        x = rX;
        y = rY;
        z = rZ;

        for (int i = 0; i < 9; ++i) {
            for (int j = 0; j < 8; j += 2) {
                ambe_fr[*y][*z] = (1 & (codeword[i] >> (7 - (j + 1))));
                ambe_fr[*w][*x] = (1 & (codeword[i] >> (7 - j)));
                w++;
                x++;
                y++;
                z++;
            }
        }

        int ambeErrs;
        char ambeErrStr[64U];
        ::memset(ambeErrStr, 0x20U, 64U);

        mbe_processAmbe3600x2450FrameF(samples, &ambeErrs, &errs, ambeErrStr, ambe_fr, ambe_d, m_mbelibParms->m_cur_mp, m_mbelibParms->m_prev_mp, m_mbelibParms->m_prev_mp_enhanced, 3);
    }
    break;

    case DECODE_88BIT_IMBE:
    {
        char imbe_d[88U];
        ::memset(imbe_d, 0x00U, 88U);

        for (int i = 0; i < 11; ++i) {
            for (int j = 0; j < 8; j++) {
                imbe_d[j + (8 * i)] = (1 & (codeword[i] >> (7 - j)));
            }
        }

        int ambeErrs;
        char ambeErrStr[64U];
        ::memset(ambeErrStr, 0x20U, 64U);

        mbe_processImbe4400DataF(samples, &ambeErrs, &errs, ambeErrStr, imbe_d, m_mbelibParms->m_cur_mp, m_mbelibParms->m_prev_mp, m_mbelibParms->m_prev_mp_enhanced, 3);
    }
    break;
    }

    return errs;
}

/* Decodes the given MBE codewords to PCM samples using the decoder mode. */

int32_t MBEDecoder::decode(uint8_t* codeword, int16_t samples[])
{
    float samplesF[160U];
    ::memset(samplesF, 0x00U, 160U);
    int32_t errs = decodeF(codeword, samplesF);

    float* sampleFPtr = samplesF;
    if (m_autoGain) {
        // detect max level
        float max = 0.0f;
        for (int n = 0; n < 160; n++) {
            float out = fabsf(*sampleFPtr);
            if (out > max) {
                max = out;
            }

            sampleFPtr++;
        }

        *gainMaxBufPtr = max;
        gainMaxBufPtr++;
        gainMaxIdx++;

        if (gainMaxIdx > 24) {
            gainMaxIdx = 0;
            gainMaxBufPtr = gainMaxBuf;
        }

        // lookup max history
        for (int i = 0; i < 25; i++) {
            float a = gainMaxBuf[i];
            if (a > max) {
                max = a;
            }
        }

        // determine optimal gain level
        float gainFactor = 0.0f, gainDelta = 0.0f;
        if (max > static_cast<float>(0)) {
            gainFactor = (static_cast<float>(30000) / max);
        }
        else {
            gainFactor = static_cast<float>(50);
        }

        if (gainFactor < m_gainAdjust) {
            m_gainAdjust = gainFactor;
            gainDelta = static_cast<float>(0);
        }
        else {
            if (gainFactor > static_cast<float>(50)) {
                gainFactor = static_cast<float>(50);
            }

            gainDelta = gainFactor - m_gainAdjust;

            if (gainDelta > (static_cast<float>(0.05) * m_gainAdjust)) {
                gainDelta = (static_cast<float>(0.05) * m_gainAdjust);
            }
        }

        gainDelta /= static_cast<float>(160);

        // adjust output gain
        sampleFPtr = samplesF;
        for (int n = 0; n < 160; n++) {
            *sampleFPtr = (m_gainAdjust + (static_cast<float>(n) * gainDelta)) * (*sampleFPtr);
            sampleFPtr++;
        }

        m_gainAdjust += (static_cast<float>(160) * gainDelta);
    }

    int16_t* samplePtr = samples;
    sampleFPtr = samplesF;
    for (int n = 0; n < 160; n++) {
        float smp = *sampleFPtr;
        if (!m_autoGain) {
            smp *= m_gainAdjust;
        }

        // audio clipping
        if (smp > 32760) {
            smp = 32760;
        }
        else if (smp < -32760) {
            smp = -32760;
        }

        *samplePtr = (int16_t)(smp);
        
        samplePtr++;
        sampleFPtr++;
    }

    return errs;
}
