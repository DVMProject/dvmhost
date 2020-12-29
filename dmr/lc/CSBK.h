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
*   Copyright (C) 2015,2016 by Jonathan Naylor G4KLX
*   Copyright (C) 2019 by Bryan Biedenkapp N2PLL
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
#if !defined(__DMR_LC__CSBK_H__)
#define __DMR_LC__CSBK_H__

#include "Defines.h"
#include "dmr/DMRDefines.h"

namespace dmr
{
    namespace lc
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents DMR control signalling block data.
        // ---------------------------------------------------------------------------

        class HOST_SW_API CSBK {
        public:
            /// <summary>Initializes a new instance of the CSBK class.</summary>
            CSBK();
            /// <summary>Finalizes a instance of the CSBK class.</summary>
            ~CSBK();

            /// <summary>Decodes a DMR CSBK.</summary>
            bool decode(const uint8_t* bytes);
            /// <summary>Encodes a DMR CSBK.</summary>
            void encode(uint8_t* bytes) const;

        public:
            /// <summary></summary>
            __PROPERTY(bool, verbose, Verbose);

            // Generic fields
            /// <summary>CSBK opcode.</summary>
            __PROPERTY(uint8_t, CSBKO, CSBKO);
            /// <summary>CSBK feature ID.</summayr>
            __PROPERTY(uint8_t, FID, FID);

            // For BS Dwn Act
            __READONLY_PROPERTY(uint32_t, bsId, BSId);

            // For Pre
            /// <summary>Flag indicating whether the CSBK is group or individual.</summary>
            __PROPERTY(bool, GI, GI);

            /// <summary>Source ID.</summary>
            __PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __PROPERTY(uint32_t, dstId, DstId);

            /// <summary></summary>
            __READONLY_PROPERTY(bool, dataContent, DataContent);

            /// <summary>Sets the number of blocks to follow.</summary>
            __PROPERTY(uint8_t, CBF, CBF);

        private:
            uint8_t* m_data;
        };
    } // namespace lc
} // namespace dmr

#endif // __DMR_LC__CSBK_H__
