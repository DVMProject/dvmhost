// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup analog Digital Mobile Radio
 * @brief Defines and implements aLaw and uLaw audio codecs, along with helper routines for analog audio.
 * @ingroup common
 * 
 * @file AnalogDefines.h
 * @ingroup analog
 */
#if !defined(__ANALOG_DEFINES_H__)
#define __ANALOG_DEFINES_H__

#include "common/Defines.h"

// Shorthand macro to analog::defines -- keeps source code that doesn't use "using" concise
#if !defined(ANODEF)
#define ANODEF analog::defines
#endif // DMRDEF
namespace analog
{
    namespace defines
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup analog
         * @{
         */

        const uint32_t  AUDIO_SAMPLES_LENGTH = 160U;        //! Sample size for 20ms of 16-bit audio at 8kHz.
        const uint32_t  AUDIO_SAMPLES_LENGTH_BYTES = 320U;  //! Sample size for 20ms of 16-bit audio at 8kHz in bytes.
        /** @} */

        /** @brief Audio Frame Type(s) */
        namespace AudioFrameType {
            /** @brief Audio Frame Type(s) */
            enum E : uint8_t {
                VOICE_START = 0x00U,                    //! Voice Start Frame
                VOICE = 0x01U,                          //! Voice Continuation Frame
                TERMINATOR = 0x02U,                     //! Voice End Frame / Call Terminator
            };
        }

    #define ANO_TERMINATOR           "Analog, TERMINATOR (Terminator)"
    #define ANO_VOICE                "Analog, VOICE (Voice Data)"
    } // namespace defines
} // namespace analog

#endif // __ANALOG_DEFINES_H__
