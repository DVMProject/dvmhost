// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2022-2024 Bryan Biedenkapp, N2PLL
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

            /**
             * @addtogroup p25_dfsi
             * @{
             */

            const uint8_t   DFSI_RTP_PAYLOAD_TYPE = 0x64U;  //!

            const uint8_t   DFSI_STATUS_NO_ERROR = 0x00U;   //!
            const uint8_t   DFSI_STATUS_ERASE = 0x02U;      //!

            const uint8_t   DFSI_RT_ENABLED = 0x02U;        //!
            const uint8_t   DFSI_RT_DISABLED = 0x04U;       //!

            const uint8_t   DFSI_START_FLAG = 0x0CU;        //!
            const uint8_t   DFSI_STOP_FLAG = 0x25U;         //!

            const uint8_t   DFSI_TYPE_DATA_PAYLOAD = 0x06U; //!
            const uint8_t   DFSI_TYPE_VOICE = 0x0BU;        //!

            const uint8_t   DFSI_DEF_ICW_SOURCE = 0x00U;    //! Infrastructure Source - Default Source
            const uint8_t   DFSI_DEF_SOURCE = 0x00U;        //!

            /** @brief DFSI Frame Type */
            namespace DFSIFrameType {
                /** @brief DFSI Frame Type */
                enum E : uint8_t {
                    MOT_START_STOP = 0x00U,     // Motorola Start/Stop

                    MOT_VHDR_1 = 0x60U,         // Motorola Voice Header 1
                    MOT_VHDR_2 = 0x61U,         // Motorola Voice Header 2

                    LDU1_VOICE1 = 0x62U,        // IMBE LDU1 - Voice 1
                    LDU1_VOICE2 = 0x63U,        // IMBE LDU1 - Voice 2
                    LDU1_VOICE3 = 0x64U,        // IMBE LDU1 - Voice 3 + Link Control
                    LDU1_VOICE4 = 0x65U,        // IMBE LDU1 - Voice 4 + Link Control
                    LDU1_VOICE5 = 0x66U,        // IMBE LDU1 - Voice 5 + Link Control
                    LDU1_VOICE6 = 0x67U,        // IMBE LDU1 - Voice 6 + Link Control
                    LDU1_VOICE7 = 0x68U,        // IMBE LDU1 - Voice 7 + Link Control
                    LDU1_VOICE8 = 0x69U,        // IMBE LDU1 - Voice 8 + Link Control
                    LDU1_VOICE9 = 0x6AU,        // IMBE LDU1 - Voice 9 + Low Speed Data

                    LDU2_VOICE10 = 0x6BU,       // IMBE LDU2 - Voice 10
                    LDU2_VOICE11 = 0x6CU,       // IMBE LDU2 - Voice 11
                    LDU2_VOICE12 = 0x6DU,       // IMBE LDU2 - Voice 12 + Encryption Sync
                    LDU2_VOICE13 = 0x6EU,       // IMBE LDU2 - Voice 13 + Encryption Sync
                    LDU2_VOICE14 = 0x6FU,       // IMBE LDU2 - Voice 14 + Encryption Sync
                    LDU2_VOICE15 = 0x70U,       // IMBE LDU2 - Voice 15 + Encryption Sync
                    LDU2_VOICE16 = 0x71U,       // IMBE LDU2 - Voice 16 + Encryption Sync
                    LDU2_VOICE17 = 0x72U,       // IMBE LDU2 - Voice 17 + Encryption Sync
                    LDU2_VOICE18 = 0x73U,       // IMBE LDU2 - Voice 18 + Low Speed Data

                    PDU = 0x87U,                // PDU
                    TSBK = 0xA1U                // TSBK
                };
            }

            /** @} */
        } // namespace defines
    } // namespace dfsi
} // namespace p25

#endif // __DFSI_DEFINES_H__
