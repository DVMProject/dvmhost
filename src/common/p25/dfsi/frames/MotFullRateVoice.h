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
 * @file MotFullRateVoice.h
 * @ingroup dfsi_frames
 * @file MotFullRateVoice.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__MOT_FULL_RATE_VOICE_H__)
#define __MOT_FULL_RATE_VOICE_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/p25/dfsi/DFSIDefines.h"
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
             * @brief Implements a P25 Motorola full rate voice packet.
             * \code{.unparsed}
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |  Addtl Data   |  Addtl Data   |  Addtl Data   |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |   Reserved    |    IMBE 1     |    IMBE 2     |    IMBE 3     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    Src Flag   |
             *     +=+=+=+=+=+=+=+=+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API MotFullRateVoice {
            public:
                static const uint8_t LENGTH = 17U;
                static const uint8_t SHORTENED_LENGTH = 14U;
                static const uint8_t ADDITIONAL_LENGTH = 4U;

                /**
                 * @brief Initializes a copy instance of the MotFullRateVoice class.
                 */
                MotFullRateVoice();
                /**
                 * @brief Initializes a copy instance of the MotFullRateVoice class.
                 * @param data Buffer to containing MotFullRateVoice to decode.
                 */
                MotFullRateVoice(uint8_t* data);
                /**
                 * @brief Finalizes a instance of the MotFullRateVoice class.
                 */
                ~MotFullRateVoice();

                /**
                 * @brief 
                 */
                uint32_t size();
                /**
                 * @brief Decode a full rate voice frame.
                 * @param[in] data Buffer to containing MotFullRateVoice to decode.
                 * @param shortened Flag indicating this is a shortened frame.
                 */
                bool decode(const uint8_t* data, bool shortened = false);
                /**
                 * @brief Encode a full rate voice frame.
                 * @param[out] data Buffer to encode a MotFullRateVoice.
                 * @param shortened Flag indicating this is a shortened frame.
                 */
                void encode(uint8_t* data, bool shortened = false);

            public:
                uint8_t* imbeData; // ?? - this should probably be private with getters/setters
                uint8_t* additionalData; // ?? - this should probably be private with getters/setters

                /**
                 * @brief Frame Type.
                 */
                DECLARE_PROPERTY(defines::DFSIFrameType::E, frameType, FrameType);
                /**
                 * @brief V.24 Data Source.
                 */
                DECLARE_PROPERTY(SourceFlag::E, source, Source);

            private:
                /**
                 * @brief Helper indicating if the frame is voice 1, 2, 10 or 11.
                 * @returns bool True, if frame is voice 1, 2, 10, or 11, otherwise false.
                 */
                bool isVoice1or2or10or11();
                /**
                 * @brief Helper indicating if the frame is voice 2 or 11.
                 * @returns bool True, if frame is voice 2, or 11, otherwise false.
                 */
                bool isVoice2or11();
                /**
                 * @brief Helper indicating if the frame is voice 9 or 18.
                 * @returns bool True, if frame is voice 9, or 18, otherwise false.
                 */
                bool isVoice9or18();
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __MOT_FULL_RATE_VOICE_H__