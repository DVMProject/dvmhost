// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2018,2022 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__P25_DATA__DATA_HEADER_H__)
#define  __P25_DATA__DATA_HEADER_H__

#include "common/Defines.h"
#include "common/edac/Trellis.h"

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
            bool decode(const uint8_t* data, bool noTrellis = false);
            /// <summary>Encodes P25 PDU data header.</summary>
            void encode(uint8_t* data, bool noTrellis = false);

            /// <summary>Helper to reset data values to defaults.</summary>
            void reset();

            /// <summary>Gets the total length in bytes of enclosed packet data.</summary>
            uint32_t getPacketLength() const;

            /// <summary>Gets the raw header data.</summary>
            uint32_t getData(uint8_t* buffer) const;

            /// <summary>Sets the flag indicating CRC-errors should be warnings and not errors.</summary>
            static void setWarnCRC(bool warnCRC) { m_warnCRC = warnCRC; }

            /// <summary>Helper to determine the pad length for a given packet length.</summary>
            static uint32_t calculatePadLength(uint8_t fmt, uint32_t packetLength);

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
            /// <summary>Total number of blocks following this header.</summary>
            __PROPERTY(uint8_t, blocksToFollow, BlocksToFollow);
            /// <summary>Total number of padding bytes.</summary>
            __PROPERTY(uint8_t, padLength, PadLength);
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

            uint8_t* m_data;
        
            static bool m_warnCRC;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__DATA_HEADER_H__
