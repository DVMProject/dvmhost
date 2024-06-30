// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file MotVoiceHeader2.h
 * @ingroup dfsi_frames
 * @file MotVoiceHeader2.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__MOT_VOICE_HEADER_2_H__)
#define __MOT_VOICE_HEADER_2_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "frames/FrameDefines.h"
#include "frames/MotStartOfStream.h"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a P25 Motorola voice header frame 2.
         * \code{.unparsed}
         * Byte 0               1               2               3
         * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     |   Header Control Word                                         |
         *     +                                                               +
         *     |                                                               |
         *     +                                                               +
         *     |                                                               |
         *     +                                                               +
         *     |                                                               |
         *     +                                                               +
         *     |                                                               |
         *     +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     |               | Reserved      |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * \endcode 
         * @ingroup dfsi_frames
         */
        class HOST_SW_API MotVoiceHeader2 {
        public:
            static const uint8_t LENGTH = 22;
            static const uint8_t HCW_LENGTH = 20;

            /**
             * @brief Initializes a copy instance of the MotVoiceHeader2 class.
             */
            MotVoiceHeader2();
            /**
             * @brief Initializes a copy instance of the MotVoiceHeader2 class.
             * @param data Buffer to containing MotVoiceHeader2 to decode.
             */
            MotVoiceHeader2(uint8_t* data);
            /**
             * @brief Finalizes a instance of the MotVoiceHeader2 class.
             */
            ~MotVoiceHeader2();

            /**
             * @brief Decode a voice header 2 frame.
             * @param[in] data Buffer to containing MotVoiceHeader2 to decode.
             */
            bool decode(const uint8_t* data);
            /**
             * @brief Encode a voice header 2 frame.
             * @param[out] data Buffer to encode a MotVoiceHeader2.
             */
            void encode(uint8_t* data);
        
        public:
            uint8_t* header; // ?? - this should probably be a private with getters/setters

            /**
             * @brief V.24 Data Source.
             */
            __PROPERTY(SourceFlag::E, source, Source);
        };
    } // namespace dfsi
} // namespace p25

#endif // __MOT_VOICE_HEADER_2_H__