// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Modem Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI peer application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*
*/
#if !defined(__MOT_RTP_FRAMES_H__)
#define __MOT_RTP_FRAMES_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"

namespace p25
{
    namespace dfsi
    {
        enum RTFlag {
            ENABLED = 0x02U,
            DISABLED = 0x04U
        };

        enum StartStopFlag {
            START = 0x0CU,
            STOP = 0x25U
        };

        enum StreamTypeFlag {
            VOICE = 0x0BU
        };

        enum RssiValidityFlag {
            INVALID = 0x00U,
            VALID = 0x1A
        };

        enum SourceFlag {
            SOURCE_DIU = 0x00U,
            SOURCE_QUANTAR = 0x02U
        };

        enum ICWFlag {
            ICW_DIU = 0x00U,
            ICW_QUANTAR = 0x1B
        };

        // ---------------------------------------------------------------------------
        //  Classes for Motorola-specific DFSI Frames (aka "THE" manufacturer)
        // ---------------------------------------------------------------------------

        class MotFullRateVoice {
        public:
            // Frame information
            static const uint8_t LENGTH = 17;
            static const uint8_t SHORTENED_LENGTH = 13;
            static const uint8_t ADDITIONAL_LENGTH = 4;
            static const uint8_t IMBE_BUF_LEN = 11;

            uint8_t frameType;
            uint8_t* imbeData;
            uint8_t* additionalData;
            SourceFlag source;

            MotFullRateVoice();
            MotFullRateVoice(uint8_t* data);
            ~MotFullRateVoice();

            uint32_t size();
            bool decode(uint8_t* data, bool shortened = false);
            void encode(uint8_t* data, bool shortened = false);
        private:
            bool isVoice1or2or10or11();
            bool isVoice2or11();
            bool isVoice9or18();
        };

        class MotStartOfStream {
        public:
            static const uint8_t LENGTH = 10;
            static const uint8_t FIXED_MARKER = 0x02;

            uint8_t marker = FIXED_MARKER;

            RTFlag rt;
            StartStopFlag startStop;
            StreamTypeFlag streamType;

            MotStartOfStream();
            MotStartOfStream(uint8_t* data);

            bool decode(uint8_t* data);
            void encode(uint8_t* data);
        };

        class MotStartVoiceFrame {
        public:
            static const uint8_t LENGTH = 22;

            ICWFlag icw;
            uint8_t rssi;
            RssiValidityFlag rssiValidity;
            uint8_t nRssi;
            uint8_t adjMM;

            MotStartOfStream* startOfStream;
            MotFullRateVoice* fullRateVoice;

            MotStartVoiceFrame();
            MotStartVoiceFrame(uint8_t* data);
            ~MotStartVoiceFrame();

            bool decode(uint8_t* data);
            void encode(uint8_t* data);
        };

        class MotVoiceHeader1 {
        public:
            static const uint8_t LENGTH = 30;
            static const uint8_t HCW_LENGTH = 21;

            ICWFlag icw;
            uint8_t rssi;
            RssiValidityFlag rssiValidity;
            uint8_t nRssi;

            uint8_t* header;
            MotStartOfStream* startOfStream;

            MotVoiceHeader1();
            MotVoiceHeader1(uint8_t* data);
            ~MotVoiceHeader1();

            bool decode(uint8_t* data);
            void encode(uint8_t* data);
        };

        class MotVoiceHeader2 {
        public:
            static const uint8_t LENGTH = 22;
            static const uint8_t HCW_LENGTH = 20;

            ICWFlag icw;
            uint8_t rssi;
            RssiValidityFlag rssiValidity;
            uint8_t nRssi;
            MotStartOfStream startOfStream;
            SourceFlag source;

            uint8_t* header;

            MotVoiceHeader2();
            MotVoiceHeader2(uint8_t* data);
            ~MotVoiceHeader2();

            bool decode(uint8_t* data);
            void encode(uint8_t* data);
        };
    }
}

#endif // __MOT_RTP_FRAMES_H__