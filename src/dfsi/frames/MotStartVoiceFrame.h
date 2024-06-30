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
#include "frames/FrameDefines.h"
#include "frames/MotStartOfStream.h"
#include "frames/MotFullRateVoice.h"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements a P25 Motorola voice frame 1/10 start.
         * \code{.unparsed}
         * Byte 0               1               2               3
         * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     |   Encoded Motorola Start of Stream                            |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     |   ICW Flag ?  |     RSSI      |  RSSI Valid   |     RSSI      |
         *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     |   Adj MM ?    |    Full Rate Voice Frame                      |
         *     +-+-+-+-+-+-+-+-+                                               +
         *     |                                                               |
         *     +                                                               +
         *     |                                                               |
         *     +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *     |               |
         *     +=+=+=+=+=+=+=+=+
         * \endcode
         * @ingroup dfsi_frames
         */
        class HOST_SW_API MotStartVoiceFrame {
        public:
            static const uint8_t LENGTH = 22;

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
             * @brief 
             */
            __PROPERTY(ICWFlag::E, icw, ICW);
            /**
             * @brief RSSI Value.
             */
            __PROPERTY(uint8_t, rssi, RSSI);
            /**
             * @brief Flag indicating whether or not the RSSI field is valid.
             */
            __PROPERTY(RssiValidityFlag::E, rssiValidity, RSSIValidity);
            /**
             * @brief 
             */
            __PROPERTY(uint8_t, nRssi, NRSSI);
            /**
             * @brief 
             */
            __PROPERTY(uint8_t, adjMM, AdjMM);
        };
    } // namespace dfsi
} // namespace p25

#endif // __MOT_START_VOICE_FRAME_H__