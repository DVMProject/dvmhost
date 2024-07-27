// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Bridge
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
 /**
  * @file SampleTimeConversion.h
  * @ingroup bridge
  */
#if !defined(__SAMPLE_TIME_CONVERSION_H__)
#define __SAMPLE_TIME_CONVERSION_H__

#include "Defines.h"

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
* @brief 
* @ingroup bridge
*/
class HOST_SW_API SampleTimeConvert {
public:
    /**
     * @brief (ms) to sample count conversion
     * @param sampleRate Sample rate.
     * @param channels Number of audio channels.
     * @param ms Number of milliseconds.
     * @returns int Number of samples.
     */
    static int ToSamples(uint32_t sampleRate, uint8_t channels, int ms)
    {
        return (int)(((long)ms) * sampleRate * channels / 1000);
    }

    /**
     * @brief Sample count to (ms) conversion
     * @param sampleRate Sample rate.
     * @param channels Number of audio channels.
     * @param samples Number of samples.
     * @returns int Number of milliseconds.
     */
    static int ToMS(uint32_t sampleRate, uint8_t channels, int samples)
    {
        return (int)(((float)samples / (float)sampleRate / (float)channels) * 1000);
    }
};

#endif // __SAMPLE_TIME_CONVERSION_H__
