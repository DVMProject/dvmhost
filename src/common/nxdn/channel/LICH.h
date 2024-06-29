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
 * @defgroup nxdn_ch NXDN Channel Types
 * @brief Implementation for the NXDN in-band data channel processing.
 * @ingroup nxdn
 * 
 * @file LICH.h
 * @ingroup nxdn_ch
 * @file LICH.cpp
 * @ingroup nxdn_ch
 */
#if !defined(__NXDN_CHANNEL__LICH_H__)
#define  __NXDN_CHANNEL__LICH_H__

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
         * @brief Implements NXDN Link Information Channel.
         * @ingroup nxdn_ch
         */
        class HOST_SW_API LICH {
        public:
            /**
             * @brief Initializes a new instance of the LICH class.
             */
            LICH();
            /**
             * @brief Initializes a copy instance of the LICH class.
             * @param data Instance of LICH to copy.
             */
            LICH(const LICH& lich);
            /**
             * @brief Finalizes a instance of the LICH class.
             */
            ~LICH();

            /**
             * @brief Equals operator.
             * @param data Instance of LICH to copy.
             */
            LICH& operator=(const LICH& lich);

            /**
             * @brief Decode a link information channel.
             * @param[in] data Buffer containing LICH to decode.
             * @returns bool True, if LICH decoded, otherwise false.
             */
            bool decode(const uint8_t* data);
            /**
             * @brief Encode a link information channel.
             * @param[out] data Buffer to encode LICH.
             */
            void encode(uint8_t* data);

        public:
            // Common Data
            /**
             * @brief RF Channel Type
             */
            __PROPERTY(defines::RFChannelType::E, rfct, RFCT);
            /**
             * @brief Functional Channel Type
             */
            __PROPERTY(defines::FuncChannelType::E, fct, FCT);
            /**
             * @brief Channel Options
             */
            __PROPERTY(defines::ChOption::E, option, Option);
            /**
             * @brief Flag indicating outbound traffic direction
             */
            __PROPERTY(bool, outbound, Outbound);

        private:
            uint8_t m_lich;

            /**
             * @brief Internal helper to copy the class.
             * @param data Instance of LICH to copy.
             */
            void copy(const LICH& data);

            /**
             * @brief Internal helper to generate the parity bit for the LICH.
             * @returns bool Boolean flag based on whether the parity bit is set or not.
             */
            bool getParity() const;
        };
    } // namespace channel
} // namespace nxdn

#endif // __NXDN_CHANNEL__LICH_H__
