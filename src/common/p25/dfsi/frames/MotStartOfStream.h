// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
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
#if !defined(__MOT_START_OF_STREAM_H__)
#define __MOT_START_OF_STREAM_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/p25/dfsi/frames/FrameDefines.h"

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
             * @brief Implements a P25 Motorola start of stream packet.
             * \code{.unparsed}
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |   Fixed Mark  |  RT Mode Flag |  Start/Stop   |  Type Flag    |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |   Reserved                                                    |
             *     +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |               |
             *     +-+-+-+-+-+-+-+-+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API MotStartOfStream {
            public:
                static const uint8_t LENGTH = 10;
                static const uint8_t FIXED_MARKER = 0x02;

                /**
                 * @brief Initializes a copy instance of the MotStartOfStream class.
                 */
                MotStartOfStream();
                /**
                 * @brief Initializes a copy instance of the MotStartOfStream class.
                 * @param data Buffer to containing MotStartOfStream to decode.
                 */
                MotStartOfStream(uint8_t* data);

                /**
                 * @brief Decode a start of stream frame.
                 * @param[in] data Buffer to containing MotStartOfStream to decode.
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a start of stream frame.
                 * @param[out] data Buffer to encode a MotStartOfStream.
                 */
                void encode(uint8_t* data);
            
            public:
                /**
                 * @brief 
                 */
                __PROPERTY(uint8_t, marker, Marker);
                /**
                 * @brief RT/RT Flag.
                 */
                __PROPERTY(RTFlag::E, rt, RT);
                /**
                 * @brief Start/Stop.
                 */
                __PROPERTY(StartStopFlag::E, startStop, StartStop);
                /**
                 * @brief Stream Type.
                 */
                __PROPERTY(StreamTypeFlag::E, streamType, StreamType);
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __MOT_START_OF_STREAM_H__