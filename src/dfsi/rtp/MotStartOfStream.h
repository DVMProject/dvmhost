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
#if !defined(__MOT_START_OF_STREAM_H__)
#define __MOT_START_OF_STREAM_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "rtp/RtpDefines.h"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements a P25 Motorola start of stream packet.
        // 
        // Byte 0               1               2               3
        // Bit  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |   Fixed Mark  |  RT Mode Flag |  Start/Stop   |  Type Flag    |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |   Reserved                                                    |
        //     +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |               |
        //     +-+-+-+-+-+-+-+-+
        // ---------------------------------------------------------------------------

        class HOST_SW_API MotStartOfStream {
        public:
            static const uint8_t LENGTH = 10;
            static const uint8_t FIXED_MARKER = 0x02;

            uint8_t marker = FIXED_MARKER;

            RTFlag rt;
            StartStopFlag startStop;
            StreamTypeFlag streamType;

            /// <summary>Initializes a copy instance of the MotStartOfStream class.</summary>
            MotStartOfStream();
            /// <summary>Initializes a copy instance of the MotStartOfStream class.</summary>
            MotStartOfStream(uint8_t* data);

            /// <summary>Decode a start of stream frame.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a start of stream frame.</summary>
            void encode(uint8_t* data);
        };
    } // namespace dfsi
} // namespace p25

#endif // __MOT_START_OF_STREAM_H__