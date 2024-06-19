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
#if !defined(__FSC_CONNECT_RESPONSE_H__)
#define __FSC_CONNECT_RESPONSE_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "frames/FrameDefines.h"
#include "frames/fsc/FSCResponse.h"

namespace p25
{
    namespace dfsi
    {
        namespace fsc
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            //
            // ---------------------------------------------------------------------------

            class HOST_SW_API FSCConnectResponse : public FSCResponse {
            public:
                static const uint8_t LENGTH = 3;

                /// <summary>Initializes a copy instance of the FSCConnectResponse class.</summary>
                FSCConnectResponse();
                /// <summary>Initializes a copy instance of the FSCConnectResponse class.</summary>
                FSCConnectResponse(uint8_t* data);

                /// <summary>Decode a FSC connect response frame.</summary>
                bool decode(const uint8_t* data) override;
                /// <summary>Encode a FSC connect response frame.</summary>
                void encode(uint8_t* data) override;
            
            public:
                /// <summary>Voice Conveyance RTP Port.</summary>
                __PROPERTY(uint16_t, vcBasePort, VCBasePort);
            };
        } // namespace fsc
    } // namespace dfsi
} // namespace p25

#endif // __FSC_CONNECT_RESPONSE_H__