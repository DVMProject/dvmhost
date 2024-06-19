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
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__MOT_FULL_RATE_VOICE_H__)
#define __MOT_FULL_RATE_VOICE_H__

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
        //      Implements a P25 Motorola full rate voice packet.
        // 
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |       FT      |  Addtl Data   |  Addtl Data   |  Addtl Data   |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |   Reserved    |    IMBE 1     |    IMBE 2     |    IMBE 3     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 4     |    IMBE 5     |    IMBE 6     |    IMBE 7     |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    IMBE 8     |    IMBE 9     |    IMBE 10    |    IMBE 11    |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |    Src Flag   |
        //     +=+=+=+=+=+=+=+=+
        // ---------------------------------------------------------------------------

        class HOST_SW_API MotFullRateVoice {
        public:
            static const uint8_t LENGTH = 17;
            static const uint8_t SHORTENED_LENGTH = 13;
            static const uint8_t ADDITIONAL_LENGTH = 4;
            static const uint8_t IMBE_BUF_LEN = 11;

            /// <summary>Initializes a copy instance of the MotFullRateVoice class.</summary>
            MotFullRateVoice();
            /// <summary>Initializes a copy instance of the MotFullRateVoice class.</summary>
            MotFullRateVoice(uint8_t* data);
            /// <summary>Finalizes a instance of the MotFullRateVoice class.</summary>
            ~MotFullRateVoice();

            /// <summary></summary>
            uint32_t size();
            /// <summary>Decode a full rate voice frame.</summary>
            bool decode(const uint8_t* data, bool shortened = false);
            /// <summary>Encode a full rate voice frame.</summary>
            void encode(uint8_t* data, bool shortened = false);

        public:
            uint8_t* imbeData; // ?? - this should probably be private with getters/setters
            uint8_t* additionalData; // ?? - this should probably be private with getters/setters

            /// <summary>Frame Type.</summary>
            __PROPERTY(uint8_t, frameType, FrameType);
            /// <summary></summary>
            __PROPERTY(uint8_t, source, Source);

        private:
            /// <summary></summary>
            bool isVoice1or2or10or11();
            /// <summary></summary>
            bool isVoice2or11();
            /// <summary></summary>
            bool isVoice9or18();
        };
    } // namespace dfsi
} // namespace p25

#endif // __MOT_FULL_RATE_VOICE_H__