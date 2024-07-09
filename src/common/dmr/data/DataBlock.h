// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file DataBlock.h
 * @ingroup dmr_pdu
 * @file DataBlock.cpp
 * @ingroup dmr_pdu
 */
#if !defined(__DMR_DATA__DATA_BLOCK_H__)
#define  __DMR_DATA__DATA_BLOCK_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/data/DataHeader.h"
#include "common/edac/BPTC19696.h"
#include "common/edac/Trellis.h"

#include <string>

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a data block for PDU DMR packets.
         * @ingroup dmr_pdu
         */
        class HOST_SW_API DataBlock {
        public:
            /**
             * @brief Initializes a new instance of the DataBlock class.
             */
            DataBlock();
            /**
             * @brief Finalizes a instance of the DataBlock class.
             */
            ~DataBlock();

            /**
             * @brief Decodes DMR PDU data block.
             * @param[in] data Buffer containing a PDU data block to decode.
             * @param header DMR PDU data header.
             * @returns bool True, if PDU data block decoded, otherwise false.
             */
            bool decode(const uint8_t* data, const DataHeader& header);
            /**
             * @brief Encodes a DMR PDU data block.
             * @param[out] data Buffer to encode a PDU data block.
             */
            void encode(uint8_t* data);

            /**
             * @brief Sets the data format from the data header.
             * @param dataType DMR data type.
             */
            void setDataType(const defines::DataType::E& dataType);
            /**
             * @brief Gets the data type.
             * @returns defines::DataType::E DMR data type.
             */
            defines::DataType::E getDataType() const;
            /**
             * @brief Sets the data format.
             * @param fmt PDU data foramt.
             */
            void setFormat(const defines::DPF::E fmt);
            /**
             * @brief Sets the data format from the data header.
             * @param header P25 PDU data header.
             */
            void setFormat(const DataHeader& header);
            /**
             * @brief Gets the data format.
             * @returns defines::DPF::E PDU data format.
             */
            defines::DPF::E getFormat() const;

            /**
             * @brief Sets the raw data stored in the data block.
             * @param[in] buffer Buffer containing bytes to store in data block.
             */
            void setData(const uint8_t* buffer);
            /**
             * @brief Gets the raw data stored in the data block.
             * @param[out] buffer Buffer to copy bytes in data block to.
             * @returns uint32_t Number of bytes copied.
             */
            uint32_t getData(uint8_t* buffer) const;

        public:
            /**
             * @brief Sets the data block serial number.
             */
            __PROPERTY(uint8_t, serialNo, SerialNo);

            /**
             * @brief Flag indicating this is the last block in a sequence of block.
             */
            __PROPERTY(bool, lastBlock, LastBlock);

        private:
            edac::BPTC19696 m_bptc;
            edac::Trellis m_trellis;

            defines::DataType::E m_dataType;
            defines::DPF::E m_DPF;

            uint8_t* m_data;
        };
    } // namespace data
} // namespace dmr

#endif // __P25_DATA__DATA_BLOCK_H__
