// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup dfsi_frames DFSI Data Frames
 * @brief Implementation for the DFSI data frames.
 * @ingroup dfsi
 *
 * @defgroup dfsi_fsc_frames DFSI Control Frames
 * @brief Implementation for the DFSI control frames.
 * @ingroup dfsi_frames
 * 
 * @file FrameDefines.h
 * @ingroup dfsi_frames
 */
#if !defined(__FRAME_DEFINES_H__)
#define  __FRAME_DEFINES_H__

#include "common/Defines.h"

namespace p25
{
    namespace dfsi
    {
        namespace frames
        {
            // ---------------------------------------------------------------------------
            //  Constants
            // ---------------------------------------------------------------------------

            /**
             * @addtogroup dfsi_frames
             * @{
             */

            /** @brief FSC Control Service Message.*/
            namespace FSCMessageType {
                /** @brief FSC Control Service Message.*/
                enum E : uint8_t {
                    FSC_CONNECT = 0,                    //! Establish connection with FSS
                    FSC_HEARTBEAT = 1,                  //! Heartbeat/Connectivity Maintenance
                    FSC_ACK = 2,                        //! Control Service Ack.

                    FSC_SEL_CHAN = 5,                   //! Channel Selection

                    FSC_REPORT_SEL_MODES = 8,           //! Report Selected Modes

                    FSC_DISCONNECT = 9,                 //! Detach Control Service

                    FSC_INVALID = 127,                  //! Invalid Control Message
                };
            }

            /** @brief FSC ACK/NAK Codes. */
            namespace FSCAckResponseCode {
                /** @brief FSC ACK/NAK Codes. */
                enum E : uint8_t {
                    CONTROL_ACK = 0,                    //! Acknowledgement.
                    CONTROL_NAK = 1,                    //! Unspecified Negative Acknowledgement.
                    CONTROL_NAK_CONNECTED = 2,          //! Server is connected to some other host.
                    CONTROL_NAK_M_UNSUPP = 3,           //! Unsupported Manufactuerer Message.
                    CONTROL_NAK_V_UNSUPP = 4,           //! Unsupported Message Version.
                    CONTROL_NAK_F_UNSUPP = 5,           //! Unsupported Function.
                    CONTROL_NAK_PARMS = 6,              //! Bad / Unsupported Command Parameters.
                    CONTROL_NAK_BUSY = 7                //! FSS is currently busy with a function.
                };
            }

            /** @brief DFSI Block Types */
            namespace BlockType {
                /** @brief DFSI Block Types */
                enum E : uint8_t {
                    FULL_RATE_VOICE = 0,                //! Full Rate Voice

                    VOICE_HEADER_P1 = 6,                //! Voice Header 1
                    VOICE_HEADER_P2 = 7,                //! Voice Header 2

                    START_OF_STREAM = 9,                //! Start of Stream
                    END_OF_STREAM = 10,                 //! End of Stream

                    START_OF_STREAM_ACK = 14,           //! Start of Stream Ack

                    UNDEFINED = 127                     //! Undefined
                };
            }

            /** @brief Motorola Start of Stream Operation */
            namespace MotStartStreamOpcode {
                /** @brief Motorola Start of Stream Operation */
                enum E : uint8_t {
                    TRANSMIT = 0x02U,                   //! Transmit
                    RECEIVE = 0x04U,                    //! Receive
                };
            }

            /** @brief Motorola Stream Payload */
            namespace MotStreamPayload {
                /** @brief Motorola Stream Payload */
                enum E : uint8_t {
                    VOICE = 0x0BU,                      //! P25 Voice
                    DATA = 0x0CU,                       //! P25 Data
                    TERM_LC = 0x0EU,                    //! P25 Termination Link Control
                    TSBK = 0x0FU                        //! P25 TSBK
                };
            }
            /** @} */
        } // namespace frames
    } // namespace dfsi
} // namespace p25

#endif // __FRAME_DEFINES_H__
