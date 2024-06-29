// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file Data.h
 * @ingroup dmr
 * @file Data.cpp
 * @ingroup dmr
 */
#if !defined(__DMR_DATA__DATA_H__)
#define __DMR_DATA__DATA_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents network DMR data.
         * @ingroup dmr
         */
        class HOST_SW_API Data {
        public:
            /**
             * @brief Initializes a new instance of the Data class.
             * @param data Instance of Data class to copy from.
             */
            Data(const Data& data);
            /**
             * @brief Initializes a new instance of the Data class.
             */
            Data();
            /**
             * @brief Finalizes a instance of the Data class.
             */
            ~Data();

            /**
             * @brief Equals operator.
             * @param data Instance of Data class to copy from.
             */
            Data& operator=(const Data& data);

            /**
             * @brief Sets raw data.
             * @param[in] buffer Raw data buffer.
             */
            void setData(const uint8_t* buffer);
            /**
             * @brief Gets raw data.
             * @param[out] buffer Raw data buffer.
             */
            uint32_t getData(uint8_t* buffer) const;

        public:
            /**
             * @brief DMR slot number.
             */
            __PROPERTY(uint32_t, slotNo, SlotNo);

            /**
             * @brief Source ID.
             */
            __PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            __PROPERTY(uint32_t, dstId, DstId);

            /**
             * @brief Sets the full-link control opcode.
             */
            __PROPERTY(defines::FLCO::E, flco, FLCO);

            /**
             * @brief 
             */
            __PROPERTY(uint8_t, n, N);

            /**
             * @brief Sequence number.
             */
            __PROPERTY(uint8_t, seqNo, SeqNo);

            /**
             * @brief Embedded data type.
             */
            __PROPERTY(defines::DataType::E, dataType, DataType);

            /**
             * @brief Bit Error Rate.
             */
            __PROPERTY(uint8_t, ber, BER);

            /**
             * @brief Received Signal Strength Indicator.
             */
            __PROPERTY(uint8_t, rssi, RSSI);

        private:
            uint8_t* m_data;
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__DATA_H__
