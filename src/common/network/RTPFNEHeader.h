// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023,2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__RTP_FNE_HEADER_H__)
#define __RTP_FNE_HEADER_H__

#include "common/Defines.h"
#include "common/network/RTPExtensionHeader.h"

#include <chrono>
#include <random>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define RTP_FNE_HEADER_LENGTH_BYTES 16
#define RTP_FNE_HEADER_LENGTH_EXT_LEN 4

#define RTP_END_OF_CALL_SEQ 65535

namespace network
{
    namespace frame
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        const uint8_t DVM_FRAME_START = 0xFEU;

        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents the FNE RTP Extension header.
        // ---------------------------------------------------------------------------

        class HOST_SW_API RTPFNEHeader : public RTPExtensionHeader {
        public:
            /// <summary>Initializes a new instance of the RTPFNEHeader class.</summary>
            RTPFNEHeader();
            /// <summary>Finalizes a instance of the RTPFNEHeader class.</summary>
            ~RTPFNEHeader();

            /// <summary>Decode a RTP header.</summary>
            bool decode(const uint8_t* data) override;
            /// <summary>Encode a RTP header.</summary>
            void encode(uint8_t* data) override;

        public:
            /// <summary>Traffic payload packet CRC-16.</summary>
            __PROPERTY(uint16_t, crc16, CRC);
            /// <summary>Function.</summary>
            __PROPERTY(uint8_t, func, Function);
            /// <summary>Sub-function.</summary>
            __PROPERTY(uint8_t, subFunc, SubFunction);
            /// <summary>Traffic Stream ID.</summary>
            __PROPERTY(uint32_t, streamId, StreamId);
            /// <summary>Traffic Peer ID.</summary>
            __PROPERTY(uint32_t, peerId, PeerId);
            /// <summary>Traffic Message Length.</summary>
            __PROPERTY(uint32_t, messageLength, MessageLength);
        };
    } // namespace frame
} // namespace network

#endif // __RTP_FNE_HEADER_H__