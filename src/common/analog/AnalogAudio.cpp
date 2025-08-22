// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
#include "Defines.h"
#include "analog/AnalogAudio.h"
#include "Log.h"
#include "Utils.h"

using namespace analog;

#include <cassert>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define SIGN_BIT (0x80)         // sign bit for a A-law byte
#define QUANT_MASK (0xf)        // quantization field mask
#define NSEGS (8)               // number of A-law segments
#define SEG_SHIFT (4)           // left shift for segment number
#define SEG_MASK (0x70)         // segment field mask

static short seg_aend[8] = { 0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF };
static short seg_uend[8] = { 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF };

#define	BIAS (0x84)             // bias for linear code
#define CLIP 8159

#define SHORT_CLIP_SMPL_MIN -32767
#define SHORT_CLIP_SMPL_MAX 32767

// ---------------------------------------------------------------------------
//  Public Static Class Members
// ---------------------------------------------------------------------------

/* Helper to convert PCM into G.711 aLaw. */

uint8_t AnalogAudio::encodeALaw(short pcm)
{
    short mask;
    uint8_t aval;

    pcm = pcm >> 3;

    if (pcm >= 0) {
        mask = 0xD5U;  // sign (7th) bit = 1
    } else {
        mask = 0x55U;  // sign bit = 0
        pcm = -pcm - 1;
    }

    // convert the scaled magnitude to segment number
    short seg = search(pcm, seg_aend, 8);

    /*
    ** combine the sign, segment, quantization bits
    */
    if (seg >= 8) // out of range, return maximum value
        return (uint8_t)(0x7F ^ mask);
    else {
        aval = (uint8_t) seg << SEG_SHIFT;
        if (seg < 2)
            aval |= (pcm >> 1) & QUANT_MASK;
        else
            aval |= (pcm >> seg) & QUANT_MASK;

        return (aval ^ mask);
    }
}

/* Helper to convert G.711 aLaw into PCM. */

short AnalogAudio::decodeALaw(uint8_t alaw)
{
    alaw ^= 0x55U;

    short t = (alaw & QUANT_MASK) << 4;
    short seg = ((unsigned)alaw & SEG_MASK) >> SEG_SHIFT;
    switch (seg) {
    case 0:
        t += 8;
        break;
    case 1:
        t += 0x108U;
        break;
    default:
        t += 0x108U;
        t <<= seg - 1;
    }

    return ((alaw & SIGN_BIT) ? t : -t);
}

/* Helper to convert PCM into G.711 MuLaw. */

uint8_t AnalogAudio::encodeMuLaw(short pcm)
{
    short mask;

    // get the sign and the magnitude of the value
    pcm = pcm >> 2;
    if (pcm < 0) {
        pcm = -pcm;
        mask = 0x7FU;
    } else {
        mask = 0xFFU;
    }

    // clip the magnitude
    if (pcm > CLIP)
        pcm = CLIP;
    pcm += (BIAS >> 2);

    // convert the scaled magnitude to segment number
    short seg = search(pcm, seg_uend, 8);

    /*
    ** combine the sign, segment, quantization bits;
    ** and complement the code word
    */
    if (seg >= 8) // out of range, return maximum value.
        return (uint8_t)(0x7F ^ mask);
    else {
        uint8_t ulaw = (uint8_t)(seg << 4) | ((pcm >> (seg + 1)) & 0xF);
        return (ulaw ^ mask);
    }
}

/* Helper to convert G.711 MuLaw into PCM. */

short AnalogAudio::decodeMuLaw(uint8_t ulaw)
{
    // complement to obtain normal u-law value
    ulaw = ~ulaw;

    /*
    ** extract and bias the quantization bits; then
    ** shift up by the segment number and subtract out the bias
    */
    short t = ((ulaw & QUANT_MASK) << 3) + BIAS;
    t <<= ((unsigned)ulaw & SEG_MASK) >> SEG_SHIFT;

    return ((ulaw & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
}

/* Helper to apply linear gain to PCM samples. */

void AnalogAudio::gain(short* pcm, int length, float gain)
{
    assert(pcm != nullptr);
    assert(length > 0);

    if (gain == 1.0f)
        return;

    for (int i = 0; i < length; i++) {
        int sample = static_cast<int>(pcm[i] * gain);
        if (sample > SHORT_CLIP_SMPL_MAX)
            sample = SHORT_CLIP_SMPL_MAX;
        else if (sample < SHORT_CLIP_SMPL_MIN)
            sample = SHORT_CLIP_SMPL_MIN;

        pcm[i] = static_cast<short>(sample);
    }
}

// ---------------------------------------------------------------------------
//  Private Static Class Members
// ---------------------------------------------------------------------------

/* Searches for the segment in the given array. */

short AnalogAudio::search(short val, short* table, short size)
{
    for (short i = 0; i < size; i++) {
        if (val <= *table++)
            return (i);
    }

   return (size);
}
