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
#if !defined(__FSC_ACK_H__)
#define __FSC_ACK_H__

#include "Defines.h"
#include "common/Defines.h"
#include "common/Log.h"
#include "common/Utils.h"
#include "frames/FrameDefines.h"
#include "frames/fsc/FSCMessage.h"

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

            class HOST_SW_API FSCACK : public FSCMessage {
            public:
                static const uint8_t LENGTH = 6;

                /// <summary>Initializes a copy instance of the FSCACK class.</summary>
                FSCACK();
                /// <summary>Initializes a copy instance of the FSCACK class.</summary>
                FSCACK(uint8_t* data);

                /// <summary>Decode a FSC ACK frame.</summary>
                bool decode(const uint8_t* data) override;
                /// <summary>Encode a FSC ACK frame.</summary>
                void encode(uint8_t* data) override;
            
            public:
                uint8_t* responseData; // ?? - this should probably be private with getters/setters

                /// <summary>Acknowledged Message ID.</summary>
                __PROPERTY(FSCMessageType::E, ackMessageId, AckMessageId);
                /// <summary>Acknowledged Message Version.</summary>
                __READONLY_PROPERTY(uint8_t, ackVersion, AckVersion);
                /// <summary></summary>
                __READONLY_PROPERTY(uint8_t, ackCorrelationTag, AckCorrelationTag);
                /// <summary>Response code.</summary>
                __PROPERTY(FSCAckResponseCode::E, responseCode, ResponseCode);
                /// <summary>Response Data Length.</summary>
                __PROPERTY(uint8_t, respLength, ResponseLength);
            };
        } // namespace fsc
    } // namespace dfsi
} // namespace p25

#endif // __FSC_ACK_H__