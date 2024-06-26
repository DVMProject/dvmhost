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
#if !defined(__P25_SNDCP__SNDCPCTXACTACCEPT_H__)
#define  __P25_SNDCP__SNDCPCTXACTACCEPT_H__

#include "common/Defines.h"
#include "common/p25/sndcp/SNDCPPacket.h"
#include "common/Utils.h"

#include <string>

namespace p25
{
    namespace sndcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents a SNDCP PDU context accept response.
        // ---------------------------------------------------------------------------

        class HOST_SW_API SNDCPCtxActAccept : public SNDCPPacket {
        public:
            /// <summary>Initializes a new instance of the SNDCPCtxActAccept class.</summary>
            SNDCPCtxActAccept();

            /// <summary>Decode a SNDCP context activation response.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a SNDCP context activation response.</summary>
            void encode(uint8_t* data);

        public:
            /// <summary>Priority</summary>
            __PROPERTY(uint8_t, priority, Priority);
            /// <summary>Ready Timer</summary>
            __PROPERTY(uint8_t, readyTimer, ReadyTimer);
            /// <summary>Ready Timer</summary>
            __PROPERTY(uint8_t, standbyTimer, StandbyTimer);
            /// <summary>Network Address Type</summary>
            __PROPERTY(uint8_t, nat, NAT);

            /// <summary>IP Address</summary>
            __PROPERTY(ulong64_t, ipAddress, IPAddress);

            /// <summary>MTU</summary>
            __PROPERTY(uint8_t, mtu, MTU);

            __COPY(SNDCPCtxActAccept);
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCPCTXACTACCEPT_H__
