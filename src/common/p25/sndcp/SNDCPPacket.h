// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_SNDCP__SNDCPHEADER_H__)
#define  __P25_SNDCP__SNDCPHEADER_H__

#include "common/Defines.h"
#include "common/Utils.h"

#include <string>

namespace p25
{
    namespace sndcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents a SNDCP PDU packet header.
        // ---------------------------------------------------------------------------

        class HOST_SW_API SNDCPPacket {
        public:
            /// <summary>Initializes a copy instance of the SNDCPPacket class.</summary>
            SNDCPPacket(const SNDCPPacket& data);
            /// <summary>Initializes a new instance of the SNDCPPacket class.</summary>
            SNDCPPacket();
            /// <summary>Finalizes a instance of the SNDCPPacket class.</summary>
            ~SNDCPPacket();

            /// <summary>Decode a SNDCP packet.</summary>
            virtual bool decode(const uint8_t* data) = 0;
            /// <summary>Encode a SNDCP packet.</summary>
            virtual void encode(uint8_t* data) = 0;

        public:
            /** Common Data */
            /// <summary>SNDCP PDU Type.</summary>
            __PROTECTED_PROPERTY(uint8_t, pduType, PDUType);
            /// <summary>Link control opcode.</summary>
            __READONLY_PROPERTY(uint8_t, sndcpVersion, SNDCPVersion);
            /// <summary>Network Service Access Point Identifier</summary>
            __PROTECTED_PROPERTY(uint8_t, nsapi, NSAPI);

        protected:
            /// <summary>Internal helper to decode a SNDCP header.</summary>
            bool decodeHeader(const uint8_t* data, bool outbound = false);
            /// <summary>Internal helper to encode a SNDCP header.</summary>
            void encodeHeader(uint8_t* data, bool outbound = false);

            __PROTECTED_COPY(SNDCPPacket);
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCPHEADER_H__
