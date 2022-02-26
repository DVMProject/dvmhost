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
#if !defined(__P25_DFSI__LC_H__)
#define  __P25_DFSI__LC_H__

#include "Defines.h"
#include "p25/data/LowSpeedData.h"
#include "p25/lc/LC.h"
#include "p25/lc/TSBK.h"

#include <string>

namespace p25
{
    namespace dfsi
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents link control data for DFSI VHDR, LDU1 and 2 packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API LC {
        public:
            /// <summary>Initializes a new instance of the LC class.</summary>
            LC();
            /// <summary>Initializes a copy instance of the LC class.</summary>
            LC(const LC& data);
            /// <summary>Initializes a new instance of the LC class.</summary>
            LC(const p25::lc::LC& control, const p25::data::LowSpeedData& lsd);
            /// <summary>Finalizes a instance of the LC class.</summary>
            ~LC();

            /// <summary>Equals operator.</summary>
            LC& operator=(const LC& data);

            /// <summary>Decode a NID start/stop.</summary>
            bool decodeNID(const uint8_t* data);
            /// <summary>Encode a NID start/stop.</summary>
            void encodeNID(uint8_t* data);

            /// <summary>Decode a voice header 1.</summary>
            bool decodeVHDR1(const uint8_t* data);
            /// <summary>Encode a voice header 1.</summary>
            void encodeVHDR1(uint8_t* data);

            /// <summary>Decode a voice header 2.</summary>
            bool decodeVHDR2(const uint8_t* data);
            /// <summary>Encode a voice header 2.</summary>
            void encodeVHDR2(uint8_t* data);

            /// <summary>Decode a logical link data unit 1.</summary>
            bool decodeLDU1(const uint8_t* data, uint8_t* imbe);
            /// <summary>Encode a logical link data unit 1.</summary>
            void encodeLDU1(uint8_t* data, const uint8_t* imbe);

            /// <summary>Decode a logical link data unit 2.</summary>
            bool decodeLDU2(const uint8_t* data, uint8_t* imbe);
            /// <summary>Encode a logical link data unit 2.</summary>
            void encodeLDU2(uint8_t* data, const uint8_t* imbe);

            /// <summary>Decode a TSBK.</summary>
            bool decodeTSBK(const uint8_t* data);
            /// <summary>Encode a TSBK.</summary>
            void encodeTSBK(uint8_t* data);

        public:
            /** Common Data */
            /// <summary>Frame Type.</summary>
            __PROPERTY(uint8_t, frameType, FrameType);
            /// <summary>RT Mode Flag.</summary>
            __PROPERTY(uint8_t, rtModeFlag, RTMode);
            /// <summary>Start/Stop Flag.</summary>
            __PROPERTY(uint8_t, startStopFlag, StartStop);
            /// <summary>Type Flag.</summary>
            __PROPERTY(uint8_t, typeFlag, Type);
            /// <summary>ICW Flag.</summary>
            __PROPERTY(uint8_t, icwFlag, ICW);

            /// <summary>RSSI.</summary>
            __PROPERTY(uint8_t, rssi, RSSI);

            /// <summary>Source.</summary>
            __PROPERTY(uint8_t, source, Source);

            /// <summary>Link control data.</summary>
            __PROPERTY_PLAIN(p25::lc::LC, control, control);
            /// <summary>TSBK.</summary>
            __PROPERTY_PLAIN(p25::lc::TSBK, tsbk, tsbk);
            /// <summary>Low speed data.</summary>
            __PROPERTY_PLAIN(p25::data::LowSpeedData, lsd, lsd);

        private:
            /** Encryption data */
            uint8_t* m_mi;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const LC& data);

            /// <summary>Decode start record data.</summary>
            bool decodeStart(const uint8_t* data);
            /// <summary>Encode start record data.</summary>
            void encodeStart(uint8_t* data);
        };
    } // namespace dfsi
} // namespace p25

#endif // __P25_DFSI__LC_H__
