// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file FullRateVoice.h
 * @ingroup dfsi_frames
 * @file FullRateVoice.cpp
 * @ingroup dfsi_frames
 */
#if !defined(__FULL_RATE_VOICE_H__)
#define __FULL_RATE_VOICE_H__

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
             * @brief Implements a P25 full rate voice packet.
             * \code{.unparsed}
             * CAI Frames 1, 2, 10 and 11.
             * 14 bytes
             * 
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |       U0(b11-0)       |      U1(b11-0)        |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |        U2(b10-0)      |      U3(b11-0)        |   U4(b10-3)   |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |  U4 |     U5(b10-0)       |     U6(b10-0)       |  U7(b6-0)   |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |  Et | Er  |M|L|E|  E1 |SF | B |
             *     |     |     | | |4|     |   |   |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             * 
             * CAI Frames 3 - 8.
             * 18 bytes
             * 
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |       U0(b11-0)       |      U1(b11-0)        |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |        U2(b10-0)      |      U3(b11-0)        |   U4(b10-3)   |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |  U4 |     U5(b10-0)       |     U6(b10-0)       |  U7(b6-0)   |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |  Et | Er  |M|L|E|  E1 |SF | B | LC0,4,8   | LC1,5,9   | LC2,  |
             *     |     |     | | |4|     |   |   |           |           | 6,10  |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |   | LC3,7,11  |R| Status      |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             * 
             * CAI Frames 12 - 17.
             * 18 bytes
             * 
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |       U0(b11-0)       |      U1(b11-0)        |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |        U2(b10-0)      |      U3(b11-0)        |   U4(b10-3)   |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |  U4 |     U5(b10-0)       |     U6(b10-0)       |  U7(b6-0)   |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |  Et | Er  |M|L|E|  E1 |SF | B | ES0,4,8   | ES1,5,9   | ES2,  |
             *     |     |     | | |4|     |   |   |           |           | 6,10  |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |   | ES3,7,11  |R| Status      |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *
             * CAI Frames 9 and 10.
             * 17 bytes
             * 
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |       U0(b11-0)       |      U1(b11-0)        |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |        U2(b10-0)      |      U3(b11-0)        |   U4(b10-3)   |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |  U4 |     U5(b10-0)       |     U6(b10-0)       |  U7(b6-0)   |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |  Et | Er  |M|L|E|  E1 |SF | B | LSD0,2        | LSD1,3        |
             *     |     |     | | |4|     |   |   |               |               |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     | Rsvd  |Si |Sj |
             *     +=+=+=+=+=+=+=+=+
             * 
             * Because the TIA.102-BAHA spec represents the "message vectors" as 
             * 16-bit units (U0 - U7) this makes understanding the layout of the 
             * buffer ... difficult for the 8-bit aligned minded. The following is
             * the layout with 8-bit aligned IMBE blocks instead of message vectors:
             * 
             * CAI Frames 1, 2, 10 and 11.
             * 14 bytes
             *
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |    IMBE 1     |    IMBE 2     |    IMBE 3     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |  Et | Er  |M|L|E|  E1 |SF | B |
             *     |     |     | | |4|     |   |   |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             * 
             * CAI Frames 3 - 8.
             * 18 bytes
             *
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |    IMBE 1     |    IMBE 2     |    IMBE 3     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |  Et | Er  |M|L|E|  E1 |SF | B | Link Ctrl | Link Ctrl | Link  |
             *     |     |     | | |4|     |   |   |           |           |       |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |Ctr|R| Status    | Rsvd        |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             * 
             * CAI Frames 12 - 17.
             * 18 bytes
             *
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |    IMBE 1     |    IMBE 2     |    IMBE 3     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |  Et | Er  |M|L|E|  E1 |SF | B | Enc Sync  | Enc Sync  | Enc   |
             *     |     |     | | |4|     |   |   |           |           |       |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |Syn|R| Status    | Rsvd        |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             * 
             * CAI Frames 9 and 18.
             * 17 bytes
             *
             * Byte 0               1               2               3
             * Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |       FT      |    IMBE 1     |    IMBE 2     |    IMBE 3     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
             *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     |  Et | Er  |M|L|E|  E1 |SF | B | LSD0,2        | LSD1,3        |
             *     |     |     | | |4|     |   |   |               |               |
             *     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
             *     | Rsvd  |Si |Sj |
             *     +=+=+=+=+=+=+=+=+
             * \endcode
             * @ingroup dfsi_frames
             */
            class HOST_SW_API FullRateVoice {
            public:
                static const uint8_t LENGTH_121011 = 14U;
                static const uint8_t LENGTH_918 = 17U;

                static const uint8_t LENGTH = 18U;
                static const uint8_t ADDITIONAL_LENGTH = 4U;
                static const uint8_t IMBE_BUF_LEN = 11U;

                /**
                 * @brief Initializes a copy instance of the FullRateVoice class.
                 */
                FullRateVoice();
                /**
                 * @brief Initializes a copy instance of the FullRateVoice class.
                 * @param data Buffer to containing FullRateVoice to decode.
                 */
                FullRateVoice(uint8_t* data);
                /**
                 * @brief Finalizes a instance of the FullRateVoice class.
                 */
                ~FullRateVoice();

                /**
                 * @brief 
                 * @returns uint8_t 
                 */
                uint8_t getLength();

                /**
                 * @brief Decode a full rate voice frame.
                 * @param[in] data Buffer to containing FullRateVoice to decode.
                 * @returns bool 
                 */
                bool decode(const uint8_t* data);
                /**
                 * @brief Encode a full rate voice frame.
                 * @param[out] data Buffer to encode a FullRateVoice.
                 */
                void encode(uint8_t* data);

            public:
                uint8_t* imbeData; // ?? - this should probably be private with getters/setters
                uint8_t* additionalData; // ?? - this should probably be private with getters/setters

                /**
                 * @brief Frame Type.
                 */
                DECLARE_PROPERTY(defines::DFSIFrameType::E, frameType, FrameType);
                /**
                 * @brief Total errors detected in the frame.
                 */
                DECLARE_PROPERTY(uint8_t, totalErrors, TotalErrors);
                /**
                 * @brief Flag indicating the frame should be muted.
                 */
                DECLARE_PROPERTY(bool, muteFrame, MuteFrame);
                /**
                 * @brief Flag indicating the frame was lost.
                 */
                DECLARE_PROPERTY(bool, lostFrame, LostFrame);
                /**
                 * @brief Superframe Counter.
                 */
                DECLARE_PROPERTY(uint8_t, superframeCnt, SuperframeCnt);
                /**
                 * @brief Busy Status.
                 */
                DECLARE_PROPERTY(uint8_t, busy, Busy);

            private:
                /**
                 * @brief Helper indicating if the frame is voice 3 through 8.
                 * @returns bool True, if frame is voice 3 through 8, otherwise false.
                 */
                bool isVoice3thru8();
                /**
                 * @brief Helper indicating if the frame is voice 12 through 17.
                 * @returns bool True, if frame is voice 12 through 17, otherwise false.
                 */
                bool isVoice12thru17();
                /**
                 * @brief Helper indicating if the frame is voice 9 or 18.
                 * @returns bool True, if frame is voice 9, or 18, otherwise false.
                 */
                bool isVoice9or18();
            };
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __FULL_RATE_VOICE_H__