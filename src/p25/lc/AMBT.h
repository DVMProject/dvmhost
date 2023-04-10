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
#if !defined(__P25_LC__AMBT_H__)
#define  __P25_LC__AMBT_H__

#include "Defines.h"
#include "p25/lc/TSBK.h"
#include "p25/data/DataHeader.h"
#include "p25/data/DataBlock.h"

namespace p25
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents link control data for Alternate Trunking packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API AMBT : public TSBK {
        public:
            /// <summary>Initializes a new instance of the AMBT class.</summary>
            AMBT();

            /// <summary>Decode a alternate trunking signalling block.</summary>
            virtual bool decodeMBT(const data::DataHeader dataHeader, const data::DataBlock* blocks) = 0;
            /// <summary>Encode a alternate trunking signalling block.</summary>
            virtual void encodeMBT(data::DataHeader& dataHeader, uint8_t* pduUserData) = 0;

            /// <summary>Decode a trunking signalling block.</summary>
            virtual bool decode(const uint8_t* data, bool rawTSBK = false);
            /// <summary>Encode a trunking signalling block.</summary>
            virtual void encode(uint8_t* data, bool rawTSBK = false, bool noTrellis = false);

        protected:
            /// <summary>Internal helper to convert TSBK bytes to a 64-bit long value.</summary>
            static ulong64_t toValue(const data::DataHeader dataHeader, const uint8_t* pduUserData);

            /// <summary>Internal helper to decode a trunking signalling block.</summary>
            bool decode(const data::DataHeader dataHeader, const data::DataBlock* blocks, uint8_t* pduUserData);
            /// <summary>Internal helper to encode a trunking signalling block.</summary>
            void encode(data::DataHeader& dataHeader, uint8_t* pduUserData);
        };
    } // namespace lc
} // namespace p25

#endif // __P25_LC__AMBT_H__
