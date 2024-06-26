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
#if !defined(__P25_SNDCP__SNDCPCTXACTREJECT_H__)
#define  __P25_SNDCP__SNDCPCTXACTREJECT_H__

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
        //      Represents a SNDCP PDU context activation reject response.
        // ---------------------------------------------------------------------------

        class HOST_SW_API SNDCPCtxActReject : public SNDCPPacket {
        public:
            /// <summary>Initializes a new instance of the SNDCPCtxActReject class.</summary>
            SNDCPCtxActReject();

            /// <summary>Decode a SNDCP context activation reject packet.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encode a SNDCP context activation reject packet.</summary>
            void encode(uint8_t* data);

        public:
            /// <summary>Reject Code</summary>
            __PROPERTY(uint8_t, rejectCode, RejectCode);

            __COPY(SNDCPCtxActReject);
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCPCTXACTREJECT_H__
