// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__MOT_VOICE_HEADER_1_H__)
#define __MOT_VOICE_HEADER_1_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "rtp/RtpDefines.h"
#include "rtp/MotStartOfStream.h"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements a P25 Motorola voice header frame 1.
        // 
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |   Encoded Motorola Start of Stream                            |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |   ICW Flag ?  |     RSSI      |  RSSI Valid   |     RSSI      |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |   Header Control Word                                         |
        //     +                                                               +
        //     |                                                               |
        //     +                                                               +
        //     |                                                               |
        //     +                                                               +
        //     |                                                               |
        //     +                                                               +
        //     |                                                               |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     | Src Flag      |
        //     +-+-+-+-+-+-+-+-+
        // ---------------------------------------------------------------------------

        class HOST_SW_API MotVoiceHeader1 {
        public:
            static const uint8_t LENGTH = 30;
            static const uint8_t HCW_LENGTH = 21;

            ICWFlag icw;
            uint8_t rssi;
            RssiValidityFlag rssiValidity;
            uint8_t nRssi;

            uint8_t* header;
            MotStartOfStream* startOfStream;

            /// <summary>Initializes a copy instance of the MotVoiceHeader1 class.</summary>
            MotVoiceHeader1();
            /// <summary>Initializes a copy instance of the MotVoiceHeader1 class.</summary>
            MotVoiceHeader1(uint8_t* data);
            /// <summary>Finalizes a instance of the MotVoiceHeader1 class.</summary>
            ~MotVoiceHeader1();

            /// <summary>Decode a voice header 1 frame.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a voice header 1 frame.</summary>
            void encode(uint8_t* data);
        };
    } // namespace dfsi
} // namespace p25

#endif // __MOT_VOICE_HEADER_1_H__