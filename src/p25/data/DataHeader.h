/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
/*
*   Copyright (C) 2018,2022 by Bryan Biedenkapp N2PLL
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
#if !defined(__P25_DATA__DATA_HEADER_H__)
#define  __P25_DATA__DATA_HEADER_H__

#include "Defines.h"
#include "p25/edac/Trellis.h"

#include <string>

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        //      Represents the data header for PDU P25 packets.
        // ---------------------------------------------------------------------------

        class HOST_SW_API DataHeader {
        public:
            /// <summary>Initializes a new instance of the DataHeader class.</summary>
            DataHeader();
            /// <summary>Finalizes a instance of the DataHeader class.</summary>
            ~DataHeader();

            /// <summary>Decodes P25 PDU data header.</summary>
            bool decode(const uint8_t* data);
            /// <summary>Encodes P25 PDU data header.</summary>
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
            /// <summary>Sets the count of block padding.</summary>
            void setPadCount(uint8_t padCount);
            /// <summary>Gets the count of block padding.</summary>
            uint8_t getPadCount() const;

        public:
            /// <summary>Flag indicating if acknowledgement is needed.</summary>
            __PROPERTY(bool, ackNeeded, AckNeeded);
            /// <summary>Flag indicating if this is an outbound data packet.</summary>
            __PROPERTY(bool, outbound, Outbound);
            /// <summary>Data packet format.</summary>
            __PROPERTY(uint8_t, fmt, Format);
            /// <summary>Service access point.</summary>
            __PROPERTY(uint8_t, sap, SAP);
            /// <summary>Manufacturer ID.</summary>
            __PROPERTY(uint8_t, mfId, MFId);
            /// <summary>Logical link ID.</summary>
            __PROPERTY(uint32_t, llId, LLId);
            /// <summary>Flag indicating whether or not this data packet is a full message.</summary>
            /// <remarks>When a response header, this represents the extended flag.</summary>
            __PROPERTY(bool, F, FullMessage);
            /// <summary>Synchronize Flag.</summary>
            __PROPERTY(bool, S, Synchronize);
            /// <summary>Fragment Sequence Number.</summary>
            __PROPERTY(uint8_t, fsn, FSN);
            /// <summary>Send Sequence Number.</summary>
            __PROPERTY(uint8_t, Ns, Ns);
            /// <summary>Flag indicating whether or not this is the last fragment in a message.</summary>
            __PROPERTY(bool, lastFragment, LastFragment);
            /// <summary>Offset of the header.</summary>
            __PROPERTY(uint8_t, headerOffset, HeaderOffset);

            /** Response Data */
            /// <summary>Source Logical link ID.</summary>
            __PROPERTY(uint32_t, srcLlId, SrcLLId);
            /// <summary>Response class.</summary>
            __PROPERTY(uint8_t, rspClass, ResponseClass);
            /// <summary>Response type.</summary>
            __PROPERTY(uint8_t, rspType, ResponseType);
            /// <summary>Response status.</summary>
            __PROPERTY(uint8_t, rspStatus, ResponseStatus);

            /** AMBT Data */
            /// <summary>Alternate Trunking Block Opcode</summary>
            __PROPERTY(uint8_t, ambtOpcode, AMBTOpcode);
            /// <summary>Alternate Trunking Block Field 8</summary>
            __PROPERTY(uint8_t, ambtField8, AMBTField8);
            /// <summary>Alternate Trunking Block Field 9</summary>
            __PROPERTY(uint8_t, ambtField9, AMBTField9);

        private:
            edac::Trellis m_trellis;

            uint8_t m_blocksToFollow;
            uint8_t m_padCount;
            uint32_t m_dataOctets;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__DATA_HEADER_H__
