// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2017-2024 Bryan Biedenkapp, N2PLL
 *  Copyright (C) 2025 Caleb, K4PHP
 *
 */
/**
 * @defgroup p25 Project 25
 * @brief Implementation for the TIA-102 Project 25 standard.
 * @ingroup common
 * 
 * @file P25Defines.h
 * @ingroup p25
 */
#if !defined(__P25_DEFINES_H__)
#define  __P25_DEFINES_H__

#include "common/Defines.h"

// Shorthand macro to p25::defines -- keeps source code that doesn't use "using" concise
#if !defined(P25DEF)
#define P25DEF p25::defines
#endif // P25DEF
namespace p25
{
    namespace defines
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        /**
         * @addtogroup p25
         * @{
         */

        /** @name Frame Lengths and Misc Constants */
        const uint32_t  P25_HDU_FRAME_LENGTH_BYTES = 99U;
        const uint32_t  P25_HDU_FRAME_LENGTH_BITS = P25_HDU_FRAME_LENGTH_BYTES * 8U;

        const uint32_t  P25_TDU_FRAME_LENGTH_BYTES = 18U;
        const uint32_t  P25_TDU_FRAME_LENGTH_BITS = P25_TDU_FRAME_LENGTH_BYTES * 8U;

        const uint32_t  P25_LDU_FRAME_LENGTH_BYTES = 216U;
        const uint32_t  P25_LDU_FRAME_LENGTH_BITS = P25_LDU_FRAME_LENGTH_BYTES * 8U;

        const uint32_t  P25_TSDU_FRAME_LENGTH_BYTES = 45U;
        const uint32_t  P25_TSDU_FRAME_LENGTH_BITS = P25_TSDU_FRAME_LENGTH_BYTES * 8U;

        const uint32_t  P25_TSDU_DOUBLE_FRAME_LENGTH_BYTES = 72U;
        const uint32_t  P25_TSDU_DOUBLE_FRAME_LENGTH_BITS = P25_TSDU_DOUBLE_FRAME_LENGTH_BYTES * 8U;

        const uint32_t  P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES = 90U;
        const uint32_t  P25_TSDU_TRIPLE_FRAME_LENGTH_BITS = P25_TSDU_TRIPLE_FRAME_LENGTH_BYTES * 8U;

        const uint32_t  P25_PDU_FRAME_LENGTH_BYTES = 512U;
        const uint32_t  P25_PDU_FRAME_LENGTH_BITS = P25_PDU_FRAME_LENGTH_BYTES * 8U;

        const uint32_t  P25_TDULC_FRAME_LENGTH_BYTES = 54U;
        const uint32_t  P25_TDULC_FRAME_LENGTH_BITS = P25_TDULC_FRAME_LENGTH_BYTES * 8U;

        const uint32_t  P25_NID_LENGTH_BYTES = 8U;
        const uint32_t  P25_NID_LENGTH_BITS = P25_NID_LENGTH_BYTES * 8U;

        const uint8_t   P25_SYNC_BYTES[] = { 0x55U, 0x75U, 0xF5U, 0xFFU, 0x77U, 0xFFU };
        const uint32_t  P25_SYNC_LENGTH_BYTES = 6U;
        const uint32_t  P25_SYNC_LENGTH_BITS = P25_SYNC_LENGTH_BYTES * 8U;
        const uint8_t   P25_START_SYNC = 0x5FU;

        const uint32_t  P25_PREAMBLE_LENGTH_BYTES = P25_SYNC_LENGTH_BYTES + P25_NID_LENGTH_BYTES;
        const uint32_t  P25_PREAMBLE_LENGTH_BITS = P25_SYNC_LENGTH_BITS + P25_NID_LENGTH_BITS;

        const uint32_t  P25_LDU_FRAME_TIME = 180U;

        const uint32_t  P25_HDU_LENGTH_BYTES = 81U;
        const uint32_t  P25_LDU_LC_FEC_LENGTH_BYTES = 18U;

        const uint32_t  P25_TDULC_FEC_LENGTH_BYTES = 36U;
        const uint32_t  P25_TDULC_LENGTH_BYTES = 18U;
        const uint32_t  P25_TDULC_PAYLOAD_LENGTH_BYTES = 8U;

        const uint32_t  P25_TSBK_FEC_LENGTH_BYTES = 25U;
        const uint32_t  P25_TSBK_FEC_LENGTH_BITS = P25_TSBK_FEC_LENGTH_BYTES * 8U - 4U; // Trellis is actually 196 bits
        const uint32_t  P25_TSBK_LENGTH_BYTES = 12U;

        const uint32_t  P25_MAX_PDU_BLOCKS = 42U;

        const uint32_t  P25_PDU_HEADER_LENGTH_BYTES = 12U;
        const uint32_t  P25_PDU_CONFIRMED_LENGTH_BYTES = 18U;
        const uint32_t  P25_PDU_CONFIRMED_DATA_LENGTH_BYTES = 16U;
        const uint32_t  P25_PDU_UNCONFIRMED_LENGTH_BYTES = 12U;

        const uint32_t  P25_PDU_ARP_PCKT_LENGTH = 22U;
        const uint8_t   P25_PDU_ARP_HW_ADDR_LENGTH = 3U;
        const uint8_t   P25_PDU_ARP_PROTO_ADDR_LENGTH = 4U;

        const uint32_t  P25_PDU_FEC_LENGTH_BYTES = 25U;
        const uint32_t  P25_PDU_FEC_LENGTH_BITS = P25_PDU_FEC_LENGTH_BYTES * 8U - 4U; // Trellis is actually 196 bits

        const uint32_t  MI_LENGTH_BYTES = 9U;
        const uint32_t  RAW_IMBE_LENGTH_BYTES = 11U;

        const uint32_t  P25_SS0_START = 70U;
        const uint32_t  P25_SS1_START = 71U;
        const uint32_t  P25_SS_INCREMENT = 72U;

        const uint8_t   NULL_IMBE[] = { 0x04U, 0x0CU, 0xFDU, 0x7BU, 0xFBU, 0x7DU, 0xF2U, 0x7BU, 0x3DU, 0x9EU, 0x45U };
        const uint8_t   ENCRYPTED_NULL_IMBE[] = { 0xFCU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };

        const uint8_t   MOT_CALLSIGN_LENGTH_BYTES = 8U;

        const uint8_t   AUTH_RES_LENGTH_BYTES = 4U;
        const uint8_t   AUTH_RAND_SEED_LENGTH_BYTES = 10U;
        const uint8_t   AUTH_RAND_CHLNG_LENGTH_BYTES = 5U;
        const uint8_t   AUTH_KEY_LENGTH_BYTES = 16U;

        const uint8_t   MAX_ENC_KEY_LENGTH_BYTES = 32U;
        /* @} */

        /** @name Thresholds */
        /** @brief Default Silence Threshold */
        const uint32_t  DEFAULT_SILENCE_THRESHOLD = 124U;
        /** @brief Default Frame Loss Threshold */
        const uint32_t  DEFAULT_FRAME_LOSS_THRESHOLD = 6U;
        /** @brief Maximum P25 voice frame errors */
        const uint32_t  MAX_P25_VOICE_ERRORS = 1233U;
        /** @} */

        /** @name Manufacturer IDs */
        /** @brief Standard MFId */
        const uint8_t   MFG_STANDARD = 0x00U;
        /** @brief Alternate Standard MFId */
        const uint8_t   MFG_STANDARD_ALT = 0x01U;

        /** @brief Motorola MFId */
        const uint8_t   MFG_MOT = 0x90U;
        /** @brief DVM; Omaha Communication Systems, LLC ($9C) */
        const uint8_t   MFG_DVM_OCS = 0x9CU;
        /** @} */

        /** @name Encryption Algorithms */
        /** @brief Unencrypted */
        const uint8_t   ALGO_UNENCRYPT = 0x80U;
        /** @brief DES-OFB */
        const uint8_t   ALGO_DES = 0x81U;
        /** @brief AES-256 */
        const uint8_t   ALGO_AES_256 = 0x84U;
        /** @brief ARC4 */
        const uint8_t   ALGO_ARC4 = 0xAAU;
        /** @} */

        /** @name IDEN Table Bandwidth Sizes */
        /** @brief 6.25khz VHF/UHF IDEN Bandwidth */
        const uint8_t   IDEN_UP_VU_BW_625K = 0x04U;
        /** @brief 12.5khz VHF/UHF IDEN Bandwidth */
        const uint8_t   IDEN_UP_VU_BW_125K = 0x05U;
        /** @} */

        /** @name Default Values */
        /** @brief Digital Squelch NAC */
        const uint32_t  NAC_DIGITAL_SQ = 0xF7EU;
        /** @brief Reuse RX NAC */
        const uint32_t  NAC_REUSE_RX_NAC = 0xF7FU;
        /** @brief Default NAC */
        const uint32_t  DEFAULT_NAC = 0x293U;

        /** @brief Default WACN */
        const uint32_t  WACN_STD_DEFAULT = 0xBB800U;
        /** @brief Default System ID */
        const uint32_t  SID_STD_DEFAULT = 0x001U;

        /** @brief FNE Working Unit ID */
        const uint32_t  WUID_FNE = 0xFFFFFCU;
        /** @brief Registration Working Unit ID */
        const uint32_t  WUID_REG = 0xFFFFFEU;
        /** @brief All-call Working Unit ID */
        const uint32_t  WUID_ALL = 0xFFFFFFU;

        /** @brief All-call Talkgroup ID */
        const uint32_t  TGID_ALL = 0xFFFFU;

        /** @brief ARP Hardware Type */
        const uint16_t  P25_PDU_ARP_CAI_TYPE = 0x21U;

        /** @brief ARP Request */
        const uint8_t   P25_PDU_ARP_REQUEST = 0x01U;
        /** @brief ARP Reply */
        const uint8_t   P25_PDU_ARP_REPLY = 0x02U;
        /** @} */

        /** @brief Station Service Classes */
        namespace ServiceClass {
            /** @brief Station Service Classes */
            enum : uint8_t {
                INVALID = 0x00U,                        //! Invalid Service Class
                COMPOSITE = 0x01U,                      //! Composite Control Channel
                DATA = 0x10U,                           //! Data
                VOICE = 0x20U,                          //! Voice
                REG = 0x40U,                            //! Registration
                AUTH = 0x80U                            //! Authentication
            };
        }

        /** @brief System Service Types */
        namespace SystemService {
            /** @brief System Service Types */
            enum : uint32_t {
                NET_ACTIVE = 0x0200000U,                //! Network Active
                GROUP_VOICE = 0x0080000U,               //! Group Voice
                IND_VOICE = 0x0040000U,                 //! Individual Voice
                PSTN_UNIT_VOICE = 0x0020000U,           //! PSTN Unit Voice
                UNIT_PSTN_VOICE = 0x0010000U,           //! Unit PSTN Voice
                GROUP_DATA = 0x0004000U,                //! Group Data
                IND_DATA = 0x0002000U,                  //! Individual Data
                UNIT_REG = 0x0000800U,                  //! Unit Registration
                GROUP_AFF = 0x0000400U,                 //! Group Affiliation
                GROUP_AFF_Q = 0x0000200U,               //! Group Affiliation Query
                USER_STS = 0x0000040U,                  //! User Status
                USER_MSG = 0x0000020U,                  //! User Message
                UNIT_STS = 0x0000010U,                  //! Unit Status
                USER_STS_Q = 0x0000008U,                //! User Status Query
                UNIT_STS_Q = 0x0000004U,                //! Unit Status Query
                CALL_ALRT = 0x0000002U,                 //! Call Alert
                EMERGENCY = 0x0000001U                  //! Emergency
            };
        }

        /** @name Default System Service Types */
        /** @brief System Service Defaults */
        const uint32_t  SYS_SRV_DEFAULT = SystemService::EMERGENCY  | SystemService::CALL_ALRT  | SystemService::USER_MSG   |
                                          SystemService::UNIT_STS_Q | SystemService::USER_STS_Q | SystemService::UNIT_STS   | SystemService::USER_STS |
                                          SystemService::IND_DATA   | SystemService::IND_VOICE  | SystemService::GROUP_DATA | SystemService::GROUP_VOICE;
        /** @brief System Service Trunking Defaults */
        const uint32_t  SYS_SRV_TRUNK   = SYS_SRV_DEFAULT           | SystemService::GROUP_AFF  | SystemService::UNIT_REG   | SystemService::GROUP_AFF_Q;
        /** @} */

        /** @brief Conventional/Failur/Valid/Networked Flags */
        namespace CFVA {
            /** @brief Conventional/Failur/Valid/Networked Flags */
            enum : uint8_t {
                CONV = 0x08U,                           //! Conventional
                FAILURE = 0x04U,                        //! Failure
                VALID = 0x02U,                          //! Valid
                NETWORK = 0x01U                         //! Networked
            };
        }

        /** @brief Response Codes */
        namespace ResponseCode {
            /** @brief Response Codes */
            enum : uint8_t {
            // General Codes
                ACCEPT = 0x00U,                         //! Accept
                FAIL = 0x01U,                           //! Fail
                DENY = 0x02U,                           //! Deny
                REFUSED = 0x03U,                        //! Refused

            // Answer Codes
                ANS_PROCEED = 0x20U,                    //! Proceed
                ANS_DENY = 0x21U,                       //! Deny
                ANS_WAIT = 0x22U                        //! Wait
            };
        }

        /** @brief Cancel Service Codes */
        namespace CancelService {
            /** @brief Cancel Service Codes */
            enum : uint8_t {
                NONE = 0x00U,                           //! None
                TERM_QUE = 0x10U,                       //! Terminate Queued
                TERM_RSRC_ASSIGN = 0x20U                //! Terminate Resource Assigned
            };
        }

        /** @brief Reason Codes */
        namespace ReasonCode {
            /** @brief Reason Codes */
            enum : uint8_t {
            // Denial Codes
                DENY_REQ_UNIT_NOT_VALID = 0x10U,        //! Requesting Unit Not Valid
                DENY_REQ_UNIT_NOT_AUTH = 0x11U,         //! Requesting Unit Not Authenticated

                DENY_TGT_UNIT_NOT_VALID = 0x20U,        //! Target Unit Not Vaild
                DENY_TGT_UNIT_NOT_AUTH = 0x21U,         //! Target Unit Not Authenticated
                DENY_SU_FAILED_AUTH = 0x22U,            //! Subscriber Failed Authentication
                DENY_TGT_UNIT_REFUSED = 0x2FU,          //! Target Unit Refused

                DENY_TGT_GROUP_NOT_VALID = 0x30U,       //! Target Group Not Valid
                DENY_TGT_GROUP_NOT_AUTH = 0x31U,        //! Target Group Not Authenticated

                DENY_NO_NET_RSRC_AVAIL = 0x53U,         //! No Network Resources Available
                DENY_NO_RF_RSRC_AVAIL = 0x54U,          //! No RF Resources Available
                DENY_SVC_IN_USE = 0x55U,                //! Service In Use

                DENY_SITE_ACCESS_DENIAL = 0x60U,        //! Site Access Denial

                DENY_PTT_COLLIDE = 0x67U,               //! Push-to-Talk Collision
                DENY_PTT_BONK = 0x77U,                  //! Push-to-Talk Denial/Bonk

                DENY_SYS_UNSUPPORTED_SVC = 0xFFU,       //! Service Unsupported
            
            // Queue Codes
                QUE_REQ_ACTIVE_SERVICE = 0x10U,         //! Requested Service Active
                QUE_TGT_ACTIVE_SERVICE = 0x20U,         //! Target Service Active

                QUE_TGT_UNIT_QUEUED = 0x2FU,            //! Target Unit Queued

                QUE_CHN_RESOURCE_NOT_AVAIL = 0x40U      //! Channel Resource Not Available
            };
        }

        /** @brief Extended Function Opcodes */
        namespace ExtendedFunctions {
            /** @brief Extended Function Opcodes */
            enum : uint16_t {
                CHECK = 0x0000U,                        //! Radio Check
                UNINHIBIT = 0x007EU,                    //! Radio Uninhibit
                INHIBIT = 0x007FU,                      //! Radio Inhibit
                CHECK_ACK = 0x0080U,                    //! Radio Check Ack
                UNINHIBIT_ACK = 0x00FEU,                //! Radio Uninhibit Ack
                INHIBIT_ACK = 0x00FFU,                  //! Radio Inhibit Ack

                DYN_REGRP_REQ = 0x0200U,                //! MFID $90 (Motorola) Dynamic Regroup IR
                DYN_REGRP_CANCEL = 0x0201U,             //! MFID $90 (Motorola) Dynamic Regroup IR Cancellation
                DYN_REGRP_LOCK = 0x0202U,               //! MFID $90 (Motorola) Lock Selector
                DYN_REGRP_UNLOCK = 0x0203U,             //! MFID $90 (Motorola) Unlock Selector

                DYN_REGRP_REQ_ACK = 0x0280U,            //! MFID $90 (Motorola) Dynamic Regroup IR Ack
                DYN_REGRP_CANCEL_ACK = 0x0281U,         //! MFID $90 (Motorola) Dynamic Regroup IR Cancellation Ack
                DYN_REGRP_LOCK_ACK = 0x0282U,           //! MFID $90 (Motorola) Lock Selector Ack
                DYN_REGRP_UNLOCK_ACK = 0x0283U,         //! MFID $90 (Motorola) Unlock Selector Ack
            };
        }

        /** @brief DVM Network Frame Types */
        namespace FrameType {
            /** @brief DVM Network Frame Types */
            enum E : uint8_t {
                HDU_VALID = 0x01U,                      //! HDU Valid
                HDU_LATE_ENTRY = 0x02U,                 //! HDU Late Entry
                TERMINATOR = 0x03U,                     //! TDU/TDULC Terminator
                DATA_UNIT = 0x00U                       //! Standard Data Unit
            };
        }

        /** @brief Data Format Type */
        namespace PDUFormatType {
            /** @brief Data Format Type */
            enum : uint8_t {
                RSP = 0x03U,                            //! Response
                UNCONFIRMED = 0x15U,                    //! Unconfirmed PDU
                CONFIRMED = 0x16U,                      //! Confirmed PDU
                AMBT = 0x17U                            //! Alternate Multi Block Trunking
            };
        }

        /** @brief Service Access Point */
        namespace PDUSAP {
            /** @brief Service Access Point */
            enum : uint8_t {
                USER_DATA = 0x00U,                      //! User Data
                ENC_USER_DATA = 0x01U,                  //! Encrypted User Data

                PACKET_DATA = 0x04U,                    //! Packet Data

                ARP = 0x05U,                            //! ARP
                
                SNDCP_CTRL_DATA = 0x06U,                //! SNDCP Control Data

                EXT_ADDR = 0x1FU,                       //! Extended Addressing

                CONV_DATA_REG = 0x20U,                  //! Registration

                UNENC_KMM = 0x28U,                      //! Unencrypted KMM
                ENC_KMM = 0x29U,                        //! Encrypted KMM

                TRUNK_CTRL = 0x3DU                      //! Trunking Control
            };
        }

        /** @brief Acknowledgement Class */
        namespace PDUAckClass {
            /** @brief Acknowledgement Class */
            enum : uint8_t {
                ACK = 0x00U,                            //! Acknowledge
                NACK = 0x01U,                           //! Negative Acknowledge
                ACK_RETRY = 0x02U                       //! Acknowledge Retry
            };
        }

        /** @brief Acknowledgement Type */
        namespace PDUAckType {
            /** @brief Acknowledgement Type */
            enum : uint8_t {
                RETRY = 0x00U,                          //! Retry

                ACK = 0x01U,                            //! Acknowledge

                NACK_ILLEGAL = 0x00U,                   //! Illegal Format
                NACK_PACKET_CRC = 0x01U,                //! Packet CRC
                NACK_MEMORY_FULL = 0x02U,               //! Memory Full
                NACK_SEQ = 0x03U,                       //! Out of logical sequence FSN
                NACK_UNDELIVERABLE = 0x04U,             //! Undeliverable
                NACK_OUT_OF_SEQ = 0x05U,                //! Out of sequence, N(S) != V(R) or V(R) + 1
                NACK_INVL_USER = 0x06U                  //! Invalid User disallowed by the system
            };
        }

        /** @brief Registration Type */
        namespace PDURegType {
            /** @brief Registration Type */
            enum : uint8_t {
                CONNECT = 0x00U,                        //! Connect
                DISCONNECT = 0x01U,                     //! Disconnect
                ACCEPT = 0x04U,                         //! Accept
                DENY = 0x05U                            //! Deny
            };
        }

        /** @brief KMM Message Type */
        namespace KMM_MessageType {
            enum : uint8_t {
                NULL_CMD = 0x00U,                       //! Null

                CHANGE_RSI_CMD = 0x03U,                 //! Change RSI Command
                CHANGE_RSI_RSP = 0x04U,                 //! Change RSI Response
                CHANGEOVER_CMD = 0x05U,                 //! Changeover Command
                CHANGEOVER_RSP = 0x06U,                 //! Changeover Response

                HELLO = 0x0CU,                          //! Hello

                INVENTORY_CMD = 0x0DU,                  //! Inventory Command
                INVENTORY_RSP = 0x0EU,                  //! Inventory Response

                MODIFY_KEY_CMD = 0x13U,                 //! Modify Key Command

                NAK = 0x16U,                            //! Negative Ack
                NO_SERVICE = 0x17U,                     //! No Service

                ZEROIZE_CMD = 0x21U,                    //! Zeroize Command
                ZEROIZE_RSP = 0x22U,                    //! Zeroize Response

                DEREG_CMD = 0x23U,                      //! SU Deregistration Command
                DEREG_RSP = 0x24U,                      //! SU Deregistration Response
                REG_CMD = 0x25U,                        //! SU Registration Command
                REG_RSP = 0x26U,                        //! SU Registration Response
            };
        }

        /** @brief KMM Response Kind */
        namespace KMM_ResponseKind {
            enum : uint8_t {
                NONE = 0x00U,                           //! Response Kind 1 (None)
                DELAYED = 0x01U,                        //! Response Kind 2 (Delayed)
                IMMEDIATE = 0x02U,                      //! Response Kind 3 (Immediate)
            };
        }

        /** @brief KMM Message Authentication */
        namespace KMM_MAC {
            enum : uint8_t {
                NO_MAC = 0x00U,                         //! No Message Authentication
                ENH_MAC = 0x02U,                        //! Enhanced Message Authentication
                DES_MAC = 0x03U,                        //! DES Message Authentication
            };
        }

        /** @brief KMM Inventory Type */
        namespace KMM_InventoryType {
            enum : uint8_t {
                NULL_INVENTORY = 0x00U,                 //! Null

                LIST_ACTIVE_KEYSET_IDS = 0x01U,         //! List Active Keyset IDs
                LIST_INACTIVE_KEYSET_IDS = 0x02U,       //! List Inactive Keyset IDs
                LIST_ACTIVE_KEY_IDS = 0x03U,            //! List Active Key IDs
                LIST_INACTIVE_KEY_IDS = 0x04U,          //! List Inactive Key IDs
            };
        }

        /** @brief KMM Hello Flag */
        namespace KMM_HelloFlag {
            enum : uint8_t {
                IDENT_ONLY = 0x00U,                     //! KMF or SU Identification Only

                REKEY_REQUEST_UKEK = 0x01U,             //! Rekey Request (UKEK Exists)
                REKEY_REQUEST_NO_UKEK = 0x02U,          //! Rekey Request (UKEK does not exist)
            };
        }

        /** @brief KMM Status */
        namespace KMM_Status {
            enum : uint8_t {
                CMD_PERFORMED = 0x00U,                  //! Command Performed
                CMD_NOT_PERFORMED = 0x01U,              //! Command Was Not Performed

                ITEM_NOT_EXIST = 0x02U,                 //! Item does not exist
                INVALID_MSG_ID = 0x03U,                 //! Invalid Message ID
                INVALID_MAC = 0x04U,                    //! Invalid Message Authentication Code

                OUT_OF_MEMORY = 0x05U,                  //! Out of Memory
                FAILED_TO_DECRYPT = 0x06U,              //! Failed to decrypt message

                INVALID_MSG_NUMBER = 0x07U,             //! Invalid Message Number
                INVALID_KID = 0x08U,                    //! Invalid Key ID
                INVALID_ALGID = 0x09U,                  //! Invalid Algorithm ID
                INVALID_MFID = 0x0AU,                   //! Invalid Manufacturer ID

                MI_ALL_ZERO = 0x0CU,                    //! Message Indicator All Zero
                KEY_FAIL = 0x0DU,                       //! Key Identified by Alg/KID is Erased

                UNKNOWN = 0xFFU,                        //! Unknown
            };
        }

        /** @brief KMM Decryption Instruction - None */
        const uint8_t   KMM_DECRYPT_INSTRUCT_NONE = 0x00U;
        /** @brief KMM Decryption Instruction - Message Indicator */
        const uint8_t   KMM_DECRYPT_INSTRUCT_MI = 0x40U;

        /** @brief KMM Key Format TEK */
        const uint8_t   KEY_FORMAT_TEK = 0x80U;
        /** @brief KMM Key Format TEK */
        const uint8_t   KEY_FORMAT_KEK_EXISTS = 0x40U;
        /** @brief KMM Key Format Delete Key */
        const uint8_t   KEY_FORMAT_DELETE = 0x20U;

        /** @brief SNDCP version 1 */
        const uint8_t   SNDCP_VERSION_1 = 0x01U;
        /** @brief 296 byte MTU */
        const uint8_t   SNDCP_MTU_296 = 1U;
        /** @brief 510 byte MTU */
        const uint8_t   SNDCP_MTU_510 = 2U;

        /** @brief Default NSAPI */
        const uint8_t   DEFAULT_NSAPI = 1U;

        /** @brief SNDCP PDU Message Type */
        namespace SNDCP_PDUType {
            /** @brief SNDCP PDU Message Type */
            enum : uint8_t {
                ACT_TDS_CTX = 0x00U,                    //! Context Activation Request (ISP) / Context Activation Accept (OSP)

                DEACT_TDS_CTX_REQ = 0x02U,              //! Deactivate Context Request
                ACT_TDS_CTX_REJECT = 0x03U,             //! Activate Context Reject

                RF_UNCONFIRMED = 0x04U,                 //! Data Unconfirmed
                RF_CONFIRMED = 0x05U                    //! Data Confirmed
            };
        }

        /** @brief SNDCP Activation TDS States */
        namespace SNDCPState {
            /** @brief SNDCP Activation TDS States */
            enum E : uint8_t {
                IDLE = 0U,                              //! Idle - Waiting for SU Registration
                READY_S = 1U,                           //! Ready* - Waiting for SU Activation
                STANDBY = 2U,                           //! Standby - SU Activated
                READY = 3U,                             //! Ready - SU Activated and Rx/Tx Data
                CLOSED = 4U,                            //! Closed - SU not yet Registered or Deregistered

                ILLEGAL = 255U                          //! Illegal/Unknown
            };
        }

        /** @brief SNDCP Network Address Type */
        namespace SNDCPNAT {
            /** @brief SNDCP Network Address Type */
            enum : uint8_t {
                IPV4_STATIC_ADDR = 0U,                  //! IPv4 Static Address
                IPV4_DYN_ADDR = 1U,                     //! IPv4 Dynamic Address
                IPV4_NO_ADDRESS = 15U                   //! No Address
            };
        }

        /** @brief SNDCP Data Subscriber Unit Type */
        namespace SNDCP_DSUT {
            /** @brief SNDCP Data Subscriber Unit Type */
            enum : uint8_t {
                TRUNKED_DATA_ONLY = 0U,                 //! Trunked Data Only
                ALTERNATING_TRUNKED_DATA_VOICE = 1U,    //! Alternating Trunked Voice & Data
                CONV_DATA_ONLY = 2U,                    //! Conventional Data Only
                ALTERNATING_CONV_DATA_VOICE = 3U,       //! Alternating Conventional Voice & Data
                TRUNKED_CONV_DATA_ONLY = 4U,            //! Trunked and Conventional Data Only
                ALT_T_AND_C_DATA_VOICE = 5U             //! Alternating Trunked and Conventional Voice & Data
            };
        }

        /** @brief SNDCP Ready Timer */
        namespace SNDCPReadyTimer {
            /** @brief SNDCP Ready Timer */
            enum : uint8_t {
                NOT_ALLOWED = 0U,                       //! Not Allowed
                ONE_SECOND = 1U,                        //! 1 Second
                TWO_SECONDS = 2U,                       //! 2 Seconds
                FOUR_SECONDS = 3U,                      //! 4 Seconds
                SIX_SECONDS = 4U,                       //! 6 Seconds
                EIGHT_SECONDS = 5U,                     //! 8 Seconds
                TEN_SECONDS = 6U,                       //! 10 Seconds
                FIFTEEN_SECONDS = 7U,                   //! 15 Seconds
                TWENTY_SECONDS = 8U,                    //! 20 Seconds
                TWENTYFIVE_SECONDS = 9U,                //! 25 Seconds
                THIRTY_SECONDS = 10U,                   //! 30 Seconds
                SIXTY_SECONDS = 11U,                    //! 60 Seconds
                ONE_TWENTY_SECONDS = 12U,               //! 120 Seconds
                ONE_EIGHT_SECONDS = 13U,                //! 180 Seconds
                THREE_HUNDRED_SECONDS = 14U,            //! 300 Seconds
                ALWAYS = 15U                            //! Always
            };
        }

        /** @brief SNDCP Standby Timer */
        namespace SNDCPStandbyTimer {
            /** @brief SNDCP Standby Timer */
            enum : uint8_t {
                NOT_ALLOWED = 0U,                       //! Not Allowed
                TEN_SECONDS = 1U,                       //! 10 Seconds
                THIRTY_SECONDS = 2U,                    //! 30 Seconds
                ONE_MINUTE = 3U,                        //! 1 Minute
                FIVE_MINUTES = 4U,                      //! 5 Minutes
                TEN_MINUTES = 5U,                       //! 10 Minutes
                THIRTY_MINUTES = 6U,                    //! 30 Minutes
                ONE_HOUR = 7U,                          //! 1 Hour
                TWO_HOURS = 8U,                         //! 2 Hours
                FOUR_HOURS = 9U,                        //! 4 Hours
                EIGHT_HOURS = 10U,                      //! 8 Hours
                TWELVE_HOURS = 11U,                     //! 12 Hours
                TWENTY_FOUR_HOURS = 12U,                //! 24 Hours
                FORTY_EIGHT_HOURS = 13U,                //! 48 Hours
                SEVENTY_TWO_HOURS = 14U,                //! 72 Hours
                ALWAYS = 15U                            //! Always
            };
        }

        /** @brief SNDCP Reject Reasons */
        namespace SNDCPRejectReason {
            /** @brief SNDCP Reject Reasons */
            enum : uint8_t {
                ANY_REASON = 0U,                        //! Any Reason
                SU_NOT_PROVISIONED = 1U,                //! Subscriber Not Provisioned
                SU_DSUT_NOT_SUPPORTED = 2U,             //! Subscriber Data Unit Type Not Supported
                MAX_TDS_CTX_EXCEEDED = 3U,              //! Maximum Number of TDS Contexts Exceeded
                SNDCP_VER_NOT_SUPPORTED = 4U,           //! SNDCP Version Not Supported
                PDS_NOT_SUPPORTED_SITE = 5U,            //! Packet Data Service Not Supported on Site
                PDS_NOT_SUPPORTED_SYSTEM = 6U,          //! Packet Data Service Not Supported on System

                STATIC_IP_NOT_CORRECT = 7U,             //! Static IP Address Not Correct
                STATIC_IP_ALLOCATION_UNSUPPORTED = 8U,  //! Static IP Address Allocation Unsupported
                STATIC_IP_IN_USE = 9U,                  //! Static IP In Use

                IPV4_NOT_SUPPORTED = 10U,               //! IPv4 Not Supported

                DYN_IP_POOL_EMPTY = 11U,                //! Dynamic IP Address Pool Empty
                DYN_IP_ALLOCATION_UNSUPPORTED = 12U     //! Dynamic IP Address Allocation Unsupported
            };
        }

        /** @brief SNDCP Deactivation Types */
        namespace SNDCPDeactivationType {
            /** @brief SNDCP Deactivation Types */
            enum : uint8_t {
                DEACT_ALL = 0U,                         //! Deactivate all NSAPIs
                DEACT_THIS_PDU = 1U                     //! Deactivate NSAPI in this PDU
            };
        }

        /** @brief LC Service Option - Emergency */
        const uint8_t   LC_SVC_OPT_EMERGENCY = 0x80U;
        /** @brief LC Service Option - Encryption */
        const uint8_t   LC_SVC_OPT_ENCRYPTION = 0x40U;

        /** @brief LDUx/TDULC Link Control Opcode(s) */
        namespace LCO {
            /** @brief LDUx/TDULC Link Control Opcode(s) */
            enum : uint8_t {
                GROUP = 0x00U,                          //! GRP VCH USER - Group Voice Channel User
                GROUP_UPDT = 0x02U,                     //! GRP VCH UPDT - Group Voice Channel Update
                PRIVATE = 0x03U,                        //! UU VCH USER - Unit-to-Unit Voice Channel User
                UU_ANS_REQ = 0x05U,                     //! UU ANS REQ - Unit to Unit Answer Request
                TEL_INT_VCH_USER = 0x06U,               //! TEL INT VCH USER - Telephone Interconnect Voice Channel User / MOT GPS DATA - Motorola In-Band GPS Data
                TEL_INT_ANS_RQST = 0x07U,               //! TEL INT ANS RQST - Telephone Interconnect Answer Request
                EXPLICIT_SOURCE_ID = 0x09U,             //! EXPLICIT SOURCE ID - Explicit Source ID
                CALL_TERM = 0x0FU,                      //! CALL TERM - Call Termination or Cancellation
                IDEN_UP = 0x18U,                        //! IDEN UP - Channel Identifier Update
                SYS_SRV_BCAST = 0x20U,                  //! SYS SRV BCAST - System Service Broadcast
                ADJ_STS_BCAST = 0x22U,                  //! ADJ STS BCAST - Adjacent Site Status Broadcast
                RFSS_STS_BCAST = 0x23U,                 //! RFSS STS BCAST - RFSS Status Broadcast
                NET_STS_BCAST = 0x24U,                  //! NET STS BCAST - Network Status Broadcast
                CONV_FALLBACK = 0x2AU,                  //! CONV FALLBACK - Conventional Fallback

            // LDUx/TDULC Motorola Link Control Opcode(s)
                FAILSOFT = 0x02U                        //! FAILSOFT - Failsoft
            };
        }

        /** @brief TSBK Control Opcode(s) */
        namespace TSBKO {
            /** @brief TSBK Control Opcode(s) */
            enum : uint8_t {
            // TSBK ISP/OSP Shared Opcode(s)
                IOSP_GRP_VCH = 0x00U,                   //! GRP VCH REQ - Group Voice Channel Request (ISP), GRP VCH GRANT - Group Voice Channel Grant (OSP)
                IOSP_UU_VCH = 0x04U,                    //! UU VCH REQ - Unit-to-Unit Voice Channel Request (ISP), UU VCH GRANT - Unit-to-Unit Voice Channel Grant (OSP)
                IOSP_UU_ANS = 0x05U,                    //! UU ANS RSP - Unit-to-Unit Answer Response (ISP), UU ANS REQ - Unit-to-Unit Answer Request (OSP)
                IOSP_TELE_INT_DIAL = 0x08U,             //! TELE INT DIAL REQ - Telephone Interconnect Request - Explicit (ISP), TELE INT DIAL GRANT - Telephone Interconnect Grant (OSP)
                IOSP_TELE_INT_ANS = 0x0AU,              //! TELE INT ANS RSP - Telephone Interconnect Answer Response (ISP), TELE INT ANS REQ - Telephone Interconnect Answer Request (OSP)
                IOSP_STS_UPDT = 0x18U,                  //! STS UPDT REQ - Status Update Request (ISP), STS UPDT - Status Update (OSP)
                IOSP_STS_Q = 0x1AU,                     //! STS Q REQ - Status Query Request (ISP), STS Q - Status Query (OSP)
                IOSP_MSG_UPDT = 0x1CU,                  //! MSG UPDT REQ - Message Update Request (ISP), MSG UPDT - Message Update (OSP)
                IOSP_RAD_MON = 0x1DU,                   //! RAD MON REQ - Radio Unit Monitor Request (ISP), RAD MON CMD - Radio Monitor Command (OSP)
                IOSP_RAD_MON_ENH = 0x1EU,               //! RAD MON ENH REQ - Radio Unit Monitor Enhanced Request (ISP), RAD MON ENH CMD - Radio Unit Monitor Enhanced Command (OSP)
                IOSP_CALL_ALRT = 0x1FU,                 //! CALL ALRT REQ - Call Alert Request (ISP), CALL ALRT - Call Alert (OSP)
                IOSP_ACK_RSP = 0x20U,                   //! ACK RSP U - Acknowledge Response - Unit (ISP), ACK RSP FNE - Acknowledge Response - FNE (OSP)
                IOSP_EXT_FNCT = 0x24U,                  //! EXT FNCT RSP - Extended Function Response (ISP), EXT FNCT CMD - Extended Function Command (OSP)
                IOSP_GRP_AFF = 0x28U,                   //! GRP AFF REQ - Group Affiliation Request (ISP), GRP AFF RSP - Group Affiliation Response (OSP)
                IOSP_U_REG = 0x2CU,                     //! U REG REQ - Unit Registration Request (ISP), U REG RSP - Unit Registration Response (OSP)

            // TSBK Inbound Signalling Packet (ISP) Opcode(s)
                ISP_TELE_INT_PSTN_REQ = 0x09U,          //! TELE INT PSTN REQ - Telephone Interconnect Request - Implicit
                ISP_SNDCP_CH_REQ = 0x12U,               //! SNDCP CH REQ - SNDCP Data Channel Request
                ISP_SNDCP_REC_REQ = 0x14U,              //! SNDCP REC REQ - SNDCP Reconnect Request
                ISP_STS_Q_RSP = 0x19U,                  //! STS Q RSP - Status Query Response
                ISP_STS_Q_REQ = 0x1CU,                  //! STS Q REQ - Status Query Request
                ISP_CAN_SRV_REQ = 0x23U,                //! CAN SRV REQ - Cancel Service Request
                ISP_EMERG_ALRM_REQ = 0x27U,             //! EMERG ALRM REQ - Emergency Alarm Request
                ISP_GRP_AFF_Q_RSP = 0x29U,              //! GRP AFF Q RSP - Group Affiliation Query Response
                ISP_U_DEREG_REQ = 0x2BU,                //! U DE REG REQ - Unit De-Registration Request
                ISP_LOC_REG_REQ = 0x2DU,                //! LOC REG REQ - Location Registration Request
                ISP_AUTH_RESP = 0x38U,                  //! AUTH RESP - Authentication Response
                ISP_AUTH_RESP_M = 0x39U,                //! AUTH RESP M - Authentication Response Mutual
                ISP_AUTH_FNE_RST = 0x3AU,               //! AUTH FNE RST - Authentication FNE Result
                ISP_AUTH_SU_DMD = 0x3BU,                //! AUTH SU DMD - Authentication SU Demand

            // TSBK Outbound Signalling Packet (OSP) Opcode(s)
                OSP_GRP_VCH_GRANT_UPD = 0x02U,          //! GRP VCH GRANT UPD - Group Voice Channel Grant Update
                OSP_UU_VCH_GRANT_UPD = 0x06U,           //! UU VCH GRANT UPD - Unit-to-Unit Voice Channel Grant Update
                OSP_SNDCP_CH_GNT = 0x14U,               //! SNDCP CH GNT - SNDCP Data Channel Grant
                OSP_SNDCP_CH_ANN = 0x16U,               //! SNDCP CH ANN - SNDCP Data Channel Announcement
                OSP_STS_Q = 0x1AU,                      //! STS Q - Status Query
                OSP_QUE_RSP = 0x21U,                    //! QUE RSP - Queued Response
                OSP_DENY_RSP = 0x27U,                   //! DENY RSP - Deny Response
                OSP_SCCB_EXP = 0x29U,                   //! SCCB - Secondary Control Channel Broadcast - Explicit
                OSP_GRP_AFF_Q = 0x2AU,                  //! GRP AFF Q - Group Affiliation Query
                OSP_LOC_REG_RSP = 0x2BU,                //! LOC REG RSP - Location Registration Response
                OSP_U_REG_CMD = 0x2DU,                  //! U REG CMD - Unit Registration Command
                OSP_U_DEREG_ACK = 0x2FU,                //! U DE REG ACK - Unit De-Registration Acknowledge
                OSP_SYNC_BCAST = 0x30U,                 //! SYNC BCAST - Synchronization Broadcast
                OSP_AUTH_DMD = 0x31U,                   //! AUTH DMD - Authentication Demand
                OSP_AUTH_FNE_RESP = 0x32U,              //! AUTH FNE RESP - Authentication FNE Response
                OSP_IDEN_UP_VU = 0x34U,                 //! IDEN UP VU - Channel Identifier Update for VHF/UHF Bands
                OSP_TIME_DATE_ANN = 0x35U,              //! TIME DATE ANN - Time and Date Announcement
                OSP_SYS_SRV_BCAST = 0x38U,              //! SYS SRV BCAST - System Service Broadcast
                OSP_SCCB = 0x39U,                       //! SCCB - Secondary Control Channel Broadcast
                OSP_RFSS_STS_BCAST = 0x3AU,             //! RFSS STS BCAST - RFSS Status Broadcast
                OSP_NET_STS_BCAST = 0x3BU,              //! NET STS BCAST - Network Status Broadcast
                OSP_ADJ_STS_BCAST = 0x3CU,              //! ADJ STS BCAST - Adjacent Site Status Broadcast
                OSP_IDEN_UP = 0x3DU,                    //! IDEN UP - Channel Identifier Update

            // TSBK Motorola Outbound Signalling Packet (OSP) Opcode(s)
                OSP_MOT_GRG_ADD = 0x00U,                //! MOT GRG ADD - Motorola / Group Regroup Add (Patch Supergroup)
                OSP_MOT_GRG_DEL = 0x01U,                //! MOT GRG DEL - Motorola / Group Regroup Delete (Unpatch Supergroup)
                OSP_MOT_GRG_VCH_GRANT = 0x02U,          //! MOT GRG GROUP VCH GRANT / Group Regroup Voice Channel Grant
                OSP_MOT_GRG_VCH_UPD = 0x03U,            //! MOT GRG GROUP VCH GRANT UPD / Group Regroup Voice Channel Grant Update
                OSP_MOT_CC_BSI = 0x0BU,                 //! MOT CC BSI - Motorola / Control Channel Base Station Identifier
                OSP_MOT_PSH_CCH = 0x0EU,                //! MOT PSH CCH - Motorola / Planned Control Channel Shutdown

            // TSBK DVM Outbound Signalling Packet (OSP) Opcode(s)
                OSP_DVM_GIT_HASH = 0x3FU,               //!
            };
        }

        /** @brief Data Unit ID(s) */
        namespace DUID {
            /** @brief Data Unit ID(s) */
            enum E : uint8_t {
                HDU = 0x00U,                            //! Header Data Unit
                TDU = 0x03U,                            //! Simple Terminator Data Unit
                LDU1 = 0x05U,                           //! Logical Link Data Unit 1
                VSELP1 = 0x06U,                         //! Motorola VSELP 1
                TSDU = 0x07U,                           //! Trunking System Data Unit
                VSELP2 = 0x09U,                         //! Motorola VSELP 2
                LDU2 = 0x0AU,                           //! Logical Link Data Unit 2
                PDU = 0x0CU,                            //! Packet Data Unit
                TDULC = 0x0FU                           //! Terminator Data Unit with Link Control
            };
        }

        /** @} */

    #define P25_HDU_STR     "P25, HDU (Header Data Unit)"
    #define P25_TDU_STR     "P25, TDU (Simple Terminator Data Unit)"
    #define P25_LDU1_STR    "P25, LDU1 (Logical Link Data Unit 1)"
    #define P25_VSELP1_STR  "P25, VSELP1 (VSELP Data Unit 1)"
    #define P25_TSDU_STR    "P25, TSDU (Trunking System Data Unit)"
    #define P25_VSELP2_STR  "P25, VSELP2 (VSELP Data Unit 2)"
    #define P25_LDU2_STR    "P25, LDU2 (Logical Link Data Unit 2)"
    #define P25_PDU_STR     "P25, PDU (Packet Data Unit)"
    #define P25_TDULC_STR   "P25, TDULC (Terminator Data Unit with Link Control)"
    } // namespace defines
} // namespace p25

#endif // __P25_DEFINES_H__
