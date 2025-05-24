// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2018 Jonathan Naylor, G4KLX
 *  Copyright (C) 2022,2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file SACCH.h
 * @ingroup nxdn_ch
 * @file SACCH.cpp
 * @ingroup nxdn_ch
 */
#if !defined(__NXDN_CHANNEL__SACCH_H__)
#define  __NXDN_CHANNEL__SACCH_H__

#include "common/Defines.h"
#include "common/nxdn/NXDNDefines.h"

namespace nxdn
{
    namespace channel
    {
        // ---------------------------------------------------------------------------
        //  Class Declaration
        // ---------------------------------------------------------------------------

        /**
         * @brief Implements NXDN Slow Associated Control Channel.
         * @ingroup nxdn_ch
         */
        class HOST_SW_API SACCH {
        public:
            /**
             * @brief Initializes a new instance of the SACCH class.
             */
            SACCH();
            /**
             * @brief Initializes a copy instance of the SACCH class.
             * @param data Instance of SACCH to copy.
             */
            SACCH(const SACCH& data);
            /**
             * @brief Finalizes a instance of the SACCH class.
             */
            ~SACCH();

            /**
             * @brief Equals operator.
             * @param data Instance of SACCH to copy.
             */
            SACCH& operator=(const SACCH& data);

            /**
             * @brief Decode a slow associated control channel.
             * @param[in] data Buffer containing SACCH to decode.
             * @returns bool True, if SACCH decoded, otherwise false.
             */
            bool decode(const uint8_t* data);
            /**
             * @brief Encode a slow associated control channel.
             * @param[out] data Buffer to encode SACCH.
             */
            void encode(uint8_t* data) const;

            /**
             * @brief Gets the raw SACCH data.
             * @param[out] data Buffer to copy raw SACCH data to.
             */
            void getData(uint8_t* data) const;
            /**
             * @brief Sets the raw SACCH data.
             * @param[in] data Buffer to copy raw SACCH data from.
             */
            void setData(const uint8_t* data);

        public:
            // Common Data
            /**
             * @brief Radio Access Number
             */
            DECLARE_PROPERTY(uint8_t, ran, RAN);
            /**
             * @brief Channel Structure
             */
            DECLARE_PROPERTY(defines::ChStructure::E, structure, Structure);

        private:
            uint8_t* m_data;

            /**
             * @brief Internal helper to copy the class.
             * @param data Instance of SACCH to copy.
             */
            void copy(const SACCH& data);
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__SACCH_H__
