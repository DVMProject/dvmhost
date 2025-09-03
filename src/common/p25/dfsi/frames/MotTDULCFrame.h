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
 * @file MotTDULCFrame.h
 * @ingroup dfsi_frames
 * @file MotTDULCFrame.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__MOT_TDULC_FRAME_H__)
#define __MOT_TDULC_FRAME_H__

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
             * @brief Implements a P25 Motorola/V.24 TDULC frame.
             * \code{.unparsed}
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |  Encoded V.24 Start of Stream                 |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |                                                               |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |               |  TDULC                                        |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |                                                               |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |                               | Reserved                      |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API MotTDULCFrame {
            public:
                /**
                 * @brief Initializes a copy instance of the MotTDULCFrame class.
                 */
                MotTDULCFrame();
                /**
                 * @brief Initializes a copy instance of the MotTDULCFrame class.
                 * @param data Buffer to containing MotTDULCFrame to decode.
                 */
                MotTDULCFrame(uint8_t* data);
                /**
                 * @brief Finalizes a instance of the MotTDULCFrame class.
                 */
                ~MotTDULCFrame();

                /**
                 * @brief Decode a TDULC frame.
                 * @param[in] data Buffer to containing MotTDULCFrame to decode.
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a TDULC frame.
                 * @param[out] data Buffer to encode a MotTDULCFrame.
                 */
                void encode(uint8_t* data);
            
            public:
                MotStartOfStream* startOfStream; // ?? - this should probably be private with getters/setters
                uint8_t* tdulcData; // ?? - this should probably be private with getters/setters
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __MOT_TDULC_FRAME_H__