/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__RTP_EXTENSION_HEADER_H__)
#define __RTP_EXTENSION_HEADER_H__

#include "Defines.h"

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

        class RTPExtensionHeader {
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
            __PROTECTED_READONLY_PROPERTY(uint16_t, payloadLength, PayloadLength);
        };
    } // namespace frame
} // namespace network

#endif // __RTP_EXTENSION_HEADER_H__