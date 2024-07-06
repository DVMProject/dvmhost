// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2021 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DataHeader.h
 * @ingroup dmr
 * @file DataHeader.cpp
 * @ingroup dmr
 */
#if !defined(__DMR_DATA__DATA_HEADER_H__)
#define __DMR_DATA__DATA_HEADER_H__

#include "common/Defines.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a DMR data header.
         * @ingroup dmr
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
            void encode(uint8_t* data) const;

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
             * @brief Count of block padding.
             */
            __PROPERTY(uint8_t, padCount, PadCount);

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
             * @brief Gets the number of data blocks following the header.
             */
            __PROPERTY(uint32_t, blocks, Blocks);

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
            uint8_t* m_data;
            bool m_SF;
            bool m_PF;
            uint8_t m_UDTO;
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__DATA_HEADER_H__
