// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file UDCH.h
 * @ingroup nxdn_ch
 * @file UDCH.cpp
 * @ingroup nxdn_ch
 */
#if !defined(__NXDN_CHANNEL__UDCH_H__)
#define  __NXDN_CHANNEL__UDCH_H__

#include "common/Defines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements NXDN User Data Channel / Fast Access Control Channel 2.
         * @ingroup nxdn_ch
         */
        class HOST_SW_API UDCH {
        public:
            /**
             * @brief Initializes a new instance of the UDCH class.
             */
            UDCH();
            /**
             * @brief Initializes a copy instance of the UDCH class.
             * @param data Instance of UDCH to copy.
             */
            UDCH(const UDCH& data);
            /**
             * @brief Finalizes a instance of the UDCH class.
             */
            ~UDCH();

            /**
             * @brief Equals operator.
             * @param data Instance of UDCH to copy.
              */
            UDCH& operator=(const UDCH& data);

            /**
             * @brief Decode a user data channel.
             * @param[in] data Buffer containing UDCH to decode.
             */
            bool decode(const uint8_t* data);
            /**
             * @brief Encode a user data channel.
             * @param[out] data Buffer to encode UDCH.
             */
            void encode(uint8_t* data) const;

            /**
             * @brief Gets the raw UDCH data.
             * @param[out] data Buffer to copy raw UDCH data to.
             */
            void getData(uint8_t* data) const;
            /**
             * @brief Sets the raw UDCH data.
             * @param[in] data Buffer to copy raw UDCH data from.
             */
            void setData(const uint8_t* data);

        public:
            // Common Data
            /**
             * @brief Radio Access Number
             */
            DECLARE_PROPERTY(uint8_t, ran, RAN);

        private:
            uint8_t* m_data;

            /**
             * @brief Internal helper to copy the class.
             * @param data Instance of UDCH to copy.
              */
            void copy(const UDCH& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__UDCH_H__
