// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file MotPDUFrame.h
 * @ingroup dfsi_frames
 * @file MotPDUFrame.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__MOT_PDU_FRAME_H__)
#define __MOT_PDU_FRAME_H__

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
             * @brief Implements a P25 Motorola PDU frame.
             * \code{.unparsed}
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |   Encoded Motorola Start of Stream                            |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |   Reserved ?                                                  |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |   PDU Header                                                  |
             *     +                                                               +
             *     |                                                               |
             *     +                                                               +
             *     |                                                               |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API MotPDUFrame {
            public:
                static const uint8_t LENGTH = 20;

                /**
                 * @brief Initializes a copy instance of the MotPDUFrame class.
                 */
                MotPDUFrame();
                /**
                 * @brief Initializes a copy instance of the MotPDUFrame class.
                 * @param data Buffer to containing MotPDUFrame to decode.
                 */
                MotPDUFrame(uint8_t* data);
                /**
                 * @brief Finalizes a instance of the MotPDUFrame class.
                 */
                ~MotPDUFrame();

                /**
                 * @brief Decode a PDU frame. (only the PDU data header...)
                 * @param[in] data Buffer to containing MotPDUFrame to decode.
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a PDU frame. (only the PDU data header...)
                 * @param[out] data Buffer to encode a MotPDUFrame.
                 */
                void encode(uint8_t* data);
            
            public:
                MotStartOfStream* startOfStream; // ?? - this should probably be private with getters/setters
                uint8_t* pduHeaderData; // ?? - this should probably be private with getters/setters
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __MOT_PDU_FRAME_H__