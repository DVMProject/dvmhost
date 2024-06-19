// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - DFSI Peer Application
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI Peer Application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__FULL_RATE_VOICE_H__)
#define __FULL_RATE_VOICE_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "frames/FrameDefines.h"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements a P25 full rate voice packet.
        // 
        // CAI Frames 1, 2, 10 and 11.
        // 
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |       U0(b11-0)       |      U1(b11-0)        |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |        U2(b10-0)      |      U3(b11-0)        |   U4(b10-3)   |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |  U4 |     U5(b10-0)       |     U6(b10-0)       |  U7(b6-0)   |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |  Et | Er  |M|L|E|  E1 |SF | B |
        //     |     |     | | |4|     |   |   |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        // 
        // CAI Frames 3 - 8.
        // 
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |       U0(b11-0)       |      U1(b11-0)        |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |        U2(b10-0)      |      U3(b11-0)        |   U4(b10-3)   |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |  U4 |     U5(b10-0)       |     U6(b10-0)       |  U7(b6-0)   |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |  Et | Er  |M|L|E|  E1 |SF | B | LC0,4,8   | LC1,5,9   | LC2,  |
        //     |     |     | | |4|     |   |   |           |           | 6,10  |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |   | LC3,7,11  |R| Status      |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        // 
        // CAI Frames 12 - 17.
        // 
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |       U0(b11-0)       |      U1(b11-0)        |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |        U2(b10-0)      |      U3(b11-0)        |   U4(b10-3)   |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |  U4 |     U5(b10-0)       |     U6(b10-0)       |  U7(b6-0)   |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |  Et | Er  |M|L|E|  E1 |SF | B | ES0,4,8   | ES1,5,9   | ES2,  |
        //     |     |     | | |4|     |   |   |           |           | 6,10  |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |   | ES3,7,11  |R| Status      |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //
        // CAI Frames 9 and 10.
        // 
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |       U0(b11-0)       |      U1(b11-0)        |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |        U2(b10-0)      |      U3(b11-0)        |   U4(b10-3)   |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |  U4 |     U5(b10-0)       |     U6(b10-0)       |  U7(b6-0)   |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |  Et | Er  |M|L|E|  E1 |SF | B | LSD0,2        | LSD1,3        |
        //     |     |     | | |4|     |   |   |               |               |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     | Rsvd  |Si |Sj |
        //     +=+=+=+=+=+=+=+=+
        // 
        // Because the TIA.102-BAHA spec represents the "message vectors" as 
        // 16-bit units (U0 - U7) this makes understanding the layout of the 
        // buffer ... difficult for the 8-bit aligned minded. The following is
        // the layout with 8-bit aligned IMBE blocks instead of message vectors:
        // 
        // CAI Frames 1, 2, 10 and 11.
        //
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |    IMBE 1     |    IMBE 2     |    IMBE 3     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |  Et | Er  |M|L|E|  E1 |SF | B |
        //     |     |     | | |4|     |   |   |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        // 
        // CAI Frames 3 - 8.
        //
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |    IMBE 1     |    IMBE 2     |    IMBE 3     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |  Et | Er  |M|L|E|  E1 |SF | B |    Link Ctrl  |    Link Ctrl  |
        //     |     |     | | |4|     |   |   |               |               |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |    Link Ctrl  |R| Status      |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        // 
        // CAI Frames 12 - 17.
        //
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |    IMBE 1     |    IMBE 2     |    IMBE 3     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |  Et | Er  |M|L|E|  E1 |SF | B |    Enc Sync   |    Enc Sync   |
        //     |     |     | | |4|     |   |   |               |               |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |    Enc Sync   |R| Status      |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        // 
        // CAI Frames 9 and 10.
        //
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |    IMBE 1     |    IMBE 2     |    IMBE 3     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     |  Et | Er  |M|L|E|  E1 |SF | B | LSD0,2        | LSD1,3        |
        //     |     |     | | |4|     |   |   |               |               |
        //     +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        //     | Rsvd  |Si |Sj |
        //     +=+=+=+=+=+=+=+=+
        // ---------------------------------------------------------------------------

        class HOST_SW_API FullRateVoice {
        public:
            static const uint8_t LENGTH = 18;
            static const uint8_t ADDITIONAL_LENGTH = 4;
            static const uint8_t IMBE_BUF_LEN = 11;

            /// <summary>Initializes a copy instance of the FullRateVoice class.</summary>
            FullRateVoice();
            /// <summary>Initializes a copy instance of the FullRateVoice class.</summary>
            FullRateVoice(uint8_t* data);
            /// <summary>Finalizes a instance of the FullRateVoice class.</summary>
            ~FullRateVoice();

            /// <summary>Decode a full rate voice frame.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a full rate voice frame.</summary>
            void encode(uint8_t* data);

        public:
            uint8_t* imbeData; // ?? - this should probably be private with getters/setters
            uint8_t* additionalData; // ?? - this should probably be private with getters/setters

            /// <summary>Frame Type.</summary>
            __PROPERTY(uint8_t, frameType, FrameType);
            /// <summary>Total errors detected in the frame.</summary>
            __PROPERTY(uint8_t, totalErrors, TotalErrors);
            /// <summary>Flag indicating the frame should be muted.</summary>
            __PROPERTY(bool, muteFrame, MuteFrame);
            /// <summary>Flag indicating the frame was lost.</summary>
            __PROPERTY(bool, lostFrame, LostFrame);
            /// <summary>Superframe Counter.</summary>
            __PROPERTY(uint8_t, superframeCnt, SuperframeCnt);
            /// <summary>Busy Status.</summary>
            __PROPERTY(uint8_t, busy, Busy);

        private:
            /// <summary></summary>
            bool isVoice3thru8();
            /// <summary></summary>
            bool isVoice12thru17();
            /// <summary></summary>
            bool isVoice9or10();
        };
    } // namespace dfsi
} // namespace p25

#endif // __FULL_RATE_VOICE_H__