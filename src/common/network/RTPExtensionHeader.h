// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__RTP_EXTENSION_HEADER_H__)
#define __RTP_EXTENSION_HEADER_H__

#include "common/Defines.h"

#include <chrono>
#include <random>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define RTP_EXTENSION_HEADER_LENGTH_BYTES 4

namespace network
{
    namespace frame
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents an RTP Extension header.
        // ---------------------------------------------------------------------------

        class HOST_SW_API RTPExtensionHeader {
        public:
            /// <summary>Initializes a new instance of the RTPExtensionHeader class.</summary>
            RTPExtensionHeader();
            /// <summary>Finalizes a instance of the RTPExtensionHeader class.</summary>
            ~RTPExtensionHeader();

            /// <summary>Decode a RTP header.</summary>
            virtual bool decode(const uint8_t* data);
            /// <summary>Encode a RTP header.</summary>
            virtual void encode(uint8_t* data);

        public:
            /// <summary>Format of the extension header payload contained within the packet.</summary>
            __PROTECTED_PROPERTY(uint16_t, payloadType, PayloadType);
            /// <summary>Length of the extension header payload (in 32-bit units).</summary>
            __PROTECTED_PROPERTY(uint16_t, payloadLength, PayloadLength);
        };
    } // namespace frame
} // namespace network

#endif // __RTP_EXTENSION_HEADER_H__