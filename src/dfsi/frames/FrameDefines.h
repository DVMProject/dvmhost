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
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__FRAME_DEFINES_H__)
#define  __FRAME_DEFINES_H__

#include "common/Defines.h"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /// <summary>
        /// Control Service Message.
        /// </summary>
        enum FSCMessageType {
            /// <summary>
            /// Establish connection with FSS.
            /// </summary>
            FSC_CONNECT = 0,

            /// <summary>
            /// Heartbeat/Connectivity Maintenance.
            /// </summary>
            FSC_HEARTBEAT = 1,
            /// <summary>
            /// Control Service Ack.
            /// </summary>
            FSC_ACK = 2,

            /// <summary>
            /// Detach Control Service.
            /// </summary>
            FSC_DISCONNECT = 9,

            /// <summary>
            /// Invalid Control Message.
            /// </summary>
            FSC_INVALID = 127,
        };

        /// <summary>
        /// ACK/NAK Codes
        /// </summary>
        enum FSCAckResponseCode {
            /// <summary>
            /// Acknowledgement.
            /// </summary>
            CONTROL_ACK = 0,
            /// <summary>
            /// Unspecified Negative Acknowledgement.
            /// </summary>
            CONTROL_NAK = 1,
            /// <summary>
            /// Server is connected to some other host.
            /// </summary>
            CONTROL_NAK_CONNECTED = 2,
            /// <summary>
            /// Unsupported Manufactuerer Message.
            /// </summary>
            CONTROL_NAK_M_UNSUPP = 3,
            /// <summary>
            /// Unsupported Message Version.
            /// </summary>
            CONTROL_NAK_V_UNSUPP = 4,
            /// <summary>
            /// Unsupported Function.
            /// </summary>
            CONTROL_NAK_F_UNSUPP = 5,
            /// <summary>
            /// Bad / Unsupported Command Parameters.
            /// </summary>
            CONTROL_NAK_PARMS = 6,
            /// <summary>
            /// FSS is currently busy with a function.
            /// </summary>
            CONTROL_NAK_BUSY = 7
        };

        /// <summary>
        /// DFSI Block Types
        /// </summary>
        enum BlockType {
            FULL_RATE_VOICE = 0,

            VOICE_HEADER_P1 = 6,
            VOICE_HEADER_P2 = 7,

            START_OF_STREAM = 9,
            END_OF_STREAM = 10,

            UNDEFINED = 127
        };

        /// <summary>
        /// 
        /// </summary>
        enum RTFlag {
            ENABLED = 0x02U,
            DISABLED = 0x04U
        };

        /// <summary>
        /// 
        /// </summary>
        enum StartStopFlag {
            START = 0x0CU,
            STOP = 0x25U
        };

        /// <summary>
        /// 
        /// </summary>
        enum StreamTypeFlag {
            VOICE = 0x0BU
        };

        /// <summary>
        /// 
        /// </summary>
        enum RssiValidityFlag {
            INVALID = 0x00U,
            VALID = 0x1A
        };

        /// <summary>
        /// 
        /// </summary>
        enum SourceFlag {
            SOURCE_DIU = 0x00U,
            SOURCE_QUANTAR = 0x02U
        };

        /// <summary>
        /// 
        /// </summary>
        enum ICWFlag {
            ICW_DIU = 0x00U,
            ICW_QUANTAR = 0x1B
        };
    } // namespace dfsi
} // namespace p25

#endif // __FRAME_DEFINES_H__
