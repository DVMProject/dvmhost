// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024,2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file AnalogAudio.h
 * @ingroup common
 * @file AnalogAudio.cpp
 * @ingroup common
 */
#if !defined(__ANALOG_AUDIO_H__)
#define __ANALOG_AUDIO_H__

#include "common/Defines.h"

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define AUDIO_SAMPLES_LENGTH 160  // sample size for 20ms of 16-bit audio at 8kHz

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Defines and implements aLaw and uLaw audio codecs, along with helper routines for analog audio.
 * @ingroup common
 */
class HOST_SW_API AnalogAudio {
public:
    /**
     * @brief Helper to convert PCM into G.711 aLaw.
     * @param pcm PCM value.
     * @return uint8_t aLaw value.
     */
    static uint8_t encodeALaw(short pcm);
    /**
     * @brief Helper to convert G.711 aLaw into PCM.
     * @param alaw aLaw value.
     * @return short PCM value.
     */
    static short decodeALaw(uint8_t alaw);
    /**
     * @brief Helper to convert PCM into G.711 MuLaw.
     * @param pcm PCM value.
     * @return uint8_t MuLaw value.
     */
    static uint8_t encodeMuLaw(short pcm);
    /**
     * @brief Helper to convert G.711 MuLaw into PCM.
     * @param ulaw MuLaw value.
     * @return short PCM value.
     */
    static short decodeMuLaw(uint8_t ulaw);

    /**
     * @brief Helper to apply linear gain to PCM samples.
     * @param pcm Buffer containing PCM samples.
     * @param length Length of the PCM buffer in samples.
     * @param gain Gain factor to apply to the PCM samples (1.0f = no change).
     */
    static void gain(short *pcm, int length, float gain);

    /**
     * @brief (ms) to sample count conversion.
     * @param sampleRate Sample rate.
     * @param channels Number of audio channels.
     * @param ms Number of milliseconds.
     * @returns int Number of samples.
     */
    static int toSamples(uint32_t sampleRate, uint8_t channels, int ms) { return (int)(((long)ms) * sampleRate * channels / 1000); }

    /**
     * @brief Sample count to (ms) conversion.
     * @param sampleRate Sample rate.
     * @param channels Number of audio channels.
     * @param samples Number of samples.
     * @returns int Number of milliseconds.
     */
    static int toMS(uint32_t sampleRate, uint8_t channels, int samples) { return (int)(((float)samples / (float)sampleRate / (float)channels) * 1000); }

private:
    /**
     * @brief Searches for the segment in the given array.
     * @param val PCM value to search for.
     * @param table Array of segment values.
     * @param size Size of the segment array.
     * @return short 
     */
    static short search(short val, short* table, short size);
};

#endif // __ANALOG_AUDIO_H__
