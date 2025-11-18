// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup p25_dfsi Digital Fixed Station Interface
 * @brief Implementation for the data handling of the TIA-102.BAHA Project 25 standard.
 * @ingroup p25
 * 
 * @file DFSIDefines.h
 * @ingroup p25_dfsi
 */
#if !defined(__DFSI_DEFINES_H__)
#define  __DFSI_DEFINES_H__

#include "common/Defines.h"

// Frame Type String(s)
#define P25_DFSI_SS_STR "P25, DFSI_START_STOP (Start/Stop)"
#define P25_DFSI_VHDR1_STR "P25, DFSI_VHDR1 (Voice Header 1)"
#define P25_DFSI_VHDR2_STR "P25, DFSI_VHDR2 (Voice Header 2)"
#define P25_DFSI_LDU1_STR "P25, DFSI_LDU1 (Logical Link Data Unit 1)"
#define P25_DFSI_LDU2_STR "P25, DFSI_LDU2 (Logical Link Data Unit 2)"
#define P25_DFSI_TSBK_STR "P25, DFSI_TSBK (Trunking System Block)"

// Shorthand macro to p25::dfsi::defines -- keeps source code that doesn't use "using" concise
#define P25DFSIDEF p25::dfsi::defines
namespace p25
{
    namespace dfsi
    {
        namespace defines
        {
            // ---------------------------------------------------------------------------
            //  Constants
            // ---------------------------------------------------------------------------

            const uint32_t  DFSI_VHDR_RAW_LEN = 36U;
            const uint32_t  DFSI_VHDR_LEN = 27U;

            const uint32_t  DFSI_MOT_START_LEN = 9U;
            const uint32_t  DFSI_MOT_VHDR_1_LEN = 30U;
            const uint32_t  DFSI_MOT_VHDR_2_LEN = 22U;
            const uint32_t  DFSI_MOT_TSBK_LEN = 24U;
            const uint32_t  DFSI_MOT_TDULC_LEN = 21U;

            const uint32_t  DFSI_PDU_BLOCK_CNT = 4U;

            const uint32_t  DFSI_TIA_VHDR_LEN = 22U;

            const uint32_t  DFSI_MOT_ICW_LENGTH = 6U;

            const uint32_t  DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES = 22U;
            const uint32_t  DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES = 14U;
            const uint32_t  DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES = 16U;

            const uint32_t  DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES = 22U;
            const uint32_t  DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES = 14U;
            const uint32_t  DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES = 17U;
            const uint32_t  DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES = 16U;

            const uint32_t  DFSI_P2_4V_FRAME_LENGTH_BYTES = 39U;
            const uint32_t  DFSI_P2_2V_FRAME_LENGTH_BYTES = 32U;

            /**
             * @addtogroup p25_dfsi
             * @{
             */

            const uint8_t   DFSI_RTP_PAYLOAD_TYPE = 0x64U;              //!< 
            const uint8_t   DFSI_RTP_MOT_PAYLOAD_TYPE = 0x5DU;          //!< 
            const uint8_t   DFSI_RTP_MOT_DATA_PAYLOAD_TYPE = 0x5EU;     //!< 

            const uint8_t   DFSI_RTP_SEQ_HANDSHAKE = 0x00U;             //!< 
            const uint8_t   DFSI_RTP_SEQ_STARTSTOP = 0x01U;             //!< 

            const uint8_t   DFSI_MOT_ICW_FMT_TYPE3 = 0x02U;             //!< 

            const uint8_t   DFSI_MOT_ICW_PARM_NOP = 0x00U;              //!< No Operation
            const uint8_t   DSFI_MOT_ICW_PARM_PAYLOAD = 0x0CU;          //!< Stream Payload
            const uint8_t   DFSI_MOT_ICW_PARM_RSSI1 = 0x1AU;            //!< RSSI Data
            const uint8_t   DFSI_MOT_ICW_PARM_RSSI2 = 0x1BU;            //!< RSSI Data
            const uint8_t   DFSI_MOT_ICW_PARM_STOP = 0x25U;             //!< Stop Stream
            const uint8_t   DFSI_MOT_ICW_TX_ADDRESS = 0x2CU;            //!< Tx Device Address
            const uint8_t   DFSI_MOT_ICW_RX_ADDRESS = 0x35U;            //!< Rx Device Address

            const uint8_t   DFSI_BUSY_BITS_TALKAROUND = 0x00U;          //!< Talkaround
            const uint8_t   DFSI_BUSY_BITS_BUSY = 0x01U;                //!< Busy
            const uint8_t   DFSI_BUSY_BITS_INBOUND = 0x02U;             //!< Inbound
            const uint8_t   DFSI_BUSY_BITS_IDLE = 0x03U;                //!< Idle

            /** @brief DFSI Frame Type */
            namespace DFSIFrameType {
                /** @brief DFSI Frame Type */
                enum E : uint8_t {
                    MOT_START_STOP = 0x00U,             //!< Motorola/V.24 Start/Stop Stream

                    MOT_VHDR_1 = 0x60U,                 //!< Motorola/V.24 Voice Header 1
                    MOT_VHDR_2 = 0x61U,                 //!< Motorola/V.24 Voice Header 2

                    LDU1_VOICE1 = 0x62U,                //!< IMBE LDU1 - Voice 1
                    LDU1_VOICE2 = 0x63U,                //!< IMBE LDU1 - Voice 2
                    LDU1_VOICE3 = 0x64U,                //!< IMBE LDU1 - Voice 3 + Link Control
                    LDU1_VOICE4 = 0x65U,                //!< IMBE LDU1 - Voice 4 + Link Control
                    LDU1_VOICE5 = 0x66U,                //!< IMBE LDU1 - Voice 5 + Link Control
                    LDU1_VOICE6 = 0x67U,                //!< IMBE LDU1 - Voice 6 + Link Control
                    LDU1_VOICE7 = 0x68U,                //!< IMBE LDU1 - Voice 7 + Link Control
                    LDU1_VOICE8 = 0x69U,                //!< IMBE LDU1 - Voice 8 + Link Control
                    LDU1_VOICE9 = 0x6AU,                //!< IMBE LDU1 - Voice 9 + Low Speed Data

                    LDU2_VOICE10 = 0x6BU,               //!< IMBE LDU2 - Voice 10
                    LDU2_VOICE11 = 0x6CU,               //!< IMBE LDU2 - Voice 11
                    LDU2_VOICE12 = 0x6DU,               //!< IMBE LDU2 - Voice 12 + Encryption Sync
                    LDU2_VOICE13 = 0x6EU,               //!< IMBE LDU2 - Voice 13 + Encryption Sync
                    LDU2_VOICE14 = 0x6FU,               //!< IMBE LDU2 - Voice 14 + Encryption Sync
                    LDU2_VOICE15 = 0x70U,               //!< IMBE LDU2 - Voice 15 + Encryption Sync
                    LDU2_VOICE16 = 0x71U,               //!< IMBE LDU2 - Voice 16 + Encryption Sync
                    LDU2_VOICE17 = 0x72U,               //!< IMBE LDU2 - Voice 17 + Encryption Sync
                    LDU2_VOICE18 = 0x73U,               //!< IMBE LDU2 - Voice 18 + Low Speed Data

                    MOT_TDULC = 0x74U,                  //!< Motorola/V.24 TDULC

                    MOT_PDU_UNCONF_HEADER = 0x80U,      //!< Motorola/V.24 PDU (Unconfirmed Block Header)
                    MOT_PDU_UNCONF_BLOCK_1 = 0x81U,     //!< Motorola/V.24 PDU (Unconfirmed Block 1)
                    MOT_PDU_UNCONF_BLOCK_2 = 0x82U,     //!< Motorola/V.24 PDU (Unconfirmed Block 2)
                    MOT_PDU_UNCONF_BLOCK_3 = 0x83U,     //!< Motorola/V.24 PDU (Unconfirmed Block 3)
                    MOT_PDU_UNCONF_BLOCK_4 = 0x84U,     //!< Motorola/V.24 PDU (Unconfirmed Block 4)
                    MOT_PDU_UNCONF_END = 0x85U,         //!< Motorola/V.24 PDU (Unconfirmed Block End)
                    MOT_PDU_SINGLE_UNCONF = 0x87U,      //!< Motorola/V.24 PDU (Single Unconfirmed Block)

                    MOT_PDU_CONF_HEADER = 0x88U,        //!< Motorola/V.24 PDU (Confirmed Block Header)
                    MOT_PDU_CONF_BLOCK_1 = 0x89U,       //!< Motorola/V.24 PDU (Confirmed Block 1)
                    MOT_PDU_CONF_BLOCK_2 = 0x8AU,       //!< Motorola/V.24 PDU (Confirmed Block 2)
                    MOT_PDU_CONF_BLOCK_3 = 0x8BU,       //!< Motorola/V.24 PDU (Confirmed Block 3)
                    MOT_PDU_CONF_BLOCK_4 = 0x8CU,       //!< Motorola/V.24 PDU (Confirmed Block 4)
                    MOT_PDU_CONF_END = 0x8DU,           //!< Motorola/V.24 PDU (Confirmed Block End)
                    MOT_PDU_SINGLE_CONF = 0x8FU,        //!< Motorola/V.24 PDU (Single Confirmed Block)

                    MOT_TSBK = 0xA1U,                   //!< Motorola/V.24 TSBK (Single Block)

                    P2_4VA = 0xF0U,                     //!< AMBE - Inbound/Outbound 4Va (P25 Phase 2)
                    P2_4VB = 0xF1U,                     //!< AMBE - Inbound/Outbound 4Vb (P25 Phase 2)
                    P2_4VC = 0xF2U,                     //!< AMBE - Inbound/Outbound 4Vc (P25 Phase 2)
                    P2_4VD = 0xF3U,                     //!< AMBE - Inbound/Outbound 4Vd (P25 Phase 2)
                    P2_2V = 0xF4U,                      //!< AMBE - Inbound/Outbound 2V (P25 Phase 2)
                };
            }

            /** @} */
        } // namespace defines
    } // namespace dfsi
} // namespace p25

#endif // __DFSI_DEFINES_H__
