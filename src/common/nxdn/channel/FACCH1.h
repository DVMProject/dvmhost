// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018 Jonathan Naylor, G4KLX
 *
 */
/**
 * @file FACCH1.h
 * @ingroup nxdn_ch
 * @file FACCH1.cpp
 * @ingroup nxdn_ch
 */
#if !defined(__NXDN_CHANNEL__FACCH1_H__)
#define  __NXDN_CHANNEL__FACCH1_H__

#include "common/Defines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements NXDN Fast Associated Control Channel 1.
         * @ingroup nxdn_ch
         */
        class HOST_SW_API FACCH1 {
        public:
            /**
             * @brief Initializes a new instance of the FACCH1 class.
             */
            FACCH1();
            /**
             * @brief Initializes a copy instance of the FACCH1 class.
             * @param data Instance of FACCH1 to copy.
             */
            FACCH1(const FACCH1& data);
            /**
             * @brief Finalizes a instance of the FACCH1 class.
             */
            ~FACCH1();

            /**
             * @brief Equals operator.
             * @param data Instance of FACCH1 to copy.
             */
            FACCH1& operator=(const FACCH1& data);

            /**
             * @brief Decode a fast associated control channel 1.
             * @param[in] data Buffer containing FACCH to decode.
             * @param offset 
             * @returns bool True, if FACCH decoded, otherwise false.
             */
            bool decode(const uint8_t* data, uint32_t offset);
            /**
             * @brief Encode a fast associated control channel 1.
             * @param[out] data Buffer to encode FACCH.
             * @param offset 
             */
            void encode(uint8_t* data, uint32_t offset) const;

            /**
             * @brief Gets the raw FACCH1 data.
             * @param[out] data Buffer to copy raw FACCH data to.
             */
            void getData(uint8_t* data) const;
            /**
             * @brief Sets the raw FACCH1 data.
             * @param[in] data Buffer to copy raw FACCH data from.
             */
            void setData(const uint8_t* data);

        private:
            uint8_t* m_data;

            /**
             * @brief Internal helper to copy the class.
             * @param data Instance of FACCH1 to copy.
             */
            void copy(const FACCH1& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__FACCH1_H__
