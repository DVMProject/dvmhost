// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2016 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file LowSpeedData.h
 * @ingroup p25
 * @file LowSpeedData.cpp
 * @ingroup p25
 */
#if !defined(__P25_DATA__LOW_SPEED_DATA_H__)
#define __P25_DATA__LOW_SPEED_DATA_H__

#include "common/Defines.h"

namespace p25
{
    namespace data
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Represents embedded low speed data in P25 LDUs.
         * @ingroup p25
         */
        class HOST_SW_API LowSpeedData {
        public:
            /**
             * @brief Initializes a new instance of the LowSpeedData class.
             */
            LowSpeedData();
            /**
             * @brief Finalizes a new instance of the LowSpeedData class.
             */
            ~LowSpeedData();

            /**
             * @brief Equals operator.
             * @param data Instance of LowSpeedData class to copy from.
             */
            LowSpeedData& operator=(const LowSpeedData& data);

            /**
             * @brief Decodes embedded low speed data.
             * @param[in] data Buffer containing low speed data to proces.
             */
            void process(uint8_t* data);
            /**
             * @brief Encode embedded low speed data.
             * @param[out] data Buffer to encode low speed data. 
             */
            void encode(uint8_t* data) const;

        public:
            /**
             * @brief Low speed data 1 value.
             */
            DECLARE_PROPERTY(uint8_t, lsd1, LSD1);
            /**
             * @brief Low speed data 2 value.
             */
            DECLARE_PROPERTY(uint8_t, lsd2, LSD2);

        private:
            /**
             * @brief 
             * @param in 
             */
            uint8_t encode(const uint8_t in) const;
        };
    } // namespace data
} // namespace p25

#endif // __P25_DATA__LOW_SPEED_DATA_H__
