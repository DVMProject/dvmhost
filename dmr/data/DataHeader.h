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
*   Copyright (C) 2015,2016,2017 by Jonathan Naylor G4KLX
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
#if !defined(__DMR_DATA__DATA_HEADER_H__)
#define __DMR_DATA__DATA_HEADER_H__

#include "Defines.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents a DMR data header.
        // ---------------------------------------------------------------------------

        class HOST_SW_API DataHeader {
        public:
            /// <summary>Initializes a new instance of the DataHeader class.</summary>
            DataHeader();
            /// <summary>Finalizes a instance of the DataHeader class.</summary>
            ~DataHeader();

            /// <summary>Equals operator.</summary>
            DataHeader& operator=(const DataHeader& header);

            /// <summary>Decodes a DMR data header.</summary>
            bool decode(const uint8_t* bytes);
            /// <summary>Encodes a DMR data header.</summary>
            void encode(uint8_t* bytes) const;

        public:
            /// <summary>Flag indicating whether the CSBK is group or individual.</summary>
            __READONLY_PROPERTY(bool, GI, GI);

            /// <summary>Source ID.</summary>
            __READONLY_PROPERTY(uint32_t, srcId, SrcId);
            /// <summary>Destination ID.</summary>
            __READONLY_PROPERTY(uint32_t, dstId, DstId);

            /// <summary>Gets the number of data blocks following the header.</summary>
            __READONLY_PROPERTY(uint32_t, blocks, Blocks);

        private:
            uint8_t* m_data;

            bool m_A;

            bool m_F;
            bool m_S;
            uint8_t m_Ns;
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__DATA_HEADER_H__
