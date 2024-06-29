// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2019-2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup dmr Digital Mobile Radio
 * @brief Implementation for the ETSI TS-102 Digital Mobile Radio (DMR) standard.
 * @ingroup common
 * 
 * @file DMRDefines.h
 * @ingroup dmr
 */
#if !defined(__DMR_DEFINES_H__)
#define __DMR_DEFINES_H__

#include "common/Defines.h"

// Shorthand macro to dmr::defines -- keeps source code that doesn't use "using" concise
#define DMRDEF dmr::defines
namespace dmr
{
    namespace defines
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup dmr
         * @{
         */

        /** @name Frame Lengths and Misc Constants */
        const uint32_t  DMR_FRAME_LENGTH_BITS = 264U;
        const uint32_t  DMR_FRAME_LENGTH_BYTES = 33U;

        const uint32_t  DMR_SYNC_LENGTH_BITS = 48U;
        const uint32_t  DMR_SYNC_LENGTH_BYTES = 6U;

        const uint32_t  DMR_EMB_LENGTH_BITS = 8U;
        const uint32_t  DMR_EMB_LENGTH_BYTES = 1U;

        const uint32_t  DMR_SLOT_TYPE_LENGTH_BITS = 8U;
        const uint32_t  DMR_SLOT_TYPE_LENGTH_BYTES = 1U;

        const uint32_t  DMR_EMBEDDED_SIGNALLING_LENGTH_BITS = 32U;
        const uint32_t  DMR_EMBEDDED_SIGNALLING_LENGTH_BYTES = 4U;

        const uint32_t  DMR_AMBE_LENGTH_BITS = 108U * 2U;
        const uint32_t  DMR_AMBE_LENGTH_BYTES = 27U;

        const uint32_t  DMR_LC_HEADER_LENGTH_BYTES = 12U;
        const uint32_t  DMR_CSBK_LENGTH_BYTES = 12U;

        const uint8_t   BS_SOURCED_AUDIO_SYNC[] = { 0x07U, 0x55U, 0xFDU, 0x7DU, 0xF7U, 0x5FU, 0x70U };
        const uint8_t   BS_SOURCED_DATA_SYNC[] = { 0x0DU, 0xFFU, 0x57U, 0xD7U, 0x5DU, 0xF5U, 0xD0U };

        const uint8_t   MS_SOURCED_AUDIO_SYNC[] = { 0x07U, 0xF7U, 0xD5U, 0xDDU, 0x57U, 0xDFU, 0xD0U };
        const uint8_t   MS_SOURCED_DATA_SYNC[] = { 0x0DU, 0x5DU, 0x7FU, 0x77U, 0xFDU, 0x75U, 0x70U };

        const uint8_t   DIRECT_SLOT1_AUDIO_SYNC[] = { 0x05U, 0xD5U, 0x77U, 0xF7U, 0x75U, 0x7FU, 0xF0U };
        const uint8_t   DIRECT_SLOT1_DATA_SYNC[] = { 0x0FU, 0x7FU, 0xDDU, 0x5DU, 0xDFU, 0xD5U, 0x50U };

        const uint8_t   DIRECT_SLOT2_AUDIO_SYNC[] = { 0x07U, 0xDFU, 0xFDU, 0x5FU, 0x55U, 0xD5U, 0xF0U };
        const uint8_t   DIRECT_SLOT2_DATA_SYNC[] = { 0x0DU, 0x75U, 0x57U, 0xF5U, 0xFFU, 0x7FU, 0x50U };

        const uint8_t   MS_DATA_SYNC_BYTES[] = { 0x0DU, 0x5DU, 0x7FU, 0x77U, 0xFDU, 0x75U, 0x70U };
        const uint8_t   MS_VOICE_SYNC_BYTES[] = { 0x07U, 0xF7U, 0xD5U, 0xDDU, 0x57U, 0xDFU, 0xD0U };

        const uint8_t   SYNC_MASK[] = { 0x0FU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xF0U };

        // The PR FILL and Data Sync pattern.
        const uint8_t   IDLE_DATA[] = { 0x01U, 0x00U,
            0x53U, 0xC2U, 0x5EU, 0xABU, 0xA8U, 0x67U, 0x1DU, 0xC7U, 0x38U, 0x3BU, 0xD9U,
            0x36U, 0x00U, 0x0DU, 0xFFU, 0x57U, 0xD7U, 0x5DU, 0xF5U, 0xD0U, 0x03U, 0xF6U,
            0xE4U, 0x65U, 0x17U, 0x1BU, 0x48U, 0xCAU, 0x6DU, 0x4FU, 0xC6U, 0x10U, 0xB4U };

        // A silence frame only
        const uint8_t   SILENCE_DATA[] = { 0x01U, 0x00U,
            0xB9U, 0xE8U, 0x81U, 0x52U, 0x61U, 0x73U, 0x00U, 0x2AU, 0x6BU, 0xB9U, 0xE8U,
            0x81U, 0x52U, 0x60U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x73U, 0x00U,
            0x2AU, 0x6BU, 0xB9U, 0xE8U, 0x81U, 0x52U, 0x61U, 0x73U, 0x00U, 0x2AU, 0x6BU };

        const uint8_t   NULL_AMBE[] = { 0xB1U, 0xA8U, 0x22U, 0x25U, 0x6BU, 0xD1U, 0x6CU, 0xCFU, 0x67U };

        const uint8_t   PAYLOAD_LEFT_MASK[] = { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xF0U };
        const uint8_t   PAYLOAD_RIGHT_MASK[] = { 0x0FU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU };

        const uint8_t   VOICE_LC_HEADER_CRC_MASK[] = { 0x96U, 0x96U, 0x96U };
        const uint8_t   TERMINATOR_WITH_LC_CRC_MASK[] = { 0x99U, 0x99U, 0x99U };
        const uint8_t   PI_HEADER_CRC_MASK[] = { 0x69U, 0x69U };
        const uint8_t   DATA_HEADER_CRC_MASK[] = { 0xCCU, 0xCCU };
        const uint8_t   CSBK_CRC_MASK[] = { 0xA5U, 0xA5U };
        const uint8_t   CSBK_MBC_CRC_MASK[] = { 0xAAU, 0xAAU };

        const uint8_t   DT_MASK = 0x0FU;

        const uint8_t   IDLE_RX = 0x80U;
        const uint8_t   SYNC_DATA = 0x40U;
        const uint8_t   SYNC_VOICE = 0x20U;

        const uint8_t   SLOT1 = 0x00U;
        const uint8_t   SLOT2 = 0x80U;

        const uint32_t  MAX_PDU_COUNT = 32U;
        const uint32_t  MAX_PDU_LENGTH = 512U;

        const uint32_t  MI_LENGTH_BYTES = 4U;           // This was guessed based on OTA data captures -- the message indicator seems to be the same length as a source/destination address
        /** @} */

        /** @name Thresholds */
        /** @brief TSCC maximum CSC count */
        const uint16_t  TSCC_MAX_CSC_CNT = 511U;

        /** @brief DMR slot time in milliseconds */
        const uint32_t  DMR_SLOT_TIME = 60U;
        /** @brief Number of AMBE per slot */
        const uint32_t  AMBE_PER_SLOT = 3U;

        /** @brief Random Access Wait */
        const uint8_t   DEFAULT_NRAND_WAIT = 8U;
        /** @brief Default Silence Threshold */
        const uint32_t  DEFAULT_SILENCE_THRESHOLD = 21U;
        /** @brief Default Frame Loss Threshold */
        const uint32_t  DEFAULT_FRAME_LOSS_THRESHOLD = 2U;
        /** @brief Maximum P25 voice frame errors */
        const uint32_t  MAX_DMR_VOICE_ERRORS = 141U;
        /** @} */

        /** @name Default Values */
        const uint8_t   DMR_ALOHA_VER_151 = 0x00U;
        const uint8_t   DMR_CHNULL = 0x00U;

        const uint16_t  DMR_LOGICAL_CH_ABSOLUTE = 0xFFFU;

        const uint32_t  WUID_SUPLI = 0xFFFEC4U;         //! Supplementary Data Service Working Unit ID
        const uint32_t  WUID_SDMI = 0xFFFEC5U;          //! UDT Short Data Service Working Unit ID
        const uint32_t  WUID_REGI = 0xFFFEC6U;          //! Registration Working Unit ID
        const uint32_t  WUID_STUNI = 0xFFFECCU;         //! MS Stun/Revive Identifier
        const uint32_t  WUID_AUTHI = 0xFFFECDU;         //! Authentication Working Unit ID
        const uint32_t  WUID_KILLI = 0xFFFECFU;         //! MS Kill Identifier
        const uint32_t  WUID_TATTSI = 0xFFFED7U;        //! Talkgroup Subscription/Attachement Service Working Unit ID
        const uint32_t  WUID_ALLL = 0xFFFFFDU;          //! All-call Site-wide Working Unit ID
        const uint32_t  WUID_ALLZ = 0xFFFFFEU;          //! All-call System-wide Working Unit ID
        const uint32_t  WUID_ALL = 0xFFFFFFU;           //! All-call Network-wide Working Unit ID

        const uint32_t  NO_HEADERS_SIMPLEX = 8U;
        const uint32_t  NO_HEADERS_DUPLEX = 3U;
        const uint32_t  NO_PREAMBLE_CSBK = 15U;
        /** @} */

        /** @brief Data Packet Format */
        namespace DPF {
            /** @brief Data Packet Format */
            enum E : uint8_t {
                UDT = 0x00U,                            //! Unified Data Transport Header
                RESPONSE = 0x01U,                       //! Response Data Header
                UNCONFIRMED_DATA = 0x02U,               //! Unconfirmed Data Header
                CONFIRMED_DATA = 0x03U,                 //! Confirmed Data Header
                DEFINED_SHORT = 0x0DU,                  //! Defined Short Data Header
                DEFINED_RAW = 0x0EU,                    //! Defined Raw Data Header
                PROPRIETARY = 0x0FU,                    //! Proprietary
            };
        };

        /** @brief Data Response Class */
        namespace PDUResponseClass {
            /** @brief Data Response Class */
            enum : uint8_t {
                ACK = 0x00U,                            //! Acknowledge
                NACK = 0x01U,                           //! Negative Acknowledge
                ACK_RETRY = 0x02U                       //! Acknowlege Retry
            };
        };

        /** @brief Data Response Type */
        namespace PDUResponseType {
            /** @brief Data Response Type */
            enum : uint8_t {
                ACK = 0x01U,                            //! Acknowledge

                NACK_ILLEGAL = 0x00U,                   //! Illegal Format
                NACK_PACKET_CRC = 0x01U,                //! Packet CRC
                NACK_MEMORY_FULL = 0x02U,               //! Memory Full
                NACK_UNDELIVERABLE = 0x04U              //! Undeliverable
            };
        };

        /** @name Feature IDs */
        /** @brief ETSI Standard Feature Set */
        const uint8_t   FID_ETSI = 0x00U;
        /** @brief Motorola */
        const uint8_t   FID_DMRA = 0x10U;
        /** @brief DVM; Omaha Communication Systems, LLC ($9C) */
        const uint8_t   FID_DVM_OCS = 0x9CU;
        /** @} */

        /** @name LC Service Options */
        const uint8_t   LC_SVC_OPT_EMERGENCY = 0x80U;
        const uint8_t   LC_SVC_OPT_PRIVACY = 0x40U;
        const uint8_t   LC_SVC_OPT_BCAST = 0x08U;
        const uint8_t   LC_SVC_OPT_OVCM = 0x04U;
        /** @} */

        /** @name Call Priorities */
        const uint8_t   CALL_PRIORITY_NONE = 0x00U;
        const uint8_t   CALL_PRIORITY_1 = 0x01U;
        const uint8_t   CALL_PRIORITY_2 = 0x02U;
        const uint8_t   CALL_PRIORITY_3 = 0x03U;
        /** @} */

        /** @brief Short-Link Control Opcode(s) */
        namespace SLCO {
            /** @brief Short-Link Control Opcode(s) */
            enum E : uint8_t {
                NONE = 0x00U,                           //! NULL
                ACT = 0x01U,                            //!
                TSCC = 0x02U,                           //! TSCC
                PAYLOAD = 0x03U                         //! Payload
            };
        }

        /** @brief Full-Link Control Opcode(s) */
        namespace FLCO {
            /** @brief Full-Link Control Opcode(s) */
            enum E : uint8_t {
                GROUP = 0x00U,                          //! GRP VCH USER - Group Voice Channel User
                PRIVATE = 0x03U,                        //! UU VCH USER - Unit-to-Unit Voice Channel User
                
                TALKER_ALIAS_HEADER = 0x04U,            //!
                TALKER_ALIAS_BLOCK1 = 0x05U,            //!
                TALKER_ALIAS_BLOCK2 = 0x06U,            //!
                TALKER_ALIAS_BLOCK3 = 0x07U,            //!
                
                GPS_INFO = 0x08U,                       //!
            };
        }

        /** @brief FID_DMRA Extended Functions. */
        namespace ExtendedFunctions {
            /** @brief FID_DMRA Extended Functions. */
            enum : uint16_t {
                CHECK = 0x0000U,                        //! Radio Check
                UNINHIBIT = 0x007EU,                    //! Radio Uninhibit
                INHIBIT = 0x007FU,                      //! Radio Inhibit
                CHECK_ACK = 0x0080U,                    //! Radio Check Ack
                UNINHIBIT_ACK = 0x00FEU,                //! Radio Uninhibit Ack
                INHIBIT_ACK = 0x00FFU                   //! Radio Inhibit Ack
            };
        };

        /** @brief Data Type(s) */
        namespace DataType {
            /** @brief Data Type(s) */
            enum E : uint8_t {
                VOICE_PI_HEADER = 0x00U,                //! Voice with Privacy Indicator Header
                VOICE_LC_HEADER = 0x01U,                //! Voice with Link Control Header

                TERMINATOR_WITH_LC = 0x02U,             //! Terminator with Link Control

                CSBK = 0x03U,                           //! CSBK

                MBC_HEADER = 0x04U,                     //! Multi-Block Control Header
                MBC_DATA = 0x05U,                       //! Multi-Block Control Data

                DATA_HEADER = 0x06U,                    //! Data Header
                RATE_12_DATA = 0x07U,                   //! 1/2 Rate Data
                RATE_34_DATA = 0x08U,                   //! 3/4 Rate Data

                IDLE = 0x09U,                           //! Idle

                RATE_1_DATA = 0x0AU,                    //! Rate 1 Data

                /*
                ** Internal Data Type(s)
                */

                VOICE_SYNC = 0xF0U,                     //! Internal - Voice Sync
                VOICE = 0xF1U                           //! Internal - Voice
            };
        }

    #define DMR_DT_VOICE_PI_HEADER      "DMR, VOICE_PI_HEADER (Voice Header with Privacy Indicator)"
    #define DMR_DT_VOICE_LC_HEADER      "DMR, VOICE_LC_HEADER (Voice Header with Link Control)"
    #define DMR_DT_TERMINATOR_WITH_LC   "DMR, TERMINATOR_WITH_LC (Terminator with Link Control)"
    #define DMR_DT_DATA_HEADER          "DMR, DATA_HEADER (Data Header)"
    #define DMR_DT_RATE_12_DATA         "DMR, RATE_12_DATA (1/2-rate Data)"
    #define DMR_DT_RATE_34_DATA         "DMR, RATE_34_DATA (3/4-rate Data)"
    #define DMR_DT_RATE_1_DATA          "DMR, RATE_1_DATA (1-rate Data)"

    #define DMR_DT_VOICE_SYNC           "DMR, VOICE_SYNC (Voice Data with Sync)"
    #define DMR_DT_VOICE                "DMR, VOICE (Voice Data)"

        /** @brief Site Models */
        namespace SiteModel {
            /** @brief Site Models */
            enum E : uint8_t {
                SM_TINY = 0x00U,                        //! Tiny
                SM_SMALL = 0x01U,                       //! Small
                SM_LARGE = 0x02U,                       //! Large
                SM_HUGE = 0x03U                         //! Huge
            };
        }

        /** @name Target Address */
        const uint8_t   TGT_ADRS_SYSCODE = 0x00U;
        const uint8_t   TGT_ADRS_TGID = 0x01U;
        /** @} */

        /** @brief Talker ID */
        namespace TalkerID {
            /** @brief Talker ID */
            enum : uint8_t {
                NONE = 0x00U,                           //! No Talker ID

                HEADER = 0x01U,                         //! Talker ID Header
                
                BLOCK1 = 0x02U,                         //! Talker ID Block 1
                BLOCK2 = 0x04U,                         //! Talker ID Block 2
                BLOCK3 = 0x08U                          //! Talker ID Block 3
            };
        }

        /** @brief Reason Code(s) */
        namespace ReasonCode {
            /** @brief Reason Code(s) */
            enum : uint8_t {
                TS_ACK_RSN_MSG = 0x60U,                 //! TS - Message Accepted
                TS_ACK_RSN_REG = 0x62U,                 //! TS - Registration Accepted
                TS_ACK_RSN_AUTH_RESP = 0x64U,           //! TS - Authentication Challenge Response
                TS_ACK_RSN_REG_SUB_ATTACH = 0x65U,      //! TS - Registration Response with subscription
                MS_ACK_RSN_MSG = 0x44U,                 //! MS - Message Accepted
                MS_ACK_RSN_AUTH_RESP = 0x48U,           //! MS - Authentication Challenge Response

                TS_DENY_RSN_SYS_UNSUPPORTED_SVC = 0x20U,//! System Unsupported Service
                TS_DENY_RSN_PERM_USER_REFUSED = 0x21U,  //! User Permenantly Refused
                TS_DENY_RSN_TEMP_USER_REFUSED = 0x22U,  //! User Temporarily Refused
                TS_DENY_RSN_TRSN_SYS_REFUSED = 0x23U,   //! System Refused
                TS_DENY_RSN_TGT_NOT_REG = 0x24U,        //! Target Not Registered
                TS_DENY_RSN_TGT_UNAVAILABLE = 0x25U,    //! Target Unavailable
                TS_DENY_RSN_SYS_BUSY = 0x27U,           //! System Busy
                TS_DENY_RSN_SYS_NOT_READY = 0x28U,      //! System Not Ready
                TS_DENY_RSN_CALL_CNCL_REFUSED = 0x29U,  //! Call Cancel Refused
                TS_DENY_RSN_REG_REFUSED = 0x2AU,        //! Registration Refused
                TS_DENY_RSN_REG_DENIED = 0x2BU,         //! Registration Denied
                TS_DENY_RSN_MS_NOT_REG = 0x2DU,         //! MS Not Registered
                TS_DENY_RSN_TGT_BUSY = 0x2EU,           //! Target Busy
                TS_DENY_RSN_TGT_GROUP_NOT_VALID = 0x2FU,//! Group Not Valid

                TS_QUEUED_RSN_NO_RESOURCE = 0xA0U,      //! No Resources Available
                TS_QUEUED_RSN_SYS_BUSY = 0xA1U,         //! System Busy

                TS_WAIT_RSN = 0xE0U,                    //! Wait

                MS_DENY_RSN_UNSUPPORTED_SVC = 0x00U,    //! Service Unsupported
            };
        }

        /** @brief Random Access Service Kind */
        namespace ServiceKind {
            /** @brief Random Access Service Kind */
            enum : uint8_t {
                IND_VOICE_CALL = 0x00U,                 //! Individual Voice Call
                GRP_VOICE_CALL = 0x01U,                 //! Group Voice Call
                IND_DATA_CALL = 0x02U,                  //! Individual Data Call
                GRP_DATA_CALL = 0x03U,                  //! Group Data Call
                IND_UDT_DATA_CALL = 0x04U,              //! Individual UDT Short Data Call
                GRP_UDT_DATA_CALL = 0x05U,              //! Group UDT Short Data Call
                UDT_SHORT_POLL = 0x06U,                 //! UDT Short Data Polling Service
                STATUS_TRANSPORT = 0x07U,               //! Status Transport Service
                CALL_DIVERSION = 0x08U,                 //! Call Diversion Service
                CALL_ANSWER = 0x09U,                    //! Call Answer Service
                SUPPLEMENTARY_SVC = 0x0DU,              //! Supplementary Service
                REG_SVC = 0x0EU,                        //! Registration Service
                CANCEL_CALL = 0x0FU                     //! Cancel Call Service
            };
        }

        /** @brief Broadcast Announcement Type(s) */
        namespace BroadcastAnncType {
            /** @brief Broadcast Announcement Type(s) */
            enum : uint8_t {
                ANN_WD_TSCC = 0x00U,                    //! Announce-Withdraw TSCC Channel
                CALL_TIMER_PARMS = 0x01U,               //! Specify Call Timer Parameters
                VOTE_NOW = 0x02U,                       //! Vote Now Advice
                LOCAL_TIME = 0x03U,                     //! Broadcast Local Time
                MASS_REG = 0x04U,                       //! Mass Registration
                CHAN_FREQ = 0x05U,                      //! Logical Channel/Frequency
                ADJ_SITE = 0x06U,                       //! Adjacent Site Information
                SITE_PARMS = 0x07U                      //! General Site Parameters
            };
        }

        /** @brief Control Signalling Block Opcode(s) */
        namespace CSBKO {
            /** @brief Control Signalling Block Opcode(s) */
            enum : uint8_t {
            // CSBK ISP/OSP Shared Opcode(s)
                NONE = 0x00U,                           //!
                UU_V_REQ = 0x04U,                       //! UU VCH REQ - Unit-to-Unit Voice Channel Request
                UU_ANS_RSP = 0x05U,                     //! UU ANS RSP - Unit-to-Unit Answer Response
                CTCSBK = 0x07U,                         //! CT CSBK - Channel Timing CSBK
                ALOHA = 0x19U,                          //! ALOHA - Aloha PDU for Random Access
                AHOY = 0x1CU,                           //! AHOY - Enquiry from TSCC
                RAND = 0x1FU,                           //! (ETSI) RAND - Random Access / (DMRA) CALL ALRT - Call Alert
                ACK_RSP = 0x20U,                        //! ACK RSP - Acknowledge Response
                EXT_FNCT = 0x24U,                       //! (DMRA) EXT FNCT - Extended Function
                NACK_RSP = 0x26U,                       //! NACK RSP - Negative Acknowledgement Response
                BROADCAST = 0x28U,                      //! BCAST - Announcement PDU
                MAINT = 0x2AU,                          //! MAINT - Call Maintainence PDU
                P_CLEAR = 0x2EU,                        //! P_CLEAR - Payload Channel Clear
                PV_GRANT = 0x30U,                       //! PV_GRANT - Private Voice Channel Grant
                TV_GRANT = 0x31U,                       //! TV_GRANT - Talkgroup Voice Channel Grant
                BTV_GRANT = 0x32U,                      //! BTV_GRANT - Broadcast Talkgroup Voice Channel Grant
                PD_GRANT = 0x33U,                       //! PD_GRANT - Private Data Channel Grant
                TD_GRANT = 0x34U,                       //! TD_GRANT - Talkgroup Data Channel Grant
                BSDWNACT = 0x38U,                       //! BS DWN ACT - BS Outbound Activation
                PRECCSBK = 0x3DU,                       //! PRE CSBK - Preamble CSBK

            // CSBK DVM Outbound Signalling Packet (OSP) Opcode(s)
                DVM_GIT_HASH = 0x3FU                    //!
            };
        }

        /** @} */
    } // namespace defines
} // namespace dmr

#endif // __DMR_DEFINES_H__
