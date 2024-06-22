// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2016,2017,2018 Jonathan Naylor, G4KLX
*   Copyright (C) 2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__NXDN_DEFINES_H__)
#define  __NXDN_DEFINES_H__

#include "common/Defines.h"

// Shorthand macro to nxdn::defines -- keeps source code that doesn't use "using" concise
#define NXDDEF nxdn::defines
namespace nxdn
{
    namespace defines
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        const uint32_t  NXDN_FRAME_LENGTH_BITS = 384U;
        const uint32_t  NXDN_FRAME_LENGTH_BYTES = NXDN_FRAME_LENGTH_BITS / 8U;
        const uint32_t  NXDN_FRAME_LENGTH_SYMBOLS = NXDN_FRAME_LENGTH_BITS / 2U;

        const uint32_t  NXDN_FSW_LENGTH_BITS = 20U;
        const uint32_t  NXDN_FSW_LENGTH_SYMBOLS = NXDN_FSW_LENGTH_BITS / 2U;

        const uint8_t   NXDN_FSW_BYTES[] = { 0xCDU, 0xF5U, 0x90U };
        const uint8_t   NXDN_FSW_BYTES_MASK[] = { 0xFFU, 0xFFU, 0xF0U };
        const uint32_t  NXDN_FSW_BYTES_LENGTH = 3U;

        const uint8_t   NXDN_PREAMBLE[] = { 0x57U, 0x75U, 0xFDU };

        const uint32_t  NXDN_LICH_LENGTH_BITS = 16U;
        const uint32_t  NXDN_LICH_LENGTH_BYTES = NXDN_LICH_LENGTH_BITS / 8U;

        const uint32_t  NXDN_SACCH_FEC_LENGTH_BITS = 60U;                                               // Puncture and Interleave Length
        const uint32_t  NXDN_SACCH_FEC_LENGTH_BYTES = (NXDN_SACCH_FEC_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_SACCH_FEC_CONV_LENGTH_BITS = 72U;                                          // Convolution Length
        const uint32_t  NXDN_SACCH_FEC_CONV_LENGTH_BYTES = NXDN_SACCH_FEC_CONV_LENGTH_BITS / 8U;
        const uint32_t  NXDN_SACCH_CRC_LENGTH_BITS = 36U;                                               // Data + CRC-6 + 4-bit NULL
        const uint32_t  NXDN_SACCH_CRC_LENGTH_BYTES = (NXDN_SACCH_CRC_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_SACCH_LENGTH_BITS = 26U;

        const uint32_t  NXDN_FACCH1_FEC_LENGTH_BITS = 144U;                                             // Puncture and Interleave Length
        const uint32_t  NXDN_FACCH1_FEC_LENGTH_BYTES = NXDN_FACCH1_FEC_LENGTH_BITS / 8U;
        const uint32_t  NXDN_FACCH1_FEC_CONV_LENGTH_BITS = 192U;                                        // Convolution Length
        const uint32_t  NXDN_FACCH1_FEC_CONV_LENGTH_BYTES = NXDN_FACCH1_FEC_CONV_LENGTH_BITS / 8U;
        const uint32_t  NXDN_FACCH1_CRC_LENGTH_BITS = 96U;                                              // Data + CRC-12 + 4-bit NULL
        const uint32_t  NXDN_FACCH1_CRC_LENGTH_BYTES = NXDN_FACCH1_CRC_LENGTH_BITS / 8U;
        const uint32_t  NXDN_FACCH1_LENGTH_BITS = 80U;

        const uint32_t  NXDN_UDCH_FEC_LENGTH_BITS = 348U;                                               // Puncture and Interleave Length
        const uint32_t  NXDN_UDCH_FEC_LENGTH_BYTES = (NXDN_UDCH_FEC_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_UDCH_FEC_CONV_LENGTH_BITS = 406U;                                          // Convolution Length
        const uint32_t  NXDN_UDCH_FEC_CONV_LENGTH_BYTES = (NXDN_UDCH_FEC_CONV_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_UDCH_CRC_LENGTH_BITS = 203U;                                               // Data + CRC-15 + 4-bit NULL
        const uint32_t  NXDN_UDCH_CRC_LENGTH_BYTES = (NXDN_UDCH_CRC_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_UDCH_LENGTH_BITS = 184U;

        const uint32_t  NXDN_CAC_FEC_LENGTH_BITS = 300U;                                                // Puncture and Interleave Length
        const uint32_t  NXDN_CAC_FEC_LENGTH_BYTES = (NXDN_CAC_FEC_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_CAC_FEC_CONV_LENGTH_BITS = 350U;                                           // Convolution Length
        const uint32_t  NXDN_CAC_FEC_CONV_LENGTH_BYTES = (NXDN_CAC_FEC_CONV_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_CAC_CRC_LENGTH_BITS = 175U;                                                // Data + CRC-16 + 4-bit NULL
        const uint32_t  NXDN_CAC_CRC_LENGTH_BYTES = (NXDN_CAC_CRC_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_CAC_LENGTH_BITS = 155U;
        const uint32_t  NXDN_CAC_E_POST_FIELD_BITS = 24U;

        const uint32_t  NXDN_CAC_IN_FEC_LENGTH_BITS = 252U;                                             // Puncture and Interleave Length
        const uint32_t  NXDN_CAC_IN_FEC_LENGTH_BYTES = (NXDN_CAC_IN_FEC_LENGTH_BITS / 8U) + 1U;
        const uint32_t  NXDN_CAC_LONG_FEC_CONV_LENGTH_BITS = 312U;                                      // Convolution Length
        const uint32_t  NXDN_CAC_LONG_FEC_CONV_LENGTH_BYTES = NXDN_CAC_LONG_FEC_CONV_LENGTH_BITS / 8U;
        const uint32_t  NXDN_CAC_LONG_CRC_LENGTH_BITS = 156U;                                           // Data + CRC-16 + 4-bit NULL
        const uint32_t  NXDN_CAC_LONG_LENGTH_BITS = 136U;
        const uint32_t  NXDN_CAC_SHORT_CRC_LENGTH_BITS = 126U;                                          // Data + CRC-16 + 4-bit NULL
        const uint32_t  NXDN_CAC_SHORT_LENGTH_BITS = 106U;

        const uint32_t  NXDN_RTCH_LC_LENGTH_BITS = 176U;
        const uint32_t  NXDN_RTCH_LC_LENGTH_BYTES = (NXDN_RTCH_LC_LENGTH_BITS / 8U);

        const uint32_t  NXDN_RCCH_LC_LENGTH_BITS = 144U;
        const uint32_t  NXDN_RCCH_LC_LENGTH_BYTES = (NXDN_RCCH_LC_LENGTH_BITS / 8U);
        const uint32_t  NXDN_RCCH_CAC_LC_LONG_LENGTH_BITS = 128U;
        const uint32_t  NXDN_RCCH_CAC_LC_SHORT_LENGTH_BITS = 96U;

        const uint32_t  NXDN_FSW_LICH_SACCH_LENGTH_BITS  = NXDN_FSW_LENGTH_BITS + NXDN_LICH_LENGTH_BITS + NXDN_SACCH_FEC_LENGTH_BITS;
        const uint32_t  NXDN_FSW_LICH_SACCH_LENGTH_BYTES = NXDN_FSW_LICH_SACCH_LENGTH_BITS / 8U;

        const uint8_t   SACCH_IDLE[] = { 0x10U, 0x00U, 0x00U };

        const uint8_t   NULL_AMBE[] = { 0xB1U, 0xA8U, 0x22U, 0x25U, 0x6BU, 0xD1U, 0x6CU, 0xCFU, 0x67U };

        const uint8_t   CALLSIGN_LENGTH_BYTES = 8U;

        const uint32_t  MI_LENGTH_BYTES = 8U;
        const uint32_t  PCKT_INFO_LENGTH_BYTES = 3U;

        /** Thresholds */
        const uint32_t  DEFAULT_SILENCE_THRESHOLD = 14U;
        const uint32_t  DEFAULT_FRAME_LOSS_THRESHOLD = 4U;
        const uint32_t  MAX_NXDN_VOICE_ERRORS = 144U;
        const uint32_t  MAX_NXDN_VOICE_ERRORS_STEAL = 94U;

        /// <summary>
        /// Link Information Channel - RF Channel Type
        /// </summary>
        namespace RFChannelType {
            // RF Channel Type Enumeration
            enum E : uint8_t {
                RCCH = 0U,                              // Control Channel
                RTCH = 1U,                              // Traffic Channel
                RDCH = 2U,                              // Data Channel
                RTCH_C = 3U                             // Composite Control/Traffic Channel
            };
        }

        /// <summary>
        /// Link Information Channel - Functional Channel Type
        /// </summary>
        namespace FuncChannelType {
            // Functional Channel Type Enumeration
            enum E : uint8_t {
            // Common Access Channel
                CAC_OUTBOUND = 0U,                      // Common Access Channel - Outbound
                CAC_INBOUND_LONG = 1U,                  // Common Access Channel - Inbound Long
                CAC_INBOUND_SHORT = 3U,                 // Common Access Channel - Inbound Short

            // Slow Associated Control Channel / User Specific Channel
                USC_SACCH_NS = 0U,                      //
                USC_UDCH = 1U,                          //
                USC_SACCH_SS = 2U,                      //
                USC_SACCH_SS_IDLE = 3U                  //
            };
        }

        /// <summary>
        /// Link Information Channel - Channel Options
        /// </summary>
        namespace ChOption {
            // Channel Option(s) Enumeration
            enum E : uint8_t {
                DATA_NORMAL = 0U,                       //
                DATA_IDLE = 1U,                         //
                DATA_COMMON = 2U,                       //

                STEAL_NONE = 3U,                        //
                STEAL_FACCH1_2 = 2U,                    //
                STEAL_FACCH1_1 = 1U,                    //
                STEAL_FACCH = 0U                        //
            };
        }

        /// <summary>
        /// Common Access Channel - Structure
        /// </summary>
        namespace ChStructure {
            // Channel Structure Enumeration
            enum E : uint8_t {
                SR_RCCH_SINGLE = 0x00U,                 //
                SR_RCCH_DUAL = 0x01U,                   //
                SR_RCCH_HEAD_SINGLE = 0x02U,            //
                SR_RCCH_HEAD_DUAL = 0x03U,              //

                SR_SINGLE = 0U,                         //
                SR_4_4 = 0U,                            //
                SR_3_4 = 1U,                            //
                SR_2_4 = 2U,                            //
                SR_1_4 = 3U                             //
            };
        }

        /** Encryption Algorithms */
        const uint8_t   CIPHER_TYPE_NONE = 0x00U;       // Unencrypted

        /// <summary>
        /// Location Category
        /// <summary>
        namespace LocationCategory {
            // Location Category Enumeration
            enum E : uint8_t {
                GLOBAL = 0x00U,                         // Global
                LOCAL = 0x01U,                          // Local
                REGIONAL = 0x02U                        // Regional
            };
        }

        /// <summary>
        /// Data Response Class
        /// <summary>
        namespace PDUResponseClass {
            enum : uint8_t
            {
                ACK = 0x00U,                            // Acknowledge
                ACK_S = 0x01U,                          //
                NACK = 0x03U                            // Negative Acknowledge
            };
        }

        /// <summary>
        /// Causes
        /// <summary>
        namespace CauseResponse {
            enum : uint8_t {
                RSRC_NOT_AVAIL_NETWORK = 0x51U,         // Network Resource Not Available
                RSRC_NOT_AVAIL_TEMP = 0x52U,            // Resource Temporarily Not Available
                RSRC_NOT_AVAIL_QUEUED = 0x53U,          // Resource Queued Not Available
                SVC_UNAVAILABLE = 0x06U,                // Service Unavailable
                PROC_ERROR = 0x70U,                     //
                PROC_ERROR_UNDEF = 0x71U,               //

                MM_REG_ACCEPTED = 0x01U,                // Registration Accepted
                MM_LOC_ACPT_GRP_FAIL = 0x04U,           // Location Accepted / Group Failed
                MM_LOC_ACPT_GRP_REFUSE = 0x04U,         // Location Accepted / Group Refused
                MM_REG_FAILED = 0x06U,                  // Registration Failed
                MM_REG_REFUSED = 0x08U,                 // Registration Refused

                VD_ACCEPTED = 0x10U,                    //
                VD_GRP_NOT_PERM = 0x11U,                //
                VD_REQ_UNIT_NOT_PERM = 0x12U,           //
                VD_TGT_UNIT_NOT_PERM = 0x13U,           //
                VD_REQ_UNIT_NOT_REG = 0x1CU,            //
                VD_QUE_CHN_RESOURCE_NOT_AVAIL = 0x30U,  //
                VD_QUE_TGT_UNIT_BUSY = 0x38U,           //
                VD_QUE_GRP_BUSY = 0x39U,                //

                SS_ACK_R = 0x01U,                       //
                SS_ACK_S = 0x02U,                       //
                SS_NACK = 0x08U,                        //
                SS_ACCEPTED = 0x10U,                    //
                SS_GRP_NOT_PERM = 0x11U,                //
                SS_REQ_UNIT_NOT_PERM = 0x12U,           //
                SS_TGT_UNIT_NOT_PERM = 0x13U,           //
                SS_REQ_UNIT_NOT_REG = 0x1CU,            //

                DREQ_USER = 0x10U,                      //
                DREQ_OTHER = 0x1FU,                     //

                DISC_USER = 0x10U,                      //
                DISC_OTHER = 0x1FU                      //
            };
        }

        /// <summary>
        /// Site Information 1 (SIF1)
        /// <summary>
        namespace SiteInformation1 {
            enum : uint8_t {
                DATA_CALL_SVC = 0x01U,                  // Data Call Service
                VOICE_CALL_SVC = 0x02U,                 // Voice Call Service
                COMPOSITE_CONTROL = 0x04U,              // Composite Control Channel
                AUTH_SVC = 0x08U,                       // Authentication Service
                GRP_REG_SVC = 0x10U,                    // Group Registration Service
                LOC_REG_SVC = 0x20U,                    // Location Registration Service
                MULTI_SYSTEM_SVC = 0x40U,               // Multi-System Service
                MULTI_SITE_SVC = 0x80U                  // Multi-Site Service
            };
        }

        /// <summary>
        /// Site Information 2 (SIF2)
        /// <summary>
        namespace SiteInformation2 {
            enum : uint8_t {
                IP_NETWORK = 0x10U,                     // IP Networked
                PSTN_NETWORK = 0x20U,                   // PSTN Networked
                STATUS_CALL_REM_CTRL = 0x40U,           //
                SHORT_DATA_CALL_SVC = 0x80U             //
            };
        }

        /// <summary>
        /// Channel Access - Step
        /// <summary>
        namespace ChAccessStep {
            // Channel Access Step Enumeration
            enum E : uint8_t {
                SYS_DEFINED = 0x00U,                    // System Defined
                ONEDOT25K = 0x02U,                      //
                THREEDOT125K = 0x03U                    //
            };
        }

        /// <summary>
        /// Channel Access - Base
        /// <summary>
        namespace ChAccessBase {
            // Channel Access Base Enumeration
            enum E : uint8_t {
                FREQ_100 = 0x01U,                       //
                FREQ_330 = 0x02U,                       //
                FREQ_400 = 0x03U,                       //
                FREQ_750 = 0x04U,                       //
                FREQ_SYS_DEFINED = 0x07U                //
            };
        }

        /// <summary>
        /// Call Types
        /// <summary>
        namespace CallType {
            enum : uint8_t {
                BROADCAST = 0x00U,                      // Broadcast
                CONFERENCE = 0x01U,                     // Conference
                UNSPECIFIED = 0x02U,                    // Unspecified
                INDIVIDUAL = 0x04U,                     // Individual
                INTERCONNECT = 0x06U,                   // Interconnect
                SPEED_DIAL = 0x07U                      // Speed Dial
            };
        }

        /// <summary>
        /// Transmission Mode
        /// <summary>
        namespace TransmissionMode { 
            enum : uint8_t {
                MODE_4800 = 0x00U,                      // 4800-baud
                MODE_9600 = 0x02U,                      // 9600-baud
                MODE_9600_EFR = 0x03U                   // 9600-baud; should never be used on data calls
            };
        }

        /// <summary>
        /// Message Types
        /// <summary>
        namespace MessageType {
            enum : uint8_t {
            // Common Message Types
                IDLE = 0x10U,                           // IDLE - Idle
                DISC = 0x11U,                           // DISC - Disconnect
                DST_ID_INFO = 0x17U,                    // DST_ID_INFO - Digital Station ID
                SRV_INFO = 0x19U,                       // SRV_INFO - Service Information
                CCH_INFO = 0x1AU,                       // CCH_INFO - Control Channel Information
                ADJ_SITE_INFO = 0x1BU,                  // ADJ_SITE_INFO - Adjacent Site Information

            // Traffic Channel Message Types
                RTCH_VCALL = 0x01U,                     // VCALL - Voice Call
                RTCH_VCALL_IV = 0x03U,                  // VCALL_IV - Voice Call Initialization Vector
                RTCH_TX_REL_EX = 0x07U,                 // TX_REL_EX - Transmission Release Extension
                RTCH_TX_REL = 0x08U,                    // TX_REL - Transmission Release
                RTCH_DCALL_HDR = 0x09U,                 // DCALL - Data Call (Header)
                RTCH_DCALL_DATA = 0x0BU,                // DCALL - Data Call (User Data Format)
                RTCH_DCALL_ACK = 0x0CU,                 // DCALL_ACK - Data Call Acknowledge
                RTCH_HEAD_DLY = 0x0FU,                  // HEAD_DLY - Header Delay
                RTCH_SDCALL_REQ_HDR = 0x38U,            // SDCALL_REQ - Short Data Call Request (Header)
                RTCH_SDCALL_REQ_DATA = 0x39U,           // SDCALL_REQ - Short Data Call Request (User Data Format)
                RTCH_SDCALL_IV = 0x3AU,                 // SDCALL_IV - Short Data Call Initialization Vector
                RTCH_SDCALL_RESP = 0x3BU,               // SDCALL_RESP - Short Data Call Response

            // Control Channel Message Types
                RCCH_VCALL_CONN = 0x03U,                // VCALL_CONN - Voice Call Connection Request (ISP) / Voice Call Connection Response (OSP)
                RCCH_VCALL_ASSGN = 0x04U,               // VCALL_ASSGN - Voice Call Assignment
                RCCH_DCALL_ASSGN = 0x14U,               // DCALL_ASSGN - Data Call Assignment
                RCCH_SITE_INFO = 0x18U,                 // SITE_INFO - Site Information
                RCCH_REG = 0x20U,                       // REG - Registration Request (ISP) / Registration Response (OSP)
                RCCH_REG_C = 0x22U,                     // REG_C - Registration Clear Request (ISP) / Registration Clear Response (OSP)
                RCCH_REG_COMM = 0x23U,                  // REG_COMM - Registration Command
                RCCH_GRP_REG = 0x24U,                   // GRP_REG - Group Registration Request (ISP) / Group Registration Response (OSP)
                RCCH_PROP_FORM = 0x3FU                  // PROP_FORM - Proprietary Form
            };
        }

    #define NXDN_RTCH_MSG_TYPE_VCALL        "VCALL (Voice Call)"
    #define NXDN_RTCH_MSG_TYPE_VCALL_RESP   "VCALL_RESP (Voice Call Response)"
    #define NXDN_RTCH_MSG_TYPE_TX_REL       "TX_REL (Transmission Release)"
    #define NXDN_RTCH_MSG_TYPE_DCALL_HDR    "DCALL (Data Call Header)"
    } // namespace defines
} // namespace nxdn

// ---------------------------------------------------------------------------
//  Namespace Prototypes
// ---------------------------------------------------------------------------
namespace edac { }
namespace nxdn
{
    namespace edac
    {
        using namespace ::edac;
    } // namespace edac
} // namespace nxdn

#endif // __NXDN_DEFINES_H__
