// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2015,2016,2017 Jonathan Naylor, G4KLX
 *  Copyright (C) 2024 Bryan Biedenkap, N2PLL
 *
 */
/**
 * @file EmbeddedData.h
 * @ingroup dmr
 * @file EmbeddedData.cpp
 * @ingroup dmr
 */
#if !defined(__DMR_DATA__EMBEDDED_DATA_H__)
#define __DMR_DATA__EMBEDDED_DATA_H__

#include "common/Defines.h"
#include "common/dmr/DMRDefines.h"
#include "common/dmr/lc/LC.h"

namespace dmr
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Constants
        // ---------------------------------------------------------------------------

        enum LC_STATE {
            LCS_NONE,
            LCS_FIRST,
            LCS_SECOND,
            LCS_THIRD
        };

        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents a DMR embedded data.
         * @ingroup dmr
         */
        class HOST_SW_API EmbeddedData {
        public:
            /**
             * @brief Initializes a new instance of the EmbeddedData class.
             */
            EmbeddedData();
            /**
             * @brief Finalizes a instance of the EmbeddedData class.
             */
            ~EmbeddedData();

            /**
             * @brief Add LC data (which may consist of 4 blocks) to the data store.
             * @param[in] data 
             * @param lcss
             * @returns bool True, if LC data is decoded, otherwise false.
             */
            bool addData(const uint8_t* data, uint8_t lcss);
            /**
             * @brief Get LC data from the data store.
             * @param[out] data
             * @param n 
             * @returns uint8_t 
             */
            uint8_t getData(uint8_t* data, uint8_t n) const;

            /**
             * @brief Sets link control data.
             * @param lc Instance of the LC class.
             */
            void setLC(const lc::LC& lc);
            /**
             * @brief Gets link control data.
             * @returns lc::LC* LC class.
             */
            std::unique_ptr<lc::LC> getLC() const;

            /**
             * @brief Get raw embedded data buffer.
             * @param[in] data 
             */
            bool getRawData(uint8_t* data) const;

            /**
             * @brief Helper to reset data values to defaults.
             */
            void reset();

        public:
            /**
             * @brief Flag indicating whether or not the embedded data is valid.
             */
            __READONLY_PROPERTY_PLAIN(bool, valid);
            /**
             * @brief Full-link control opcode.
             */
            __READONLY_PROPERTY(defines::FLCO::E, FLCO, FLCO);

        private:
            LC_STATE m_state;
            bool* m_data;

            bool* m_raw;

            /**
             * @brief Unpack and error check an embedded LC.
             */
            void decodeEmbeddedData();
            /**
             * @brief Pack and FEC for an embedded LC.
             */
            void encodeEmbeddedData();
        };
    } // namespace data
} // namespace dmr

#endif // __DMR_DATA__EMBEDDED_DATA_H__
