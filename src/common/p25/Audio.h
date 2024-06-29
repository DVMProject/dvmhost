// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file Audio.h
 * @ingroup p25
 * @file Audio.cpp
 * @ingroup p25
 */
#if !defined(__P25_AUDIO_H__)
#define __P25_AUDIO_H__

#include "common/Defines.h"
#include "common/edac/AMBEFEC.h"

namespace p25
{
    // ---------------------------------------------------------------------------
    //  Class Declaration
    // ---------------------------------------------------------------------------

    /**
     * @brief Implements P25 audio processing and interleaving.
     * @ingroup p25
     */
    class HOST_SW_API Audio {
    public:
        /**
         * @brief Initializes a new instance of the Audio class.
         */
        Audio();
        /**
         * @brief Finalizes a instance of the Audio class.
         */
        ~Audio();

        /**
         * @brief Process P25 IMBE audio data.
         * @param data IMBE audio buffer.
         * @returns uint32_t Number of errors in the audio buffer.
         */
        uint32_t process(uint8_t* data);

        /**
         * @brief Decode a P25 IMBE audio frame.
         * @param data Interleaved IMBE audio buffer.
         * @param imbe Raw IMBE buffer.
         * @param n Audio sequence.
         */
        void decode(const uint8_t* data, uint8_t* imbe, uint32_t n);
        /**
         * @brief Encode a P25 IMBE audio frame.
         * @param data Interleaved IMBE audio buffer.
         * @param imbe Raw IMBE buffer.
         * @param n Audio sequence.
         */
        void encode(uint8_t* data, const uint8_t* imbe, uint32_t n);

    private:
        edac::AMBEFEC m_fec;
    };
} // namespace p25

#endif // __P25_AUDIO_H__
