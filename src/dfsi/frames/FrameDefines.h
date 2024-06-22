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
        namespace FSCMessageType {
            // FSC Control Service Message Enumeration
            enum E : uint8_t {
                FSC_CONNECT = 0,                    // Establish connection with FSS.
                FSC_HEARTBEAT = 1,                  // Heartbeat/Connectivity Maintenance.
                FSC_ACK = 2,                        // Control Service Ack.

                FSC_DISCONNECT = 9,                 // Detach Control Service.

                FSC_INVALID = 127,                  // Invalid Control Message.
            };
        }

        /// <summary>
        /// ACK/NAK Codes
        /// </summary>
        namespace FSCAckResponseCode {
            // FSC ACK/NAK Code Enumeration
            enum E : uint8_t {
                CONTROL_ACK = 0,                    // Acknowledgement.
                CONTROL_NAK = 1,                    // Unspecified Negative Acknowledgement.
                CONTROL_NAK_CONNECTED = 2,          // Server is connected to some other host.
                CONTROL_NAK_M_UNSUPP = 3,           // Unsupported Manufactuerer Message.
                CONTROL_NAK_V_UNSUPP = 4,           // Unsupported Message Version.
                CONTROL_NAK_F_UNSUPP = 5,           // Unsupported Function.
                CONTROL_NAK_PARMS = 6,              // Bad / Unsupported Command Parameters.
                CONTROL_NAK_BUSY = 7                // FSS is currently busy with a function.
            };
        }

        /// <summary>
        /// DFSI Block Types
        /// </summary>
        namespace BlockType {
            // DFSI Block Types Enumeration
            enum E : uint8_t {
                FULL_RATE_VOICE = 0,                //

                VOICE_HEADER_P1 = 6,                //
                VOICE_HEADER_P2 = 7,                //

                START_OF_STREAM = 9,                //
                END_OF_STREAM = 10,                 //

                UNDEFINED = 127                     //
            };
        }

        /// <summary>
        /// 
        /// </summary>
        namespace RTFlag {
            // 
            enum E : uint8_t {
                ENABLED = 0x02U,                    //
                DISABLED = 0x04U                    //
            };
        }

        /// <summary>
        /// 
        /// </summary>
        namespace StartStopFlag {
            // 
            enum E : uint8_t {
                START = 0x0CU,                      //
                STOP = 0x25U                        //
            };
        }

        /// <summary>
        /// 
        /// </summary>
        namespace StreamTypeFlag {
            // 
            enum E : uint8_t {
                VOICE = 0x0BU                       //
            };
        }

        /// <summary>
        /// 
        /// </summary>
        namespace RssiValidityFlag {
            // 
            enum E : uint8_t {
                INVALID = 0x00U,                    //
                VALID = 0x1A                        //
            };
        }

        /// <summary>
        /// 
        /// </summary>
        namespace SourceFlag {
            // 
            enum E : uint8_t {
                DIU = 0x00U,                        //
                QUANTAR = 0x02U                     //
            };
        }

        /// <summary>
        /// 
        /// </summary>
        namespace ICWFlag {
            // 
            enum E : uint8_t {
                DIU = 0x00U,                        //
                QUANTAR = 0x1B                      //
            };
        }
    } // namespace dfsi
} // namespace p25

#endif // __FRAME_DEFINES_H__
