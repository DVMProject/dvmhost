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
 * @file MotVoiceHeader1.h
 * @ingroup dfsi_frames
 * @file MotVoiceHeader1.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__MOT_VOICE_HEADER_1_H__)
#define __MOT_VOICE_HEADER_1_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "common/p25/dfsi/frames/FrameDefines.h"
#include "common/p25/dfsi/frames/MotStartOfStream.h"

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
             * @brief Implements a P25 Motorola voice header frame 1.
             * \code{.unparsed}
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |   Encoded Motorola Start of Stream                            |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |   ICW Flag ?  |     RSSI      |  RSSI Valid   |     RSSI      |
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
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     | Src Flag      |
             *     +-+-+-+-+-+-+-+-+
             * \endcode 
             * @ingroup dfsi_frames
             */
            class HOST_SW_API MotVoiceHeader1 {
            public:
                static const uint8_t LENGTH = 30U;
                static const uint8_t HCW_LENGTH = 21U;

                /**
                 * @brief Initializes a copy instance of the MotVoiceHeader1 class.
                 */
                MotVoiceHeader1();
                /**
                 * @brief Initializes a copy instance of the MotVoiceHeader1 class.
                 * @param data Buffer to containing MotVoiceHeader1 to decode.
                 */
                MotVoiceHeader1(uint8_t* data);
                /**
                 * @brief Finalizes a instance of the MotVoiceHeader1 class.
                 */
                ~MotVoiceHeader1();

                /**
                 * @brief Decode a voice header 1 frame.
                 * @param[in] data Buffer to containing MotVoiceHeader1 to decode.
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a voice header 1 frame.
                 * @param[out] data Buffer to encode a MotVoiceHeader1.
                 */
                void encode(uint8_t* data);
            
            public:
                uint8_t* header; // ?? - this should probably be private with getters/setters
                MotStartOfStream* startOfStream; // ?? - this should probably be private with getters/setters

                /**
                 * @brief 
                 * @note bryanb: I doubt the authenticity of this field.
                 */
                DECLARE_PROPERTY(ICWFlag::E, icw, ICW);
                /**
                 * @brief Flag indicating whether or not the RSSI field is valid.
                 * @note bryanb: I doubt the authenticity of this field.
                 */
                DECLARE_PROPERTY(RssiValidityFlag::E, rssiValidity, RSSIValidity);

                /**
                 * @brief RSSI Value.
                 */
                DECLARE_PROPERTY(uint8_t, rssi, RSSI);
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __MOT_VOICE_HEADER_1_H__