/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
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

#include "common/Defines.h"
#include "common/p25/data/LowSpeedData.h"
#include "common/p25/lc/LC.h"
#include "common/edac/RS634717.h"

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

            /// <summary>Helper to set the LC data.</summary>
            void setControl(const lc::LC& data);

            /// <summary>Decode a logical link data unit 1.</summary>
            bool decodeLDU1(const uint8_t* data, uint8_t* imbe);
            /// <summary>Encode a logical link data unit 1.</summary>
            void encodeLDU1(uint8_t* data, const uint8_t* imbe);

            /// <summary>Decode a logical link data unit 2.</summary>
            bool decodeLDU2(const uint8_t* data, uint8_t* imbe);
            /// <summary>Encode a logical link data unit 2.</summary>
            void encodeLDU2(uint8_t* data, const uint8_t* imbe);

        public:
            /** Common Data */
            /// <summary>Frame Type.</summary>
            __PROPERTY(uint8_t, frameType, FrameType);

            /// <summary>RSSI.</summary>
            __PROPERTY(uint8_t, rssi, RSSI);

            /// <summary>Link control data.</summary>
            __READONLY_PROPERTY_PLAIN(p25::lc::LC*, control);
            /// <summary>Low speed data.</summary>
            __READONLY_PROPERTY_PLAIN(p25::data::LowSpeedData*, lsd);

        private:
            edac::RS634717 m_rs;

            /** Encryption data */
            uint8_t* m_mi;

            /// <summary>Internal helper to copy the class.</summary>
            void copy(const LC& data);
        };
    } // namespace dfsi
} // namespace p25

#endif // __P25_DFSI__LC_H__
