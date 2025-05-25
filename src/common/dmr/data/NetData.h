// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file NetData.h
 * @ingroup dmr
 * @file NetData.cpp
 * @ingroup dmr
 */
#if !defined(__DMR_DATA__NET_DATA_H__)
#define __DMR_DATA__NET_DATA_H__

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
        class HOST_SW_API NetData {
        public:
            /**
             * @brief Initializes a new instance of the NetData class.
             * @param data Instance of NetData class to copy from.
             */
            NetData(const NetData& data);
            /**
             * @brief Initializes a new instance of the NetData class.
             */
            NetData();
            /**
             * @brief Finalizes a instance of the NetData class.
             */
            ~NetData();

            /**
             * @brief Equals operator.
             * @param data Instance of NetData class to copy from.
             */
            NetData& operator=(const NetData& data);

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
            DECLARE_PROPERTY(uint32_t, slotNo, SlotNo);

            /**
             * @brief Source ID.
             */
            DECLARE_PROPERTY(uint32_t, srcId, SrcId);
            /**
             * @brief Destination ID.
             */
            DECLARE_PROPERTY(uint32_t, dstId, DstId);

            /**
             * @brief Sets the full-link control opcode.
             */
            DECLARE_PROPERTY(defines::FLCO::E, flco, FLCO);

            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint8_t, control, Control);

            /**
             * @brief 
             */
            DECLARE_PROPERTY(uint8_t, n, N);

            /**
             * @brief Sequence number.
             */
            DECLARE_PROPERTY(uint8_t, seqNo, SeqNo);

            /**
             * @brief Embedded data type.
             */
            DECLARE_PROPERTY(defines::DataType::E, dataType, DataType);

            /**
             * @brief Bit Error Rate.
             */
            DECLARE_PROPERTY(uint8_t, ber, BER);

            /**
             * @brief Received Signal Strength Indicator.
             */
            DECLARE_PROPERTY(uint8_t, rssi, RSSI);

        private:
            uint8_t* m_data;
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__NET_DATA_H__
