/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2018-2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_DATA__DATA_BLOCK_H__)
#define  __P25_DATA__DATA_BLOCK_H__

#include "Defines.h"
#include "p25/data/DataHeader.h"
#include "p25/edac/Trellis.h"

#include <string>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents a data block for PDU P25 packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API DataBlock {
        public:
            /// <summary>Initializes a new instance of the DataBlock class.</summary>
            DataBlock();
            /// <summary>Finalizes a instance of the DataBlock class.</summary>
            ~DataBlock();

            /// <summary>Decodes P25 PDU data block.</summary>
            bool decode(const uint8_t* data, const DataHeader header);
            /// <summary>Encodes a P25 PDU data block.</summary>
            void encode(uint8_t* data);

            /// <summary>Sets the data format.</summary>
            void setFormat(const uint8_t fmt);
            /// <summary>Sets the data format from the data header.</summary>
            void setFormat(const DataHeader header);
            /// <summary>Gets the data format.</summary>
            uint8_t getFormat() const;

            /// <summary>Sets the raw data stored in the data block.</summary>
            void setData(const uint8_t* buffer);
            /// <summary>Gets the raw data stored in the data block.</summary>
            uint32_t getData(uint8_t* buffer) const;

        public:
            /// <summary>Sets the data block serial number.</summary>
            __PROPERTY(uint8_t, serialNo, SerialNo);

            /// <summary>Flag indicating this is the last block in a sequence of block.</summary>
            __PROPERTY(bool, lastBlock, LastBlock);

            /// <summary>Logical link ID.</summary>
            __PROPERTY(uint32_t, llId, LLId);
            /// <summary>Service access point.</summary>
            __PROPERTY(uint8_t, sap, SAP);

        private:
            edac::Trellis m_trellis;

            uint8_t m_fmt;
            uint8_t m_headerSap;

            uint8_t* m_data;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__DATA_BLOCK_H__
