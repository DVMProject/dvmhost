/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2016 by Jonathan Naylor, G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; version 2 of the License.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*/
#if !defined(__DMR_EDAC__TRELLIS_H__)
#define __DMR_EDAC__TRELLIS_H__

#include "Defines.h"

namespace dmr
{
    namespace edac
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Implements 3/4 rate Trellis for DMR.
        // ---------------------------------------------------------------------------

        class HOST_SW_API Trellis {
        public:
            /// <summary>Initializes a new instance of the Trellis class.</summary>
            Trellis();
            /// <summary>Finalizes a instance of the Trellis class.</summary>
            ~Trellis();

            /// <summary>Decodes 3/4 rate Trellis.</summary>
            bool decode(const uint8_t* data, uint8_t* payload);
            /// <summary>Encodes 3/4 rate Trellis.</summary>
            void encode(const uint8_t* payload, uint8_t* data);

        private:
            /// <summary></summary>
            void deinterleave(const uint8_t* in, int8_t* dibits) const;
            /// <summary></summary>
            void interleave(const int8_t* dibits, uint8_t* out) const;
            /// <summary></summary>
            void dibitsToPoints(const int8_t* dibits, uint8_t* points) const;
            /// <summary></summary>
            void pointsToDibits(const uint8_t* points, int8_t* dibits) const;
            /// <summary></summary>
            void bitsToTribits(const uint8_t* payload, uint8_t* tribits) const;
            /// <summary></summary>
            void tribitsToBits(const uint8_t* tribits, uint8_t* payload) const;

            /// <summary></summary>
            bool fixCode(uint8_t* points, uint32_t failPos, uint8_t* payload) const;
            /// <summary></summary>
            uint32_t checkCode(const uint8_t* points, uint8_t* tribits) const;
        };
    } // namespace edac
} // namespace dmr

#endif // __DMR_EDAC__TRELLIS_H__
