// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018,2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup p25_pdu Packet Data Unit
 * @brief Implementation for the data handling of the TIA-102.BAAA Project 25 standard.
 * @ingroup p25
 * 
 * @file DataHeader.h
 * @ingroup p25_pdu
 * @file DataHeader.cpp
 * @ingroup p25_pdu
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
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents the data header for PDU P25 packets.
         * @ingroup p25_pdu
         */
        class HOST_SW_API DataHeader {
        public:
            /**
             * @brief Initializes a new instance of the DataHeader class.
             */
            DataHeader();
            /**
             * @brief Finalizes a instance of the DataHeader class.
             */
            ~DataHeader();

            /**
             * @brief Decodes P25 PDU data header.
             * @param[in] data Buffer containing a PDU data header to decode.
             * @param noTrellis Flag indicating not to perform Trellis decoding.
             * @returns bool True, if PDU data header decoded, otherwise false.
             */
            bool decode(const uint8_t* data, bool noTrellis = false);
            /**
             * @brief Encodes P25 PDU data header.
             * @param[out] data Buffer to encode a PDU data header.
             * @param noTrellis Flag indicating not to perform Trellis encoding.
             */
            void encode(uint8_t* data, bool noTrellis = false);

            /**
             * @brief Helper to reset data values to defaults.
             */
            void reset();

            /**
             * @brief Gets the total length in bytes of enclosed packet data.
             * @returns uint32_t Total length of packet in bytes.
             */
            uint32_t getPacketLength() const;

            /**
             * @brief Gets the raw header data.
             * @param[out] buffer Buffer to copy raw header data to.
             * @returns uint32_t Length of data copied.
             */
            uint32_t getData(uint8_t* buffer) const;

            /**
             * @brief Sets the flag indicating CRC-errors should be warnings and not errors.
             * @param warnCRC Flag indicating CRC-errors should be treated as warnings.
             */
            static void setWarnCRC(bool warnCRC) { m_warnCRC = warnCRC; }

            /**
             * @brief Helper to determine the pad length for a given packet length.
             * @param fmt PDU format type.
             * @param packetLength Length of PDU.
             * @returns uint32_t Number of pad bytes.
             */
            static uint32_t calculatePadLength(uint8_t fmt, uint32_t packetLength);

        public:
            /**
             * @brief Flag indicating if acknowledgement is needed.
             */
            __PROPERTY(bool, ackNeeded, AckNeeded);
            /**
             * @brief Flag indicating if this is an outbound data packet.
             */
            __PROPERTY(bool, outbound, Outbound);
            /**
             * @brief Data packet format.
             */
            __PROPERTY(uint8_t, fmt, Format);
            /**
             * @brief Service access point.
             */
            __PROPERTY(uint8_t, sap, SAP);
            /**
             * @brief Manufacturer ID.
             */
            __PROPERTY(uint8_t, mfId, MFId);
            /**
             * @brief Logical link ID.
             */
            __PROPERTY(uint32_t, llId, LLId);
            /**
             * @brief Total number of blocks following this header.
             */
            __PROPERTY(uint8_t, blocksToFollow, BlocksToFollow);
            /**
             * @brief Total number of padding bytes.
             */
            __PROPERTY(uint8_t, padLength, PadLength);
            /**
             * @brief Flag indicating whether or not this data packet is a full message.
             */
            __PROPERTY(bool, F, FullMessage);
            /**
             * @brief Synchronize Flag.
             */
            __PROPERTY(bool, S, Synchronize);
            /**
             * @brief Fragment Sequence Number.
             */
            __PROPERTY(uint8_t, fsn, FSN);
            /**
             * @brief Send Sequence Number.
             */
            __PROPERTY(uint8_t, Ns, Ns);
            /**
             * @brief Flag indicating whether or not this is the last fragment in a message.
             */
            __PROPERTY(bool, lastFragment, LastFragment);
            /**
             * @brief Offset of the header.
             */
            __PROPERTY(uint8_t, headerOffset, HeaderOffset);

            // Response Data
            /**
             * @brief Source Logical link ID.
             */
            __PROPERTY(uint32_t, srcLlId, SrcLLId);
            /**
             * @brief Response class.
             */
            __PROPERTY(uint8_t, rspClass, ResponseClass);
            /**
             * @brief Response type.
             */
            __PROPERTY(uint8_t, rspType, ResponseType);
            /**
             * @brief Response status.
             */
            __PROPERTY(uint8_t, rspStatus, ResponseStatus);

            // AMBT Data
            /**
             * @brief Alternate Trunking Block Opcode
             */
            __PROPERTY(uint8_t, ambtOpcode, AMBTOpcode);
            /**
             * @brief Alternate Trunking Block Field 8
             */
            __PROPERTY(uint8_t, ambtField8, AMBTField8);
            /**
             * @brief Alternate Trunking Block Field 9
             */
            __PROPERTY(uint8_t, ambtField9, AMBTField9);

        private:
            edac::Trellis m_trellis;

            uint8_t* m_data;
        
            static bool m_warnCRC;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__DATA_HEADER_H__
