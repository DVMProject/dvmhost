// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file MotStartVoiceFrame.h
 * @ingroup dfsi_frames
 * @file MotStartVoiceFrame.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__MOT_START_VOICE_FRAME_H__)
#define __MOT_START_VOICE_FRAME_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/p25/dfsi/frames/FrameDefines.h"
#include "common/p25/dfsi/frames/MotStartOfStream.h"
#include "common/p25/dfsi/frames/MotFullRateVoice.h"

namespace p25
{
    namespace dfsi
    {
        namespace frames
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Implements a P25 Motorola/V.24 voice frame 1/10 start.
             * \code{.unparsed}
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |  Encoded V.24 Start of Stream                 |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+--+-+-+-+-+-+-+--+-+-+-+-+-+-+-+-+
             *     |                                                               |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |               |    Full Rate Voice Frame                      |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |                                                               |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |                                                               |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |               |
             *     +=+=+=+=+=+=+=+=+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API MotStartVoiceFrame {
            public:
                static const uint8_t LENGTH = 22U;

                /**
                 * @brief Initializes a copy instance of the MotStartVoiceFrame class.
                 */
                MotStartVoiceFrame();
                /**
                 * @brief Initializes a copy instance of the MotStartVoiceFrame class.
                 * @param data Buffer to containing MotStartVoiceFrame to decode.
                 */
                MotStartVoiceFrame(uint8_t* data);
                /**
                 * @brief Finalizes a instance of the MotStartVoiceFrame class.
                 */
                ~MotStartVoiceFrame();

                /**
                 * @brief Decode a start voice frame.
                 * @param[in] data Buffer to containing MotStartVoiceFrame to decode.
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a start voice frame.
                 * @param[out] data Buffer to encode a MotStartVoiceFrame.
                 */
                void encode(uint8_t* data);
            
            public:
                MotStartOfStream* startOfStream; // ?? - this should probably be private with getters/setters
                MotFullRateVoice* fullRateVoice; // ?? - this should probably be private with getters/setters

                /**
                 * @brief Total errors detected in the frame.
                 */
                DECLARE_PROPERTY(uint8_t, totalErrors, TotalErrors);
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __MOT_START_VOICE_FRAME_H__