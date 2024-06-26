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
#if !defined(__P25_SNDCP__SNDCP_FACTORY_H__)
#define  __P25_SNDCP__SNDCP_FACTORY_H__

#include "common/Defines.h"

#include "common/p25/sndcp/SNDCPPacket.h"
#include "common/p25/sndcp/SNDCPCtxActAccept.h"
#include "common/p25/sndcp/SNDCPCtxActReject.h"
#include "common/p25/sndcp/SNDCPCtxActRequest.h"
#include "common/p25/sndcp/SNDCPCtxDeactivation.h"

namespace p25
{
    namespace sndcp
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Helper class to instantiate an instance of a SNDCP packet.
        // ---------------------------------------------------------------------------

        class HOST_SW_API SNDCPFactory {
        public:
            /// <summary>Initializes a new instance of the SNDCPFactory class.</summary>
            SNDCPFactory();
            /// <summary>Finalizes a instance of the SNDCPFactory class.</summary>
            ~SNDCPFactory();

            /// <summary>Create an instance of a SNDCPPacket.</summary>
            static std::unique_ptr<SNDCPPacket> create(const uint8_t* data);

        private:
            /// <summary></summary>
            static std::unique_ptr<SNDCPPacket> decode(SNDCPPacket* packet, const uint8_t* data);
        };
    } // namespace sndcp
} // namespace p25

#endif // __P25_SNDCP__SNDCP_FACTORY_H__
