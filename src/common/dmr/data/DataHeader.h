// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2021,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @defgroup dmr_pdu Packet Data Unit
 * @brief Implementation for the data handling of ETSI TS-102 packet data.
 * @ingroup dmr
 * 
 * @file DataHeader.h
 * @ingroup dmr_pdu
 * @file DataHeader.cpp
 * @ingroup dmr_pdu
 */
#if !defined(__DMR_DATA__DATA_HEADER_H__)
#define __DMR_DATA__DATA_HEADER_H__

#include "common/Defines.h"
#include "common/edac/BPTC19696.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents the data header for PDU DMR packets.
         * @ingroup dmr_pdu
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
             * @brief Equals operator.
             * @param header Instance of DataHeader class to copy from.
             */
            DataHeader& operator=(const DataHeader& header);

            /**
             * @brief Decodes a DMR data header.
             * @param[in] data Buffer containing DMR data header.
             * @returns bool True, if DMR data header is decoded, otherwise false.
             */
            bool decode(const uint8_t* data);
            /**
             * @brief Encodes a DMR data header.
             * @param[out] data Buffer to encode a DMR data header.
             */
            void encode(uint8_t* data);

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
             * @brief Helper to calculate the number of blocks to follow and padding length for a PDU.
             * @param packetLength Length of PDU.
             */
            void calculateLength(uint32_t packetLength);

            /**
             * @brief Helper to determine the pad length for a given packet length.
             * @param dpf PDU format type.
             * @param packetLength Length of PDU.
             * @returns uint32_t Number of pad bytes.
             */
            static uint32_t calculatePadLength(defines::DPF::E dpf, uint32_t packetLength);

        public:
            /**
             * @brief Flag indicating whether the data header is group or individual.
             */
            __PROPERTY(bool, GI, GI);
            /**
             * @brief Flag indicating whether the data header requires acknowledgement.
             */
            __PROPERTY(bool, A, A);

            /**
             * @brief Data packet format.
             */
            __PROPERTY(defines::DPF::E, DPF, DPF);

            /**
             * @brief Service access point.
             */
            __PROPERTY(uint8_t, sap, SAP);
            /**
             * @brief Fragment Sequence Number.
             */
            __PROPERTY(uint8_t, fsn, FSN);
            /**
             * @brief Send Sequence Number.
             */
            __PROPERTY(uint8_t, Ns, Ns);

            /**
             * @brief Gets the number of data blocks following the header.
             */
            __PROPERTY(uint32_t, blocksToFollow, BlocksToFollow);
            /**
             * @brief Count of block padding.
             */
            __PROPERTY(uint8_t, padLength, PadLength);
            /**
             * @brief Full Message Flag.
             */
            __PROPERTY(bool, F, FullMesage);
            /**
             * @brief Synchronize Flag.
             */
            __PROPERTY(bool, S, Synchronize);

            /**
             * @brief Unified Data or Defined Data Format.
             */
            __PROPERTY(uint8_t, dataFormat, DataFormat);

            /**
             * @brief Source ID.
             */
            __PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            __PROPERTY(uint32_t, dstId, DstId);

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

            /**
             * @brief Source Port.
             */
            __PROPERTY(uint8_t, srcPort, SrcPort);
            /**
             * @brief Destination Port.
             */
            __PROPERTY(uint8_t, dstPort, DstPort);

        private:
            edac::BPTC19696 m_bptc;

            uint8_t* m_data;
            bool m_SF;
            bool m_PF;
            uint8_t m_UDTO;
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__DATA_HEADER_H__
