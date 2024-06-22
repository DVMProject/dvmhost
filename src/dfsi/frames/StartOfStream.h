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
#if !defined(__START_OF_STREAM_H__)
#define __START_OF_STREAM_H__

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
        //      Implements a P25 DFSI start of stream packet.
        // 
        // Byte 0               1               2
        // Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |              NID              | Rsvd  | Err C |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // ---------------------------------------------------------------------------

        class HOST_SW_API StartOfStream {
        public:
            static const uint8_t LENGTH = 4;

            /// <summary>Initializes a copy instance of the StartOfStream class.</summary>
            StartOfStream();
            /// <summary>Initializes a copy instance of the StartOfStream class.</summary>
            StartOfStream(uint8_t* data);

            /// <summary>Decode a start of stream frame.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a start of stream frame.</summary>
            void encode(uint8_t* data);
        
        public:
            /// <summary>Network Identifier.</summary>
            __PROPERTY(uint16_t, nid, NID);
            /// <summary>Error count.</summary>
            __PROPERTY(uint8_t, errorCount, ErrorCount);
        };
    } // namespace dfsi
} // namespace p25

#endif // __START_OF_STREAM_H__