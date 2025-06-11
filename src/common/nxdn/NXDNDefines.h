// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016,2017,2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup nxdn Next Generation Digital Narrowband
 * @brief Implementation for the NXDN standard.
 * @ingroup common
 * 
 * @file NXDNDefines.h
 * @ingroup nxdn
 */
#if !defined(__NXDN_DEFINES_H__)
#define  __NXDN_DEFINES_H__

#include "common/Defines.h"

// Shorthand macro to nxdn::defines -- keeps source code that doesn't use "using" concise
#if !defined(NXDDEF)
#define NXDDEF nxdn::defines
#endif // NXDDEF
namespace nxdn
{
    namespace defines
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup nxdn
         * @{
         */

        /** @name Frame Lengths and Misc Constants */
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
        /** @} */

        /** @name Thresholds */
        /** @brief Default Silence Threshold */
        const uint32_t  DEFAULT_SILENCE_THRESHOLD = 14U;
        /** @brief Default Frame Loss Threshold */
        const uint32_t  DEFAULT_FRAME_LOSS_THRESHOLD = 4U;
        /** @brief Maximum voice frame errors */
        const uint32_t  MAX_NXDN_VOICE_ERRORS = 144U;
        /** @brief Maximum voice frame errors with FACCH stealing */
        const uint32_t  MAX_NXDN_VOICE_ERRORS_STEAL = 94U;
        /** @} */

        /** @brief Link Information Channel - RF Channel Type */
        namespace RFChannelType {
            /** @brief Link Information Channel - RF Channel Type */
            enum E : uint8_t {
                RCCH = 0U,                              //! Control Channel
                RTCH = 1U,                              //! Traffic Channel
                RDCH = 2U,                              //! Data Channel
                RTCH_C = 3U                             //! Composite Control/Traffic Channel
            };
        }

        /** @brief Link Information Channel - Functional Channel Type */
        namespace FuncChannelType {
            /** @brief Link Information Channel - Functional Channel Type */
            enum E : uint8_t {
            // Common Access Channel
                CAC_OUTBOUND = 0U,                      //! Common Access Channel - Outbound
                CAC_INBOUND_LONG = 1U,                  //! Common Access Channel - Inbound Long
                CAC_INBOUND_SHORT = 3U,                 //! Common Access Channel - Inbound Short

            // Slow Associated Control Channel / User Specific Channel
                USC_SACCH_NS = 0U,                      //! Slow Access Control Channel - Non-Superframe
                USC_UDCH = 1U,                          //! 
                USC_SACCH_SS = 2U,                      //! Slow Access Control Channel - Superframe
                USC_SACCH_SS_IDLE = 3U                  //! Slow Access Control Channel - Sueprframe/Idle
            };
        }

        /** @brief Link Information Channel - Channel Options */
        namespace ChOption {
            /** @brief Link Information Channel - Channel Options */
            enum E : uint8_t {
                DATA_NORMAL = 0U,                       //! Normal RCCH Data (Mandatory Data)
                DATA_IDLE = 1U,                         //! Idle RCCH Data (Data that does not need to be Rx)
                DATA_COMMON = 2U,                       //! Common RCCH Data (Optional Data)

                STEAL_NONE = 3U,                        //! No Stealing
                STEAL_FACCH1_2 = 2U,                    //! 2 VCHs second half of FACCH1
                STEAL_FACCH1_1 = 1U,                    //! 2 VCHs first half of FACCH1
                STEAL_FACCH = 0U                        //! FACCH
            };
        }

        /** @brief Common Access Channel - Structure */
        namespace ChStructure {
            /** @brief Common Access Channel - Structure */
            enum E : uint8_t {
                SR_RCCH_SINGLE = 0x00U,                 //! Single Non-Header RCCH Message
                SR_RCCH_DUAL = 0x01U,                   //! Dual Non-Header RCCH Message
                SR_RCCH_HEAD_SINGLE = 0x02U,            //! Single Header RCCH Message
                SR_RCCH_HEAD_DUAL = 0x03U,              //! Dual Header RCCH Message

                SR_SINGLE = 0U,                         //! SACCH Single (same as SR_4_4, kept for clarity)

                SR_4_4 = 0U,                            //! 4/4 SACCH Single/Last
                SR_3_4 = 1U,                            //! 3/4 SACCH
                SR_2_4 = 2U,                            //! 2/4 SACCH
                SR_1_4 = 3U                             //! 1/4 SACCH Header
            };
        }

        /** @name Encryption Algorithms */
        const uint8_t   CIPHER_TYPE_NONE = 0x00U;       //! Unencrypted
        /** @} */

        /** @brief Location Category */
        namespace LocationCategory {
            /** @brief Location Category */
            enum E : uint8_t {
                GLOBAL = 0x00U,                         //! Global
                LOCAL = 0x01U,                          //! Local
                REGIONAL = 0x02U                        //! Regional
            };
        }

        /** @brief Data Response Class */
        namespace PDUResponseClass {
            /** @brief Data Response Class */
            enum : uint8_t
            {
                ACK = 0x00U,                            //! Acknowledge
                ACK_S = 0x01U,                          //!
                NACK = 0x03U                            //! Negative Acknowledge
            };
        }

        /** @brief Cause Responses */
        namespace CauseResponse {
            /** @brief Cause Responses */
            enum : uint8_t {
                RSRC_NOT_AVAIL_NETWORK = 0x51U,         //! Network Resource Not Available
                RSRC_NOT_AVAIL_TEMP = 0x52U,            //! Resource Temporarily Not Available
                RSRC_NOT_AVAIL_QUEUED = 0x53U,          //! Resource Queued Not Available
                SVC_UNAVAILABLE = 0x60U,                //! Service Unavailable
                PROC_ERROR = 0x70U,                     //! Procedure Error - Lack of packet data
                PROC_ERROR_UNDEF = 0x71U,               //! Procedure Error - Invalid packet data

                MM_REG_ACCEPTED = 0x01U,                //! Registration Accepted
                MM_LOC_ACPT_GRP_FAIL = 0x04U,           //! Location Accepted / Group Failed
                MM_LOC_ACPT_GRP_REFUSE = 0x05U,         //! Location Accepted / Group Refused
                MM_REG_FAILED = 0x06U,                  //! Registration Failed
                MM_REG_REFUSED = 0x08U,                 //! Registration Refused

                VD_ACCEPTED = 0x10U,                    //! Voice Accepted
                VD_GRP_NOT_PERM = 0x11U,                //! Voice Group Not Permitted
                VD_REQ_UNIT_NOT_PERM = 0x12U,           //! Requesting Unit Not Permitted
                VD_TGT_UNIT_NOT_PERM = 0x13U,           //! Target Unit Not Permitted
                VD_REQ_UNIT_NOT_REG = 0x1CU,            //! Requesting Unit Not Registered
                VD_QUE_CHN_RESOURCE_NOT_AVAIL = 0x30U,  //! Channel resources unavailable
                VD_QUE_TGT_UNIT_BUSY = 0x38U,           //! Target Unit Busy
                VD_QUE_GRP_BUSY = 0x39U,                //! Group Busy

                SS_ACK_R = 0x01U,                       //! Data Response - ACK Rx Success
                SS_ACK_S = 0x02U,                       //! Data Response - ACK Tx Success
                SS_NACK = 0x08U,                        //! Data Response - NACK (Request Full Retry)
                SS_ACCEPTED = 0x10U,                    //! Data Accepted
                SS_GRP_NOT_PERM = 0x11U,                //! Data Group Not Permitted
                SS_REQ_UNIT_NOT_PERM = 0x12U,           //! Requesting Unit Not Permitted
                SS_TGT_UNIT_NOT_PERM = 0x13U,           //! Target Unit Not Permitted
                SS_REQ_UNIT_NOT_REG = 0x1CU,            //! Requesting Unit Not Registered

                DREQ_USER = 0x10U,                      //! Disconnect by User Request
                DREQ_OTHER = 0x1FU,                     //! Other Disconnect Request

                DISC_USER = 0x10U,                      //! Disconnect by User
                DISC_OTHER = 0x1FU                      //! Other Disconnect
            };
        }

        /** @brief Site Information 1 (SIF1) */
        namespace SiteInformation1 {
            /** @brief Site Information 1 (SIF1) */
            enum : uint8_t {
                DATA_CALL_SVC = 0x01U,                  //! Data Call Service
                VOICE_CALL_SVC = 0x02U,                 //! Voice Call Service
                COMPOSITE_CONTROL = 0x04U,              //! Composite Control Channel
                AUTH_SVC = 0x08U,                       //! Authentication Service
                GRP_REG_SVC = 0x10U,                    //! Group Registration Service
                LOC_REG_SVC = 0x20U,                    //! Location Registration Service
                MULTI_SYSTEM_SVC = 0x40U,               //! Multi-System Service
                MULTI_SITE_SVC = 0x80U                  //! Multi-Site Service
            };
        }

        /** @brief Site Information 2 (SIF2) */
        namespace SiteInformation2 {
            /** @brief Site Information 2 (SIF2) */
            enum : uint8_t {
                IP_NETWORK = 0x10U,                     //! IP Networked
                PSTN_NETWORK = 0x20U,                   //! PSTN Networked
                STATUS_CALL_REM_CTRL = 0x40U,           //! Status Call & Remote Control Service
                SHORT_DATA_CALL_SVC = 0x80U             //! Short Data Call Service
            };
        }

        /** @brief Remote Control Commands */
        namespace RemoteControlCommand {
            /** @brief Remote Control Commands */
            enum : uint8_t {
                STUN = 0x00U,                           //! Stun
                REVIVE = 0x01U,                         //! Revive
                KILL = 0x02U,                           //! Kill
                REMOTE_MONITOR = 0x04U                  //! Remote Monitor
            };
        }

        /** @brief Channel Access - Step */
        namespace ChAccessStep {
            /** @brief Channel Access - Step */
            enum E : uint8_t {
                SYS_DEFINED = 0x00U,                    //! System Defined
                ONEDOT25K = 0x02U,                      //! 1.25khz Step
                THREEDOT125K = 0x03U                    //! 3.125khz Step
            };
        }

        /** @brief Channel Access - Base */
        namespace ChAccessBase {
            /** @brief Channel Access - Base */
            enum E : uint8_t {
                FREQ_100 = 0x01U,                       //! 100mhz Base
                FREQ_330 = 0x02U,                       //! 330mhz Base
                FREQ_400 = 0x03U,                       //! 400mhz Base
                FREQ_750 = 0x04U,                       //! 750mhz Base
                FREQ_SYS_DEFINED = 0x07U                //! System Defined
            };
        }

        /** @brief Call Types */
        namespace CallType {
            /** @brief Call Types */
            enum : uint8_t {
                BROADCAST = 0x00U,                      //! Broadcast
                CONFERENCE = 0x01U,                     //! Conference
                UNSPECIFIED = 0x02U,                    //! Unspecified
                INDIVIDUAL = 0x04U,                     //! Individual
                INTERCONNECT = 0x06U,                   //! Interconnect
                SPEED_DIAL = 0x07U                      //! Speed Dial
            };
        }

        /** @brief Transmission Mode */
        namespace TransmissionMode { 
            /** @brief Transmission Mode */
            enum : uint8_t {
                MODE_4800 = 0x00U,                      //! 4800-baud
                MODE_9600 = 0x02U,                      //! 9600-baud
                MODE_9600_EFR = 0x03U                   //! 9600-baud; should never be used on data calls
            };
        }

        /** @brief Message Types */
        namespace MessageType {
            /** @brief Message Types */
            enum : uint8_t {
            // Common Message Types
                IDLE = 0x10U,                           //! IDLE - Idle
                DISC = 0x11U,                           //! DISC - Disconnect
                DST_ID_INFO = 0x17U,                    //! DST_ID_INFO - Digital Station ID
                SRV_INFO = 0x19U,                       //! SRV_INFO - Service Information
                CCH_INFO = 0x1AU,                       //! CCH_INFO - Control Channel Information
                ADJ_SITE_INFO = 0x1BU,                  //! ADJ_SITE_INFO - Adjacent Site Information
                REM_CON_REQ = 0x34U,                    //! REM_CON_REQ - Remote Control Request
                REM_CON_RESP = 0x35U,                   //! REM_CON_RESP - Remote Control Response

            // Traffic Channel Message Types
                RTCH_VCALL = 0x01U,                     //! VCALL - Voice Call
                RTCH_VCALL_IV = 0x03U,                  //! VCALL_IV - Voice Call Initialization Vector
                RTCH_TX_REL_EX = 0x07U,                 //! TX_REL_EX - Transmission Release Extension
                RTCH_TX_REL = 0x08U,                    //! TX_REL - Transmission Release
                RTCH_DCALL_HDR = 0x09U,                 //! DCALL - Data Call (Header)
                RTCH_DCALL_DATA = 0x0BU,                //! DCALL - Data Call (User Data Format)
                RTCH_DCALL_ACK = 0x0CU,                 //! DCALL_ACK - Data Call Acknowledge
                RTCH_HEAD_DLY = 0x0FU,                  //! HEAD_DLY - Header Delay
                RTCH_SDCALL_REQ_HDR = 0x38U,            //! SDCALL_REQ - Short Data Call Request (Header)
                RTCH_SDCALL_REQ_DATA = 0x39U,           //! SDCALL_REQ - Short Data Call Request (User Data Format)
                RTCH_SDCALL_IV = 0x3AU,                 //! SDCALL_IV - Short Data Call Initialization Vector
                RTCH_SDCALL_RESP = 0x3BU,               //! SDCALL_RESP - Short Data Call Response

            // Control Channel Message Types
                RCCH_VCALL_CONN = 0x03U,                //! VCALL_CONN - Voice Call Connection Request (ISP) / Voice Call Connection Response (OSP)
                RCCH_VCALL_ASSGN = 0x04U,               //! VCALL_ASSGN - Voice Call Assignment
                RCCH_DCALL_ASSGN = 0x14U,               //! DCALL_ASSGN - Data Call Assignment
                RCCH_SITE_INFO = 0x18U,                 //! SITE_INFO - Site Information
                RCCH_REG = 0x20U,                       //! REG - Registration Request (ISP) / Registration Response (OSP)
                RCCH_REG_C = 0x22U,                     //! REG_C - Registration Clear Request (ISP) / Registration Clear Response (OSP)
                RCCH_REG_COMM = 0x23U,                  //! REG_COMM - Registration Command
                RCCH_GRP_REG = 0x24U,                   //! GRP_REG - Group Registration Request (ISP) / Group Registration Response (OSP)
                RCCH_PROP_FORM = 0x3FU                  //! PROP_FORM - Proprietary Form
            };
        }

        /** @} */

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
