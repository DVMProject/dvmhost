/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__P25_DFSI_DEFINES_H__)
#define  __P25_DFSI_DEFINES_H__

#include "Defines.h"

// Frame Type String(s)
#define P25_DFSI_SS_STR "P25_DFSI_START_STOP (Start/Stop)"
#define P25_DFSI_VHDR1_STR "P25_DFSI_VHDR1 (Voice Header 1)"
#define P25_DFSI_VHDR2_STR "P25_DFSI_VHDR2 (Voice Header 2)"
#define P25_DFSI_LDU1_STR "P25_DFSI_LDU1 (Logical Link Data Unit 1)"
#define P25_DFSI_LDU2_STR "P25_DFSI_LDU2 (Logical Link Data Unit 2)"
#define P25_DFSI_TSBK_STR "P25_DFSI_TSBK (Trunking System Block)"

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        const uint32_t  P25_DFSI_START_LENGTH_BYTES = 4U;

        const uint32_t  P25_DFSI_SS_FRAME_LENGTH_BYTES = 10U;

        const uint32_t  P25_DFSI_VHDR1_FRAME_LENGTH_BYTES = 30U;
        const uint32_t  P25_DFSI_VHDR2_FRAME_LENGTH_BYTES = 22U;

        const uint32_t  P25_DFSI_LDU1_VOICE1_FRAME_LENGTH_BYTES = 22U;
        const uint32_t  P25_DFSI_LDU1_VOICE2_FRAME_LENGTH_BYTES = 14U;
        const uint32_t  P25_DFSI_LDU1_VOICE3_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU1_VOICE4_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU1_VOICE5_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU1_VOICE6_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU1_VOICE7_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU1_VOICE8_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU1_VOICE9_FRAME_LENGTH_BYTES = 16U;

        const uint32_t  P25_DFSI_LDU2_VOICE10_FRAME_LENGTH_BYTES = 22U;
        const uint32_t  P25_DFSI_LDU2_VOICE11_FRAME_LENGTH_BYTES = 14U;
        const uint32_t  P25_DFSI_LDU2_VOICE12_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU2_VOICE13_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU2_VOICE14_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU2_VOICE15_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU2_VOICE16_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU2_VOICE17_FRAME_LENGTH_BYTES = 17U;
        const uint32_t  P25_DFSI_LDU2_VOICE18_FRAME_LENGTH_BYTES = 16U;

        const uint32_t  P25_DFSI_TSBK_FRAME_LENGTH_BYTES = 25U;

        const uint8_t   P25_DFSI_STATUS = 0x02U;            //

        const uint8_t   P25_DFSI_RT_ENABLED = 0x02U;        //
        const uint8_t   P25_DFSI_RT_DISABLED = 0x04U;       //

        const uint8_t   P25_DFSI_START_FLAG = 0x0CU;        //
        const uint8_t   P25_DFSI_STOP_FLAG = 0x25U;         //

        const uint8_t   P25_DFSI_TYPE_DATA_PAYLOAD = 0x06U; //
        const uint8_t   P25_DFSI_TYPE_VOICE = 0x0BU;        //
        const uint8_t   P25_DFSI_TYPE_DATA = 0x0CU;         //
        const uint8_t   P25_DFSI_TYPE_TSBK = 0x0FU;         //

        const uint8_t   P25_DFSI_DEF_ICW_SOURCE = 0x00U;    // Infrastructure Source - Default Source
        const uint8_t   P25_DFSI_DEF_SOURCE = 0x00U;        //

        // Frame Type(s)
        const uint8_t   P25_DFSI_START_STOP = 0x00U;        // Start/Stop

        const uint8_t   P25_DFSI_VHDR1 = 0x60U;             // Voice Header 1
        const uint8_t   P25_DFSI_VHDR2 = 0x61U;             // Voice Header 2

        const uint8_t   P25_DFSI_LDU1_VOICE1 = 0x62U;       // IMBE LDU1 - Voice 1
        const uint8_t   P25_DFSI_LDU1_VOICE2 = 0x63U;       // IMBE LDU1 - Voice 2
        const uint8_t   P25_DFSI_LDU1_VOICE3 = 0x64U;       // IMBE LDU1 - Voice 3 + Link Control
        const uint8_t   P25_DFSI_LDU1_VOICE4 = 0x65U;       // IMBE LDU1 - Voice 4 + Link Control
        const uint8_t   P25_DFSI_LDU1_VOICE5 = 0x66U;       // IMBE LDU1 - Voice 5 + Link Control
        const uint8_t   P25_DFSI_LDU1_VOICE6 = 0x67U;       // IMBE LDU1 - Voice 6 + Link Control
        const uint8_t   P25_DFSI_LDU1_VOICE7 = 0x68U;       // IMBE LDU1 - Voice 7 + Link Control
        const uint8_t   P25_DFSI_LDU1_VOICE8 = 0x69U;       // IMBE LDU1 - Voice 8 + Link Control
        const uint8_t   P25_DFSI_LDU1_VOICE9 = 0x6AU;       // IMBE LDU1 - Voice 9 + Low Speed Data

        const uint8_t   P25_DFSI_LDU2_VOICE10 = 0x6BU;      // IMBE LDU2 - Voice 10
        const uint8_t   P25_DFSI_LDU2_VOICE11 = 0x6CU;      // IMBE LDU2 - Voice 11
        const uint8_t   P25_DFSI_LDU2_VOICE12 = 0x6DU;      // IMBE LDU2 - Voice 12 + Encryption Sync
        const uint8_t   P25_DFSI_LDU2_VOICE13 = 0x6EU;      // IMBE LDU2 - Voice 13 + Encryption Sync
        const uint8_t   P25_DFSI_LDU2_VOICE14 = 0x6FU;      // IMBE LDU2 - Voice 14 + Encryption Sync
        const uint8_t   P25_DFSI_LDU2_VOICE15 = 0x70U;      // IMBE LDU2 - Voice 15 + Encryption Sync
        const uint8_t   P25_DFSI_LDU2_VOICE16 = 0x71U;      // IMBE LDU2 - Voice 16 + Encryption Sync
        const uint8_t   P25_DFSI_LDU2_VOICE17 = 0x72U;      // IMBE LDU2 - Voice 17 + Encryption Sync
        const uint8_t   P25_DFSI_LDU2_VOICE18 = 0x73U;      // IMBE LDU2 - Voice 18 + Low Speed Data

        const uint8_t   P25_DFSI_TSBK = 0xA1U;              // TSBK
    } // namespace dfsi
} // namespace p25

#endif // __P25_DFSI_DEFINES_H__
