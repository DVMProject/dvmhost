/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
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
#if !defined(__RTP_HEADER_H__)
#define __RTP_HEADER_H__

#include "common/Defines.h"

#include <chrono>
#include <string>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define INVALID_TS UINT32_MAX

#define RTP_HEADER_LENGTH_BYTES 12
#define RTP_GENERIC_CLOCK_RATE 8000

namespace network
{
    namespace frame
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents an RTP header.
        // ---------------------------------------------------------------------------

        class HOST_SW_API RTPHeader {
        public:
            /// <summary>Initializes a new instance of the RTPHeader class.</summary>
            RTPHeader();
            /// <summary>Finalizes a instance of the RTPHeader class.</summary>
            ~RTPHeader();

            /// <summary>Decode a RTP header.</summary>
            virtual bool decode(const uint8_t* data);
            /// <summary>Encode a RTP header.</summary>
            virtual void encode(uint8_t* data);

            /// <summary>Helper to reset the start timestamp.</summary>
            static void resetStartTime();

        public:
            /// <summary>RTP Protocol Version.</summary>
            __READONLY_PROPERTY(uint8_t, version, Version);
            /// <summary>Flag indicating if the packet has trailing padding.</summary>
            __READONLY_PROPERTY(bool, padding, Padding);
            /// <summary>Flag indicating the presence of an extension header.</summary>
            __PROPERTY(bool, extension, Extension);
            /// <summary>Count of contributing source IDs that follow the SSRC.</summary>
            __READONLY_PROPERTY(uint8_t, cc, CSRCCount);
            /// <summary>Flag indicating application-specific behavior.</summary>
            __PROPERTY(bool, marker, Marker);
            /// <summary>Format of the payload contained within the packet.</summary>
            __PROPERTY(uint8_t, payloadType, PayloadType);
            /// <summary>Sequence number for the RTP packet.</summary>
            __PROPERTY(uint16_t, seq, Sequence);
            /// <summary>RTP packet timestamp.</summary>
            __PROPERTY(uint32_t, timestamp, Timestamp);
            /// <summary>Synchronization Source ID.</summary>
            __PROPERTY(uint32_t, ssrc, SSRC);
        
        private:
            static std::chrono::time_point<std::chrono::high_resolution_clock> m_wcStart;
        };
    } // namespace frame
} // namespace network

#endif // __RTP_HEADER_H__