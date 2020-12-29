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
*   Copyright (C) 2020 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_DATA__DATA_RSP_HEADER_H__)
#define  __P25_DATA__DATA_RSP_HEADER_H__

#include "Defines.h"
#include "p25/edac/Trellis.h"

#include <string>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents the data response header for PDU P25 packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API DataRspHeader {
        public:
            /// <summary>Initializes a new instance of the DataRspHeader class.</summary>
            DataRspHeader();
            /// <summary>Finalizes a instance of the DataRspHeader class.</summary>
            ~DataRspHeader();

            /// <summary>Decodes P25 PDU data response header.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encodes P25 PDU data response header.</summary>
            void encode(uint8_t* data);

            /// <summary>Helper to reset data values to defaults.</summary>
            void reset();

            /// <summary>Gets the total number of data octets.</summary>
            uint32_t getDataOctets() const;

            /** Common Data */
            /// <summary>Sets the total number of blocks to follow this header.</summary>
            void setBlocksToFollow(uint8_t blocksToFollow);
            /// <summary>Gets the total number of blocks to follow this header.</summary>
            uint8_t getBlocksToFollow() const;

        public:
            /// <summary>Flag indicating if this is an outbound data packet.</summary>
            __PROPERTY(bool, outbound, Outbound);
            /// <summary>Response class.</summary>
            __PROPERTY(uint8_t, rspClass, Class);
            /// <summary>Response type.</summary>
            __PROPERTY(uint8_t, rspType, Type);
            /// <summary>Response status.</summary>
            __PROPERTY(uint8_t, rspStatus, Status);
            /// <summary>Manufacturer ID.</summary>
            __PROPERTY(uint8_t, mfId, MFId);
            /// <summary>Logical link ID.</summary>
            __PROPERTY(uint32_t, llId, LLId);
            /// <summary>Source Logical link ID.</summary>
            __PROPERTY(uint32_t, srcLlId, SrcLLId);
            /// <summary>Flag indicating whether or not this response packet is to extended addressing.</summary>
            __PROPERTY(bool, extended, Extended);

        private:
            edac::Trellis m_trellis;

            uint8_t m_blocksToFollow;
            uint32_t m_dataOctets;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__DATA_RSP_HEADER_H__
