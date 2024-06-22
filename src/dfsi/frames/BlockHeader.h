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
#if !defined(__BLOCK_HEADER_H__)
#define __BLOCK_HEADER_H__

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
        //      Implements a DFSI block header packet.
        // 
        // Compact Form
        // Byte 0
        // Bit  7 6 5 4 3 2 1 0
        //     +-+-+-+-+-+-+-+-+
        //     |E|      BT     |
        //     +-+-+-+-+-+-+-+-+
        // 
        // Verbose Form
        // Byte 0               1               2               3
        // Bit  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //     |E|      BT     |             TSO           |         BL        |
        //     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // ---------------------------------------------------------------------------

        class HOST_SW_API BlockHeader {
        public:
            static const uint8_t LENGTH = 1;
            static const uint8_t VERBOSE_LENGTH = 4;

            /// <summary>Initializes a copy instance of the BlockHeader class.</summary>
            BlockHeader();
            /// <summary>Initializes a copy instance of the BlockHeader class.</summary>
            BlockHeader(uint8_t* data, bool verbose = false);

            /// <summary>Decode a block header frame.</summary>
            bool decode(const uint8_t* data, bool verbose = false);
            /// <summary>Encode a block header frame.</summary>
            void encode(uint8_t *data, bool verbose = false);

        public:
            /// <summary>Payload type.</summary>
            /// <remarks>This simple boolean marks this header as either IANA standard, or profile specific.</remarks>
            __PROPERTY(bool, payloadType, PayloadType);
            /// <summary>Block type.</summary>
            __PROPERTY(BlockType::E, blockType, BlockType);
            /// <summary>Timestamp Offset.</summary>
            __PROPERTY(uint16_t, timestampOffset, TimestampOffset);
            /// <summary>Block length.</summary>
            __PROPERTY(uint16_t, blockLength, BlockLength);
        };
    } // namespace dfsi
} // namespace p25

#endif // __BLOCK_HEADER_H__